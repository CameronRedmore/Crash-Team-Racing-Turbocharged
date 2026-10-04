/*
 * GPU state: the cached blend/depth/stencil/scissor/viewport setters the PSX
 * submit run drives, draw-environment projection and clipping, texture and
 * shader selection, the isolated-depth overlay pass, and the save/restore that
 * wraps a full-screen utility pass.
 *
 * The caches behind the setters live here. Invalidation goes through the
 * Invalidate and Reset entry points rather than a direct write, with one
 * exception: LoadRenderTargetFromVRAM in native_renderer_targets.c snapshots
 * and restores the whole set itself, because its pass leaves the target
 * framebuffer bound and writes depth with depth-write on, so it cannot use
 * BeginUtilityPass/EndUtilityPass. That is why these fields are still declared
 * in the internal header rather than kept private to this file.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>

#include "platform/native_draw3d.h"
#include "platform/native_gpu.h"
#include "platform/native_log.h"
#include "platform/native_options.h"
#include "platform/native_renderer_internal.h"

// Last scissor and stencil state pushed to GL. Only the cached setters here
// read or write them; other modules go through the setters.
global_variable int s_previousStencilMode = 0;
global_variable int s_previousScissorRectValid = 0;
global_variable int s_previousScissorX = 0;
global_variable int s_previousScissorY = 0;
global_variable int s_previousScissorW = 0;
global_variable int s_previousScissorH = 0;

// Uniform locations of the currently bound PSX shader. SetShader writes them
// when a shader binds and the setters below read them; no other module does
// either. They were plain globals in the single-file renderer, and are static
// here because nothing outside this file ever referenced them.
global_variable GLint u_projectionLoc;
global_variable GLint u_bilinearFilterLoc;
global_variable GLint u_texelSizeLoc;
#ifndef __vita__
global_variable GLint u_psxSemiTransPassLoc;
global_variable GLint u_psxDitherEnabledLoc;
global_variable GLint u_psxColorDepth15Loc;
#endif
global_variable GLint u_psxDrawMaskSetLoc;
global_variable GLint u_psxTextureOutputStpLoc;

internal void NativeRenderer_InvalidateScissorRectCache(void)
{
	s_previousScissorRectValid = 0;
}

internal void NativeRenderer_SetScissorRectCached(int x, int y, int width, int height)
{
	if (s_previousScissorRectValid && s_previousScissorX == x && s_previousScissorY == y && s_previousScissorW == width && s_previousScissorH == height)
	{
		return;
	}

	glScissor(x, y, width, height);
	s_previousScissorRectValid = 1;
	s_previousScissorX = x;
	s_previousScissorY = y;
	s_previousScissorW = width;
	s_previousScissorH = height;
}

internal void NativeRenderer_Ortho2D(float left, float right, float bottom, float top, float znear, float zfar)
{
	float a = 2.0f / (right - left);
	float b = 2.0f / (top - bottom);
	float c = 2.0f / (znear - zfar);

	float x = (left + right) / (left - right);
	float y = (bottom + top) / (bottom - top);

	// -1..1
	float z = (znear + zfar) / (znear - zfar);

	float ortho[16] = {a, 0, 0, 0, 0, b, 0, 0, 0, 0, c, 0, x, y, z, 1};

	glUniformMatrix4fv(u_projectionLoc, 1, GL_FALSE, ortho);
}

void NativeRenderer_SetupClipMode(const RECT16 *rect, const DISPENV *displayEnv, int enable)
{
	if ((displayEnv->disp.w <= 0) || (displayEnv->disp.h <= 0))
	{
		NativeRenderer_SetScissorState(0);
		return;
	}

	if ((rect->w <= 0) || (rect->h <= 0))
	{
		// NOTE(aalhendi): Retail draw-area commands define inclusive corners.
		// Collapsed areas clip all pixels; GL scissor rejects negative sizes.
		NativeRenderer_SetScissorState(enable != 0);
		if (enable)
		{
			NativeRenderer_SetScissorRectCached(0, 0, 0, 0);
		}
		return;
	}

	// [A] isinterlaced dirty hack for widescreen
	const bool scissorOn = enable && (displayEnv->isinter || (rect->x - displayEnv->disp.x > 0 || rect->y - displayEnv->disp.y > 0 ||
	                                                          rect->w < displayEnv->disp.w || rect->h < displayEnv->disp.h));

	NativeRenderer_SetScissorState(scissorOn);

	if (!scissorOn)
	{
		return;
	}

	const int displayX = displayEnv->disp.x;
	const int displayY = displayEnv->disp.y;
	const int displayW = displayEnv->disp.w;
	const int displayH = displayEnv->disp.h;
	const int clipRight = rect->x + rect->w;
	const int clipBottom = rect->y + rect->h;
	const int displayRight = displayX + displayW;
	const int displayBottom = displayY + displayH;
	const int overlapX = rect->x > displayX ? rect->x : displayX;
	const int overlapY = rect->y > displayY ? rect->y : displayY;
	const int overlapRight = clipRight < displayRight ? clipRight : displayRight;
	const int overlapBottom = clipBottom < displayBottom ? clipBottom : displayBottom;

	if ((overlapRight <= overlapX) || (overlapBottom <= overlapY))
	{
		NativeRenderer_SetScissorRectCached(0, 0, 0, 0);
		return;
	}

	const int physicalW = s_mainRenderTarget.width > 0 ? s_mainRenderTarget.width : displayW;
	const int physicalH = s_mainRenderTarget.height > 0 ? s_mainRenderTarget.height : displayH;
	const int relLeft = overlapX - displayX;
	const int relRight = overlapRight - displayX;
	const int relTop = overlapY - displayY;
	const int relBottom = overlapBottom - displayY;
	const int scissorX = relLeft * physicalW / displayW;
	const int scissorRight = (relRight * physicalW + displayW - 1) / displayW;
	const int scissorY = (displayH - relBottom) * physicalH / displayH;
	const int scissorTop = ((displayH - relTop) * physicalH + displayH - 1) / displayH;

	NativeRenderer_SetScissorRectCached(scissorX, scissorY, scissorRight - scissorX, scissorTop - scissorY);
}

// The caches above remember the last value each setter pushed to GL. Anything
// that changes GL state behind a setter's back -- destroying a texture,
// rebinding a framebuffer, compiling or deleting a shader -- has to drop the
// memory, or the next draw compares against a stale value and skips a GL call
// it does need. Other modules drop it through these rather than writing the
// caches themselves.
internal void NativeRenderer_InvalidateTextureBinding(void)
{
	s_lastBoundTexture = (TextureID)-1;
}

internal void NativeRenderer_InvalidateTextureBindingIfCurrent(TextureID texture)
{
	if (s_lastBoundTexture == texture)
	{
		s_lastBoundTexture = (TextureID)-1;
	}
}

internal void NativeRenderer_InvalidateBindingCache(void)
{
	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
}

// A new scene starts with no texture bound at all, which is a known state
// rather an unknown one, so this records GL texture 0 instead of -1.
internal void NativeRenderer_ResetTextureBinding(void)
{
	s_lastBoundTexture = 0;
}

internal void NativeRenderer_SetShader(const ShaderID shader)
{
	if (s_previousShader != shader)
	{
		glUseProgram(shader);

		s_previousShader = shader;
	}
}


void NativeRenderer_SetTexture(TextureID texture, TexFormat texFormat, int semiTransPass, BlendMode blendMode, int textured, int superTurboTint,
                               int textureFullyOpaque, int cachedP4)
{
#ifndef __vita__
	if (texFormat == TF_32_BIT_RGBA && blendMode == BM_STRAIGHT_ALPHA)
		semiTransPass = 4;
#endif
#ifdef __vita__
	(void)superTurboTint;
	int variant;
	if (semiTransPass == 1)
	{
		variant = NATIVE_PSX_SHADER_NON_STP;
	}
	else if (semiTransPass == 2)
	{
		variant = blendMode == BM_AVERAGE             ? NATIVE_PSX_SHADER_STP_AVERAGE
		          : blendMode == BM_ADD_QUATER_SOURCE ? NATIVE_PSX_SHADER_STP_QUARTER
		                                              : NATIVE_PSX_SHADER_STP;
	}
	else if (semiTransPass == 3)
	{
		variant = blendMode == BM_AVERAGE             ? NATIVE_PSX_SHADER_MIXED_AVERAGE
		          : blendMode == BM_ADD_QUATER_SOURCE ? NATIVE_PSX_SHADER_MIXED_QUARTER
		                                              : NATIVE_PSX_SHADER_MIXED_ADD;
	}
	else
	{
		variant = blendMode == BM_AVERAGE             ? NATIVE_PSX_SHADER_OPAQUE_AVERAGE
		          : blendMode == BM_ADD_QUATER_SOURCE ? NATIVE_PSX_SHADER_OPAQUE_QUARTER
		                                              : NATIVE_PSX_SHADER_OPAQUE;
	}
	if (texFormat < TF_4_BIT || texFormat > TF_32_BIT_RGBA)
	{
		return;
	}
	GTEShader *shader;
	if (cachedP4)
	{
		shader = &s_gteCachedP4ShaderVariants[variant];
	}
	else if (!textured)
	{
		shader = &s_gteUntexturedShaderVariants[variant < 3 ? variant : 0];
	}
	else if (textureFullyOpaque && texFormat <= TF_8_BIT)
	{
		const int opaqueVariant = blendMode == BM_AVERAGE             ? NATIVE_PSX_SHADER_OPAQUE_AVERAGE
		                          : blendMode == BM_ADD_QUATER_SOURCE ? NATIVE_PSX_SHADER_OPAQUE_QUARTER
		                                                              : NATIVE_PSX_SHADER_OPAQUE;
		shader = &s_gteFullyOpaqueShaderVariants[texFormat][opaqueVariant];
	}
	else
	{
		shader = &s_gteShaderVariants[texFormat][variant];
	}
#else
	(void)blendMode;
	(void)textured;
	(void)textureFullyOpaque;
	(void)cachedP4;
	GTEShader *shader = NULL;
	switch (texFormat)
	{
	case TF_4_BIT:
		shader = superTurboTint ? &s_gteShader4SuperTurbo : &s_gteShader4;
		break;
	case TF_8_BIT:
		shader = superTurboTint ? &s_gteShader8SuperTurbo : &s_gteShader8;
		break;
	case TF_16_BIT:
		shader = superTurboTint ? &s_gteShader16SuperTurbo : &s_gteShader16;
		break;
	case TF_32_BIT_RGBA:
		shader = &s_gteShader32Rgba;
		break;
	case TF_TEXT_SDF:
		shader = &s_gteShaderTextSdf;
		break;
	}
	if (shader == NULL)
	{
		return;
	}
#endif

	NativeRenderer_SetShader(shader->shader);
	u_bilinearFilterLoc = shader->bilinearFilterLoc;
	u_projectionLoc = shader->projectionLoc;
	u_texelSizeLoc = (texFormat == TF_32_BIT_RGBA || texFormat == TF_TEXT_SDF) ? shader->texelSizeLoc : -1;
#ifndef __vita__
	u_psxSemiTransPassLoc = shader->psxSemiTransPassLoc;
	u_psxDitherEnabledLoc = shader->psxDitherEnabledLoc;
	u_psxColorDepth15Loc = shader->psxColorDepth15Loc;
#endif
	u_psxDrawMaskSetLoc = shader->psxDrawMaskSetLoc;
	u_psxTextureOutputStpLoc = shader->psxTextureOutputStpLoc;

	if (g_dbg_texturelessMode)
	{
		texture = s_whiteTexture;
	}

	// NOTE(penta3): s_texture (unit 0) and s_rgLut (unit 1) sampler bindings are baked
	// into each program at compile time (NativeRenderer_Shader_Compile) and uniform
	// values persist per-program, so re-setting them on every split was redundant GL
	// churn. bilinearFilter stays here because it toggles at runtime (debug key).
	if (u_bilinearFilterLoc >= 0)
	{
		glUniform1i(u_bilinearFilterLoc, g_cfg_bilinearFiltering);
	}
#ifndef __vita__
	if (u_psxSemiTransPassLoc >= 0)
	{
		glUniform1i(u_psxSemiTransPassLoc, semiTransPass);
	}
	if (u_psxDitherEnabledLoc >= 0)
	{
		glUniform1i(u_psxDitherEnabledLoc, gNativeDitheringEnabled != 0);
	}
	if (u_psxColorDepth15Loc >= 0)
	{
		glUniform1i(u_psxColorDepth15Loc, gNativeColorDepth == NATIVE_COLOR_DEPTH_15BIT);
	}
#endif

	if (s_lastBoundTexture == texture)
	{
		return;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	s_lastBoundTexture = texture;
}

void NativeRenderer_SetOverrideTextureSize(int width, int height)
{
	if (u_texelSizeLoc == -1)
	{
		return;
	}

	float vec[] = {1.0f / (float)width, 1.0f / (float)height};
	glUniform2fv(u_texelSizeLoc, 1, vec);
}

void NativeRenderer_SetPSXTextureOutputSTP(int enabled)
{
	if (u_psxTextureOutputStpLoc >= 0)
	{
		glUniform1f(u_psxTextureOutputStpLoc, enabled ? 1.0f : 0.0f);
	}
}

void NativeRenderer_SetPSXDrawMaskSet(int maskSet)
{
	if (u_psxDrawMaskSetLoc >= 0)
	{
		glUniform1f(u_psxDrawMaskSetLoc, maskSet ? 1.0f : 0.0f);
	}
}

internal void NativeRenderer_SetScissorState(int enable)
{
	if (s_previousScissorState == enable)
	{
		return;
	}

	if (s_previousScissorState)
	{
		glDisable(GL_SCISSOR_TEST);
	}
	else
	{
		glEnable(GL_SCISSOR_TEST);
	}
	s_previousScissorState = enable;
}

void NativeRenderer_SetOffscreenState(const RECT16 *offscreenRect, int enable)
{
	const int sameOffscreenRect = NativeRenderer_RectEquals(&s_previousOffscreen, offscreenRect);
	if (!enable && !s_previousOffscreenState)
	{
		return;
	}

	if (enable && s_previousOffscreenState && sameOffscreenRect)
	{
		return;
	}

	if (enable)
	{
		if (s_previousOffscreenState)
		{
			NativeRenderer_FlushOffscreenToVRAM();
		}

		s_previousOffscreenState = 1;
		NativeRenderer_EnsureRenderTarget(&s_offscreenRenderTarget, offscreenRect->w, offscreenRect->h);
		s_offscreenRenderTarget.logicalWidth = offscreenRect->w;
		s_offscreenRenderTarget.logicalHeight = offscreenRect->h;
		s_previousOffscreen = *offscreenRect;
		NativeRenderer_LoadRenderTargetFromVRAM(&s_offscreenRenderTarget, offscreenRect->x, offscreenRect->y, offscreenRect->w, offscreenRect->h);
	}
	else
	{
		s_previousOffscreenState = 0;

		NativeRenderer_FlushOffscreenToVRAM();
		NativeRenderer_BindMainRenderTarget();
		NativeRenderer_SetViewPort(0, 0, s_mainRenderTarget.width, s_mainRenderTarget.height);
	}
}
void NativeRenderer_SetProjection(const RECT16 *drawRect, const DISPENV *displayEnv, int offscreen)
{
	if (offscreen)
	{
		NativeRenderer_Ortho2D(0, drawRect->w, drawRect->h, 0, -1.0f, 1.0f);
		return;
	}

	const int displayW = displayEnv->disp.w > 0 ? displayEnv->disp.w : 1;
	const int displayH = displayEnv->disp.h > 0 ? displayEnv->disp.h : 1;
	NativeRenderer_Ortho2D(0, displayW, displayH, 0, -1.0f, 1.0f);
}

void NativeRenderer_SetDepthState(int enable, int write)
{
	if (s_previousDepthMode != enable)
	{
		s_previousDepthMode = enable;
		if (enable)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
	}

	if (s_previousDepthWrite != write)
	{
		s_previousDepthWrite = write;
		glDepthMask(write ? GL_TRUE : GL_FALSE);
	}
}

#if NATIVE_DRAW3D_SUPPORTED
static struct
{
	struct NativeRenderTarget *target;
	GLint drawFramebuffer, readFramebuffer;
} s_isolatedDepth;

int NativeRenderer_BeginIsolatedDepth(void)
{
	if (s_isolatedDepth.target != NULL)
	{
		NativeDraw3D_ReportDiagnostic(NATIVE_DRAW3D_DIAG_OVERLAY_NESTED, "BeginIsolatedDepth", 0);
		return 0;
	}
	GLint drawFramebuffer, readFramebuffer, renderbuffer;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
	struct NativeRenderTarget *target = s_previousOffscreenState ? &s_offscreenRenderTarget : &s_mainRenderTarget;
	if ((GLuint)drawFramebuffer != NativeRenderer_GetDrawFramebuffer(target))
	{
		NativeDraw3D_ReportDiagnostic(NATIVE_DRAW3D_DIAG_OVERLAY_TARGET, "BeginIsolatedDepth", (u32)drawFramebuffer);
		return 0;
	}
	const int samples = target->msaaFramebuffer && (GLuint)drawFramebuffer == target->msaaFramebuffer ? target->samples : 1;
	if (!target->isolatedFramebuffer)
	{
		glGenFramebuffers(1, &target->isolatedFramebuffer);
		glGenRenderbuffers(1, &target->isolatedDepthStencilBuffer);
	}
	if (target->isolatedWidth != target->width || target->isolatedHeight != target->height || target->isolatedSamples != samples)
	{
		glBindRenderbuffer(GL_RENDERBUFFER, target->isolatedDepthStencilBuffer);
		if (samples > 1)
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, target->width, target->height);
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, target->width, target->height);
		target->isolatedWidth = target->width;
		target->isolatedHeight = target->height;
		target->isolatedSamples = samples;
	}
	glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)renderbuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->isolatedFramebuffer);
	// Colour is shared, so this pass composites directly into the live target.
	if (samples > 1)
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target->msaaColorBuffer);
	else
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->isolatedDepthStencilBuffer);
	const GLenum isolatedStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if (isolatedStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)drawFramebuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
		NativeDraw3D_ReportDiagnostic(NATIVE_DRAW3D_DIAG_OVERLAY_FRAMEBUFFER, "BeginIsolatedDepth", (u32)isolatedStatus);
		return 0;
	}
	// Preserve the PS1 draw-mask stencil while giving the model fresh depth.
	const int scissor = s_previousScissorState;
	NativeRenderer_SetScissorState(0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)drawFramebuffer);
	glBlitFramebuffer(0, 0, target->width, target->height, 0, 0, target->width, target->height, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	const int depthMode = s_previousDepthMode, depthWrite = s_previousDepthWrite;
	NativeRenderer_SetDepthState(0, 1);
	glClear(GL_DEPTH_BUFFER_BIT);
	NativeRenderer_SetDepthState(depthMode, depthWrite);
	NativeRenderer_SetScissorState(scissor);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, target->isolatedFramebuffer);
	s_isolatedDepth.target = target;
	s_isolatedDepth.drawFramebuffer = drawFramebuffer;
	s_isolatedDepth.readFramebuffer = readFramebuffer;
	return 1;
}

void NativeRenderer_EndIsolatedDepth(void)
{
	struct NativeRenderTarget *target = s_isolatedDepth.target;
	if (target == NULL)
		return;
	const int scissor = s_previousScissorState;
	NativeRenderer_SetScissorState(0);
	// Only stencil is returned; the world depth attachment stays untouched.
	glBindFramebuffer(GL_READ_FRAMEBUFFER, target->isolatedFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)s_isolatedDepth.drawFramebuffer);
	glBlitFramebuffer(0, 0, target->width, target->height, 0, 0, target->width, target->height, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)s_isolatedDepth.readFramebuffer);
	NativeRenderer_SetScissorState(scissor);
	s_isolatedDepth.target = NULL;
}
#endif

// Always passing still writes depth, unlike disabling the test, so geometry
// drawn later is occluded by what was drawn in retail order.
void NativeRenderer_SetDepthAlwaysPass(int alwaysPass)
{
	if (s_previousDepthAlwaysPass != alwaysPass)
	{
		s_previousDepthAlwaysPass = alwaysPass;
		glDepthFunc(alwaysPass ? GL_ALWAYS : GL_LEQUAL);
	}
}

internal void NativeRenderer_EnableDepth(int enable)
{
#ifdef __vita__
	NativeRenderer_SetDepthState(enable, enable);
#else
	// Blits, presentation and legacy 2D draws have no world depth. The GPU
	// parser explicitly enables testing only for recovered world polygons.
	(void)enable;
	NativeRenderer_SetDepthState(0, 1);
#endif
}

void NativeRenderer_SetStencilMode(int drawPrim)
{
	if (s_previousStencilMode == drawPrim)
	{
		return;
	}

	s_previousStencilMode = drawPrim;

	if (drawPrim)
	{
		glStencilFunc(GL_ALWAYS, 1, 0x10);
		glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
	}
	else
	{
		glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
		glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
	}
}

void NativeRenderer_SetBlendMode(BlendMode blendMode)
{
	if (s_previousBlendMode == blendMode && !s_previousMixedSTPBlend)
	{
		return;
	}

	if (blendMode != BM_NONE)
	{
		if (s_previousBlendMode == BM_NONE)
		{
#ifndef __vita__
			glBlendColor(0.25f, 0.25f, 0.25f, 0.5f);
#endif
			glEnable(GL_BLEND);
		}

		NativeRenderer_EnableDepth(0);
	}

	switch (blendMode)
	{
	case BM_NONE:
		if (s_previousBlendMode != BM_NONE)
		{
#ifndef __vita__
			glBlendColor(1.f, 1.f, 1.f, 1.f);
#endif
			glDisable(GL_BLEND);
		}

		NativeRenderer_EnableDepth(1);
		break;
	case BM_AVERAGE:
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
#ifdef __vita__
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
#else
		// NOTE(aalhendi): keep RGB blend weight constant so alpha can carry the PS1 mask bit.
		glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_ONE, GL_ZERO);
#endif
		break;
	case BM_ADD:
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
		break;
	case BM_SUBTRACT:
		glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
		break;
	case BM_ADD_QUATER_SOURCE:
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
#ifdef __vita__
		glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
#else
		glBlendFuncSeparate(GL_CONSTANT_COLOR, GL_ONE, GL_ONE, GL_ZERO);
#endif
		break;
	case BM_STRAIGHT_ALPHA:
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
		break;
	}

	s_previousBlendMode = blendMode;
	s_previousMixedSTPBlend = 0;
}

void NativeRenderer_SetMixedSTPBlendMode(BlendMode blendMode)
{
#ifdef __vita__
	if (s_previousBlendMode == blendMode && s_previousMixedSTPBlend)
	{
		return;
	}

	if (blendMode == BM_NONE || blendMode == BM_SUBTRACT)
	{
		NativeRenderer_SetBlendMode(blendMode);
		return;
	}

	if (s_previousBlendMode == BM_NONE)
	{
		glEnable(GL_BLEND);
	}

	NativeRenderer_EnableDepth(0);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ONE, GL_ZERO);
	s_previousBlendMode = blendMode;
	s_previousMixedSTPBlend = 1;
#else
	NativeRenderer_SetBlendMode(blendMode);
#endif
}

internal void NativeRenderer_SetViewPort(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}
internal void NativeRenderer_SetWireframe(int enable)
{
#ifdef __EMSCRIPTEN__
	(void)enable;
#else
	glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL);
#endif
}

// ---------------------------------------------------------------------------
// Scoped full-screen passes.
//
// A utility pass rebinds the framebuffer and pushes a fresh shader, blend and
// depth state, then puts the submit run's state back afterwards. That is this
// module's business, so the save/restore lives here and the passes module just
// calls it; keeping it in native_renderer_passes.c is what forced the caches
// above to be shared.
// ---------------------------------------------------------------------------

internal void NativeRenderer_BeginUtilityPass(struct NativeRendererPassState *state, GLuint framebuffer, int x, int y, int width, int height)
{
	state->shader = s_previousShader;
	state->texture = s_lastBoundTexture;
	state->blendMode = s_previousBlendMode;
	state->mixedSTPBlend = s_previousMixedSTPBlend;
	state->depthMode = s_previousDepthMode;
	state->depthWrite = s_previousDepthWrite;
	state->scissorState = s_previousScissorState;
	state->stencilEnabled = glIsEnabled(GL_STENCIL_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	NativeRenderer_SetDepthState(0, 0);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glViewport(x, y, width, height);
}

internal void NativeRenderer_EndUtilityPass(const struct NativeRendererPassState *state)
{
	if (s_boundVertexBuffer >= 0)
	{
		glBindVertexArray(s_glVertexArray[s_boundVertexBuffer]);
	}
	else
	{
		glBindVertexArray(0);
	}

	if (s_previousOffscreenState)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, s_offscreenRenderTarget.framebuffer);
		glViewport(0, 0, s_offscreenRenderTarget.width, s_offscreenRenderTarget.height);
	}
	else
	{
		NativeRenderer_BindMainRenderTarget();
		glViewport(0, 0, s_mainRenderTarget.width, s_mainRenderTarget.height);
	}
	if (state->stencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}

	glUseProgram(state->shader == (ShaderID)-1 ? 0 : state->shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->texture == (TextureID)-1 ? 0 : state->texture);
	s_previousShader = state->shader;
	s_lastBoundTexture = state->texture;
	s_previousBlendMode = BM_NONE;
	s_previousMixedSTPBlend = 0;
	s_previousScissorState = 0;
	if (state->mixedSTPBlend)
	{
		NativeRenderer_SetMixedSTPBlendMode(state->blendMode);
	}
	else
	{
		NativeRenderer_SetBlendMode(state->blendMode);
	}
	NativeRenderer_SetDepthState(state->depthMode, state->depthWrite);
	NativeRenderer_SetScissorState(state->scissorState);
}
