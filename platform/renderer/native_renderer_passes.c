/*
 * Full-screen offscreen passes: saving and restoring the PSX submit state
 * around them, packing a render target back into VRAM, the projected-world
 * world pass and the pause backdrop.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>

#include "platform/native_gpu.h"
#include "platform/native_log.h"
#include "platform/native_projection.h"
#include "platform/native_renderer_internal.h"

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

// NOTE(aalhendi): Pack an RGBA render texture straight into the RG8 VRAM texture
// on the GPU, no CPU round trip. Restore or invalidate the render-state caches
// disturbed by this native bridge before the submit run continues.
internal void NativeRenderer_GpuPackTextureToVRAM(TextureID sourceTexture, int x, int y, int w, int h, b32 flipY)
{
	struct NativeRendererPassState state;

	NativeRenderer_UpdateVRAM();
	NativeRenderer_BeginUtilityPass(&state, s_glVramFramebuffer, x, y, w, h);

	glUseProgram(s_packShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sourceTexture);
	glUniform1i(s_packFlipYLoc, flipY);

	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);

	NativeRenderer_EndUtilityPass(&state);
	NativeRenderer_MarkGpuVRAMNewer(x, y, w, h);
}

global_variable b32 s_projectedWorldSeeded = false;

int NativeRenderer_BindProjectedWorld(int enable)
{
#ifndef __vita__
	if (!enable)
	{
		s_projectedWorldBound = false;
		NativeRenderer_BindMainRenderTarget();
		NativeRenderer_SetViewPort(0, 0, s_mainRenderTarget.width, s_mainRenderTarget.height);
		return 1;
	}

	if (s_previousOffscreenState || (s_mainRenderTarget.width <= 0) || (s_mainRenderTarget.height <= 0))
	{
		return 0;
	}

	if (!s_projectedWorldTargetReady)
	{
		NativeRenderer_InitRenderTarget(&s_projectedWorldTarget);
		s_projectedWorldTargetReady = true;
	}
	NativeRenderer_EnsureRenderTarget(&s_projectedWorldTarget, s_mainRenderTarget.width, s_mainRenderTarget.height);
	NativeRenderer_EnsureMultisampleStorage(&s_projectedWorldTarget, s_mainRenderTarget.samples);
	s_projectedWorldTarget.logicalWidth = s_mainRenderTarget.logicalWidth;
	s_projectedWorldTarget.logicalHeight = s_mainRenderTarget.logicalHeight;

	if (!s_projectedWorldSeeded)
	{
		// Seed the complete target from current main colour once per frame.
		const struct NativeRenderTarget *source = NativeRenderer_ResolveMainRenderTarget();
		struct NativeRendererPassState state;
		NativeRenderer_BeginUtilityPass(&state, NativeRenderer_GetDrawFramebuffer(&s_projectedWorldTarget), 0, 0, s_projectedWorldTarget.width,
		                                s_projectedWorldTarget.height);
		glUseProgram(s_presentRgbaShader);
		glUniform1f(s_presentRgbaFlipYLoc, 0.0f);
		glUniform1i(s_presentRgbaFxaaLoc, 0);
		glUniform2f(s_presentRgbaTexelSizeLoc, 1.0f / (GLfloat)s_mainRenderTarget.width, 1.0f / (GLfloat)s_mainRenderTarget.height);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, source->texture);
		glBindVertexArray(s_vramQuadVAO);
		NativeRenderer_DrawTriangles(0, 2);
		glBindVertexArray(0);
		NativeRenderer_EndUtilityPass(&state);

		// Clear depth/stencil with the scene seed, not on each split batch bind.
		struct NativeRendererPassState clearState;
		NativeRenderer_BeginUtilityPass(&clearState, NativeRenderer_GetDrawFramebuffer(&s_projectedWorldTarget), 0, 0, s_projectedWorldTarget.width,
		                                s_projectedWorldTarget.height);
		glDepthMask(GL_TRUE);
		glStencilMask(0xff);
		glClearDepth(1.0);
		glClearStencil(0);
		glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		NativeRenderer_EndUtilityPass(&clearState);
		s_projectedWorldSeeded = true;
	}

	s_projectedWorldBound = true;
	NativeRenderer_BindMainRenderTarget();
	NativeRenderer_SetViewPort(0, 0, s_projectedWorldTarget.width, s_projectedWorldTarget.height);
	return 1;
#else
	(void)enable;
	return 0;
#endif
}

void NativeRenderer_ResolveProjectedWorld(const RECT16 *cameraRect, const NativeProjectionParams *params)
{
#ifndef __vita__
	if ((cameraRect == NULL) || !s_projectedWorldTargetReady || (s_projectedWorldTarget.width <= 0) || (s_projectedWorldTarget.height <= 0))
	{
		return;
	}
	if (s_projectedWorldBound)
	{
		NativeRenderer_BindProjectedWorld(0);
	}
	NativeRenderer_ResolveMultisample(&s_projectedWorldTarget);

	const int logicalWidth = s_mainRenderTarget.logicalWidth > 0 ? s_mainRenderTarget.logicalWidth : 512;
	const int logicalHeight = s_mainRenderTarget.logicalHeight > 0 ? s_mainRenderTarget.logicalHeight : 216;
	const int x0 = cameraRect->x < 0 ? 0 : (cameraRect->x > logicalWidth ? logicalWidth : cameraRect->x);
	const int y0 = cameraRect->y < 0 ? 0 : (cameraRect->y > logicalHeight ? logicalHeight : cameraRect->y);
	const int x1Raw = cameraRect->x + cameraRect->w;
	const int y1Raw = cameraRect->y + cameraRect->h;
	const int x1 = x1Raw < 0 ? 0 : (x1Raw > logicalWidth ? logicalWidth : x1Raw);
	const int y1 = y1Raw < 0 ? 0 : (y1Raw > logicalHeight ? logicalHeight : y1Raw);
	if ((x1 <= x0) || (y1 <= y0))
	{
		return;
	}

	NativeProjectionParams identity = {NATIVE_PROJECTION_PERSPECTIVE, 0, 1.0, 1.0, 1.0};
	if (params == NULL)
	{
		params = &identity;
	}

	const int dstX = x0 * s_mainRenderTarget.width / logicalWidth;
	const int dstRight = x1 * s_mainRenderTarget.width / logicalWidth;
	const int dstBottom = s_mainRenderTarget.height - y1 * s_mainRenderTarget.height / logicalHeight;
	const int dstTop = s_mainRenderTarget.height - y0 * s_mainRenderTarget.height / logicalHeight;
	const int dstW = dstRight - dstX;
	const int dstH = dstTop - dstBottom;
	if ((dstW <= 0) || (dstH <= 0))
	{
		return;
	}

	const GLfloat sourceRect[4] = {
	    (GLfloat)x0 / (GLfloat)logicalWidth,
	    1.0f - (GLfloat)y1 / (GLfloat)logicalHeight,
	    (GLfloat)(x1 - x0) / (GLfloat)logicalWidth,
	    (GLfloat)(y1 - y0) / (GLfloat)logicalHeight,
	};
	struct NativeRendererPassState state;
	NativeRenderer_BindMainRenderTarget();
	NativeRenderer_BeginUtilityPass(&state, NativeRenderer_GetDrawFramebuffer(&s_mainRenderTarget), dstX, dstBottom, dstW, dstH);
	glUseProgram(s_projectedWorldShader);
	glUniform4fv(s_projectedWorldRectLoc, 1, sourceRect);
	glUniform2f(s_projectedWorldTexelLoc, 1.0f / s_projectedWorldTarget.width, 1.0f / s_projectedWorldTarget.height);
	glUniform1i(s_projectedWorldModeLoc, params->mode);
	glUniform1f(s_projectedWorldStrengthLoc, (GLfloat)params->strength / 100.0f);
	glUniform1f(s_projectedWorldTanHalfLoc, (GLfloat)params->horizontalTanHalf);
	glUniform1f(s_projectedWorldOverscanLoc, (GLfloat)((params->overscan > 0.0) ? params->overscan : 1.0));
	glUniform1f(s_projectedWorldEndpointLoc, (GLfloat)((params->paniniEndpoint > 0.0) ? params->paniniEndpoint : params->horizontalTanHalf));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_projectedWorldTarget.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindVertexArray(0);
	NativeRenderer_EndUtilityPass(&state);
#else
	(void)cameraRect;
	(void)params;
#endif
}

// NOTE(aalhendi): PS1 draws into VRAM and can texture from that same VRAM. Native
// mirrors that by flushing pending CPU VRAM writes, then packing the presented
// framebuffer into the persistent RG8 VRAM texture. CPU-side VRAM reads pull from
// that packed texture lazily, avoiding the old per-frame GPU->CPU->GPU round trip.
void NativeRenderer_StoreFrameBuffer(int x, int y, int w, int h)
{
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_FRAMEBUFFER_STORE);
#ifndef __vita__
	struct NativeRenderTarget *source = (s_projectedWorldBound && s_projectedWorldTargetReady) ? &s_projectedWorldTarget : &s_mainRenderTarget;
	// The pack samples one texel per VRAM pixel, so an SSAA target is packed
	// directly; only MSAA needs a resolve before it can be sampled.
	NativeRenderer_ResolveMultisample(source);
#else
	struct NativeRenderTarget *source = &s_mainRenderTarget;
#endif
	NativeRenderer_GpuPackTextureToVRAM(source->texture, x, y, w, h, true);

	NativePerf_EndScope(NATIVE_PERF_BUCKET_FRAMEBUFFER_STORE);
}

#ifndef __vita__
// Convert the full-resolution main render target into the greyscale pause
// backdrop. Saves and restores the render state like the VRAM pack pass.
int NativeRenderer_CapturePauseBackground(const u16 *bgr555Palette16, int smooth)
{
	if ((bgr555Palette16 == NULL) || (s_mainRenderTarget.texture == (TextureID)-1) || (s_mainRenderTarget.width < 2) || (s_mainRenderTarget.height < 2))
	{
		return 0;
	}

	// Anti-aliased, presentation-resolution source image.
	const struct NativeRenderTarget *source = NativeRenderer_ResolveMainRenderTarget();

	if (!s_pauseBackgroundTargetReady)
	{
		NativeRenderer_InitRenderTarget(&s_pauseBackgroundTarget);
		s_pauseBackgroundTargetReady = true;
	}
	NativeRenderer_EnsureRenderTarget(&s_pauseBackgroundTarget, source->width, source->height);

	// The backdrop is sampled as a streaming texture, so keep it smooth if the window is resized while paused.
	glBindTexture(GL_TEXTURE_2D, s_pauseBackgroundTarget.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	float palette[16 * 3];
	for (int i = 0; i < 16; i++)
	{
		const int r5 = bgr555Palette16[i] & 31;
		const int g5 = (bgr555Palette16[i] >> 5) & 31;
		const int b5 = (bgr555Palette16[i] >> 10) & 31;
		// Same 5-to-8 bit expansion as the VRAM present path.
		palette[i * 3 + 0] = (float)((r5 << 3) | (r5 >> 2)) / 255.0f;
		palette[i * 3 + 1] = (float)((g5 << 3) | (g5 >> 2)) / 255.0f;
		palette[i * 3 + 2] = (float)((b5 << 3) | (b5 >> 2)) / 255.0f;
	}

	struct NativeRendererPassState state;
	NativeRenderer_BeginUtilityPass(&state, s_pauseBackgroundTarget.framebuffer, 0, 0, s_pauseBackgroundTarget.width, s_pauseBackgroundTarget.height);

	glUseProgram(s_pauseBackgroundShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, source->texture);
	// Row 0 of the backdrop is the top of the screen, like a streaming texture.
	// The main target stores the top of the screen at its last row, so flip,
	// as StoreFrameBuffer does when packing it into VRAM.
	glUniform1i(s_pauseBackgroundFlipYLoc, 1);
	glUniform1i(s_pauseBackgroundSmoothLoc, smooth != 0);
	glUniform3fv(s_pauseBackgroundPaletteLoc, 16, palette);

	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);

	NativeRenderer_EndUtilityPass(&state);
	return 1;
}

TextureID NativeRenderer_GetPauseBackgroundTexture(void)
{
	return s_pauseBackgroundTargetReady ? s_pauseBackgroundTarget.texture : (TextureID)-1;
}
#endif

#ifndef __vita__

// The projected-world target persists across a frame; the pause backdrop
// persists past it. Both are released only with the renderer.
internal void NativeRenderer_DestroyPassTargets(void)
{
	if (s_projectedWorldTargetReady)
	{
		NativeRenderer_DestroyRenderTarget(&s_projectedWorldTarget);
		s_projectedWorldTargetReady = false;
	}
	if (s_pauseBackgroundTargetReady)
	{
		NativeRenderer_DestroyRenderTarget(&s_pauseBackgroundTarget);
		s_pauseBackgroundTargetReady = false;
	}
}

// A new frame re-seeds the projected-world target from the current display
// environment rather than reusing the previous frame's contents.
internal void NativeRenderer_BeginPassFrame(void)
{
	s_projectedWorldBound = false;
	s_projectedWorldSeeded = false;
}

#endif
