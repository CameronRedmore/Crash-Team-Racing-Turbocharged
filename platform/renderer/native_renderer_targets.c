/*
 * Render targets and presentation: main/offscreen/supersample targets, MSAA
 * and SSAA, the aspect-ratio viewport and letterbox bars, and presenting the
 * finished frame.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>

#include "platform/native_aspect.h"
#include "platform/native_log.h"
#include "platform/native_options.h"
#include "platform/native_projection.h"
#include "platform/native_renderer_internal.h"

// Set to the sample count the driver refused, so a failed MSAA allocation
// is not retried on every target every frame.
global_variable int s_multisampleFailedSamples = 0;

// Aspect the presentation viewport is built from.
global_variable int s_presentAspectW = 4;
global_variable int s_presentAspectH = 3;

internal int NativeRenderer_IntAbs(int value)
{
	return value < 0 ? -value : value;
}

internal int NativeRenderer_GCD(int a, int b)
{
	a = NativeRenderer_IntAbs(a);
	b = NativeRenderer_IntAbs(b);

	while (b != 0)
	{
		int t = a % b;
		a = b;
		b = t;
	}

	return a;
}

internal void NativeRenderer_SetPresentationAspect(int width, int height)
{
	const int divisor = NativeRenderer_GCD(width, height);

	if ((width <= 0) || (height <= 0) || (divisor <= 0))
	{
		return;
	}

	s_presentAspectW = width / divisor;
	s_presentAspectH = height / divisor;
}

internal void NativeRenderer_UpdatePresentationViewport(void)
{
	int viewportW;
	int viewportH;

	if ((g_windowWidth <= 0) || (g_windowHeight <= 0) || (s_presentAspectW <= 0) || (s_presentAspectH <= 0))
	{
		s_presentViewport.x = 0;
		s_presentViewport.y = 0;
		s_presentViewport.w = g_windowWidth;
		s_presentViewport.h = g_windowHeight;
		return;
	}

	viewportW = g_windowWidth;
	viewportH = (viewportW * s_presentAspectH) / s_presentAspectW;

	if (viewportH > g_windowHeight)
	{
		viewportH = g_windowHeight;
		viewportW = (viewportH * s_presentAspectW) / s_presentAspectH;
	}

	if (viewportW < 1)
	{
		viewportW = 1;
	}
	if (viewportH < 1)
	{
		viewportH = 1;
	}

	s_presentViewport.w = viewportW;
	s_presentViewport.h = viewportH;
	s_presentViewport.x = (g_windowWidth - viewportW) / 2;
	s_presentViewport.y = (g_windowHeight - viewportH) / 2;
}

internal void NativeRenderer_InitRenderTarget(struct NativeRenderTarget *target)
{
	target->texture = (TextureID)-1;
	glGenTextures(1, &target->texture);
	glBindTexture(GL_TEXTURE_2D, target->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, NATIVE_RENDER_TARGET_TYPE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenRenderbuffers(1, &target->stencilBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, target->stencilBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glGenFramebuffers(1, &target->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->stencilBuffer);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		NATIVE_RENDERER_ERROR("%s\n", "failed to create RGBA/depth/stencil render target");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	target->logicalWidth = 0;
	target->logicalHeight = 0;
}

#ifndef __vita__
internal void NativeRenderer_DestroyMultisampleStorage(struct NativeRenderTarget *target)
{
	if (target->msaaFramebuffer != 0)
	{
		glDeleteFramebuffers(1, &target->msaaFramebuffer);
		glDeleteRenderbuffers(1, &target->msaaColorBuffer);
		glDeleteRenderbuffers(1, &target->msaaDepthStencilBuffer);
	}
	target->msaaFramebuffer = 0;
	target->msaaColorBuffer = 0;
	target->msaaDepthStencilBuffer = 0;
	target->samples = 0;
	target->msaaWidth = 0;
	target->msaaHeight = 0;
}

// Allocate (or drop, for samples <= 1) multisampled colour and depth/stencil
// storage matching the target's current size.
internal void NativeRenderer_EnsureMultisampleStorage(struct NativeRenderTarget *target, int samples)
{
	if ((samples <= 1) || (samples == s_multisampleFailedSamples))
	{
		if (target->msaaFramebuffer != 0)
		{
			NativeRenderer_DestroyMultisampleStorage(target);
		}
		return;
	}

	if ((target->samples == samples) && (target->msaaWidth == target->width) && (target->msaaHeight == target->height))
	{
		return;
	}

	if (target->msaaFramebuffer == 0)
	{
		glGenFramebuffers(1, &target->msaaFramebuffer);
		glGenRenderbuffers(1, &target->msaaColorBuffer);
		glGenRenderbuffers(1, &target->msaaDepthStencilBuffer);
	}

	glBindRenderbuffer(GL_RENDERBUFFER, target->msaaColorBuffer);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, target->width, target->height);
	glBindRenderbuffer(GL_RENDERBUFFER, target->msaaDepthStencilBuffer);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, target->width, target->height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, target->msaaFramebuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target->msaaColorBuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->msaaDepthStencilBuffer);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		NATIVE_RENDERER_ERROR("failed to create %dx MSAA render target, falling back to no MSAA\n", samples);
		NativeRenderer_DestroyMultisampleStorage(target);
		s_multisampleFailedSamples = samples;
		return;
	}

	target->samples = samples;
	target->msaaWidth = target->width;
	target->msaaHeight = target->height;
}

internal int NativeRenderer_MultisampleCount(int mode)
{
	int samples = 0;
	switch (mode)
	{
	case NATIVE_AA_MSAA_2X:
		samples = 2;
		break;
	case NATIVE_AA_MSAA_4X:
		samples = 4;
		break;
	case NATIVE_AA_MSAA_8X:
		samples = 8;
		break;
	default:
		return 0;
	}
	return (samples > s_maxSamples) ? s_maxSamples : samples;
}

// Per-axis render scale, so SSAA 2X has twice and SSAA 4X four times the pixels.
internal float NativeRenderer_SupersampleScale(int mode)
{
	switch (mode)
	{
	case NATIVE_AA_SSAA_2X:
		return 1.41421356f;
	case NATIVE_AA_SSAA_4X:
		return 2.0f;
	default:
		return 1.0f;
	}
}
#endif

internal GLuint NativeRenderer_GetDrawFramebuffer(const struct NativeRenderTarget *target)
{
#ifndef __vita__
	if (target->samples > 1)
	{
		return target->msaaFramebuffer;
	}
#endif
	return target->framebuffer;
}

internal void NativeRenderer_DestroyRenderTarget(struct NativeRenderTarget *target)
{
#if NATIVE_DRAW3D_SUPPORTED
	glDeleteFramebuffers(1, &target->isolatedFramebuffer);
	glDeleteRenderbuffers(1, &target->isolatedDepthStencilBuffer);
#endif
#ifndef __vita__
	NativeRenderer_DestroyMultisampleStorage(target);
#endif
	glDeleteFramebuffers(1, &target->framebuffer);
	glDeleteRenderbuffers(1, &target->stencilBuffer);
	NativeRenderer_DestroyTexture(target->texture);
	SDL_memset(target, 0, sizeof(*target));
	target->texture = (TextureID)-1;
}

internal void NativeRenderer_EnsureRenderTarget(struct NativeRenderTarget *target, int width, int height)
{
	if (width < 1)
	{
		width = 1;
	}
	if (height < 1)
	{
		height = 1;
	}

	if ((target->width == width) && (target->height == height))
	{
		return;
	}

	glBindTexture(GL_TEXTURE_2D, target->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, NATIVE_RENDER_TARGET_TYPE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, target->stencilBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	target->width = width;
	target->height = height;
	s_lastBoundTexture = (TextureID)-1;
}

internal void NativeRenderer_BindMainRenderTarget(void)
{
	int logicalWidth = NativeGpu_GetRenderDispEnv()->disp.w;
	int logicalHeight = NativeGpu_GetRenderDispEnv()->disp.h;
	if ((logicalWidth <= 0) || (logicalHeight <= 0))
	{
		logicalWidth = NativeGpu_GetRenderDrawEnv()->clip.w;
		logicalHeight = NativeGpu_GetRenderDrawEnv()->clip.h;
	}

	int physicalWidth = logicalWidth;
	int physicalHeight = logicalHeight;
#if CTR_NATIVE_WIDESCREEN
	if ((g_windowWidth > 0) && (g_windowHeight > 0))
	{
		physicalWidth = (s_presentViewport.w > 0) ? s_presentViewport.w : g_windowWidth;
		physicalHeight = (s_presentViewport.h > 0) ? s_presentViewport.h : g_windowHeight;
	}
#endif

#ifndef __vita__
	s_mainResolveWidth = (physicalWidth < 1) ? 1 : physicalWidth;
	s_mainResolveHeight = (physicalHeight < 1) ? 1 : physicalHeight;
	float scale = NativeRenderer_SupersampleScale(s_frameAntiAliasingMode);
	if (scale > 1.0f)
	{
		// Keep the aspect ratio when the GPU's maximum target size limits the scale.
		const int largestSide = (s_mainResolveWidth > s_mainResolveHeight) ? s_mainResolveWidth : s_mainResolveHeight;
		if ((float)largestSide * scale > (float)s_maxRenderTargetSize)
		{
			scale = (float)s_maxRenderTargetSize / (float)largestSide;
		}
		if (scale > 1.0f)
		{
			physicalWidth = (int)((float)s_mainResolveWidth * scale + 0.5f);
			physicalHeight = (int)((float)s_mainResolveHeight * scale + 0.5f);
		}
	}
#endif

	NativeRenderer_EnsureRenderTarget(&s_mainRenderTarget, physicalWidth, physicalHeight);
#ifndef __vita__
	NativeRenderer_EnsureMultisampleStorage(&s_mainRenderTarget, NativeRenderer_MultisampleCount(s_frameAntiAliasingMode));
#endif
	s_mainRenderTarget.logicalWidth = logicalWidth;
	s_mainRenderTarget.logicalHeight = logicalHeight;
	struct NativeRenderTarget *drawTarget = &s_mainRenderTarget;
#ifndef __vita__
	if (s_projectedWorldBound && s_projectedWorldTargetReady)
	{
		drawTarget = &s_projectedWorldTarget;
		NativeRenderer_EnsureRenderTarget(drawTarget, s_mainRenderTarget.width, s_mainRenderTarget.height);
		NativeRenderer_EnsureMultisampleStorage(drawTarget, s_mainRenderTarget.samples);
		drawTarget->logicalWidth = logicalWidth;
		drawTarget->logicalHeight = logicalHeight;
	}
#endif
	glBindFramebuffer(GL_FRAMEBUFFER, NativeRenderer_GetDrawFramebuffer(drawTarget));
}

internal void NativeRenderer_DrawVRAMRegion(int x, int y, int width, int height)
{
	glUseProgram(s_presentVramShader);
	glUniform4f(s_presentVramSourceRectLoc, (float)x, (float)y, (float)width, (float)height);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_vram.texture);
	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);
}

internal void NativeRenderer_LoadRenderTargetFromVRAM(struct NativeRenderTarget *target, int x, int y, int logicalWidth, int logicalHeight)
{
	const ShaderID previousShader = s_previousShader;
	const TextureID previousTexture = s_lastBoundTexture;
	const BlendMode previousBlendMode = s_previousBlendMode;
	const int previousMixedSTPBlend = s_previousMixedSTPBlend;
	const int previousDepthMode = s_previousDepthMode;
	const int previousDepthWrite = s_previousDepthWrite;
	const int previousScissorState = s_previousScissorState;
	const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);

	NativeRenderer_UpdateVRAM();
	glBindFramebuffer(GL_FRAMEBUFFER, NativeRenderer_GetDrawFramebuffer(target));
	NativeRenderer_SetDepthState(0, 1);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glViewport(0, 0, target->width, target->height);
	NativeRenderer_DrawVRAMRegion(x, y, logicalWidth, logicalHeight);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (previousStencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}

	if (s_boundVertexBuffer >= 0)
	{
		glBindVertexArray(s_glVertexArray[s_boundVertexBuffer]);
	}
	else
	{
		glBindVertexArray(0);
	}
	glUseProgram(previousShader == (ShaderID)-1 ? 0 : previousShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, previousTexture == (TextureID)-1 ? 0 : previousTexture);
	s_previousShader = previousShader;
	s_lastBoundTexture = previousTexture;
	s_previousBlendMode = BM_NONE;
	s_previousMixedSTPBlend = 0;
	s_previousScissorState = 0;
	if (previousMixedSTPBlend)
	{
		NativeRenderer_SetMixedSTPBlendMode(previousBlendMode);
	}
	else
	{
		NativeRenderer_SetBlendMode(previousBlendMode);
	}
	NativeRenderer_SetDepthState(previousDepthMode, previousDepthWrite);
	NativeRenderer_SetScissorState(previousScissorState);
}

internal void NativeRenderer_ClearHostRect(int x, int y, int width, int height)
{
	if ((width <= 0) || (height <= 0))
	{
		return;
	}

	glScissor(x, y, width, height);
	glClear(GL_COLOR_BUFFER_BIT);
}

internal void NativeRenderer_ClearPresentationBars(void)
{
	GLint previousScissorBox[4];
	GLfloat previousClearColor[4];
	const GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	const int viewportRight = s_presentViewport.x + s_presentViewport.w;
	const int viewportTop = s_presentViewport.y + s_presentViewport.h;

	if ((g_windowWidth <= 0) || (g_windowHeight <= 0))
	{
		return;
	}

	if ((s_presentViewport.x == 0) && (s_presentViewport.y == 0) && (s_presentViewport.w == g_windowWidth) && (s_presentViewport.h == g_windowHeight))
	{
		return;
	}

	glGetIntegerv(GL_SCISSOR_BOX, previousScissorBox);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);

	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	NativeRenderer_ClearHostRect(0, 0, s_presentViewport.x, g_windowHeight);
	NativeRenderer_ClearHostRect(viewportRight, 0, g_windowWidth - viewportRight, g_windowHeight);
	NativeRenderer_ClearHostRect(s_presentViewport.x, 0, s_presentViewport.w, s_presentViewport.y);
	NativeRenderer_ClearHostRect(s_presentViewport.x, viewportTop, s_presentViewport.w, g_windowHeight - viewportTop);

	if (previousScissorEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(previousScissorBox[0], previousScissorBox[1], previousScissorBox[2], previousScissorBox[3]);
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}

	glClearColor(previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);
	s_previousScissorState = previousScissorEnabled ? 1 : 0;
	NativeRenderer_InvalidateScissorRectCache();
}

internal void NativeRenderer_UpdateGamePresentationAspect(void)
{
	if (!s_gamePresentationEnabled)
	{
		return;
	}

	int presentationWidth = 0;
	int presentationHeight = 0;
	NativeAspect_GetPresentation(&presentationWidth, &presentationHeight);
	if ((presentationWidth <= 0) || (presentationHeight <= 0))
	{
		return;
	}

	const int divisor = NativeRenderer_GCD(presentationWidth, presentationHeight);
	if ((divisor <= 0) || ((s_presentAspectW == presentationWidth / divisor) && (s_presentAspectH == presentationHeight / divisor)))
	{
		return;
	}
	NativeRenderer_SetPresentationAspect(presentationWidth, presentationHeight);
	// A frozen pause capture is sampled by normalized UVs and remains valid
	// when the viewport changes. Retain it through in-game aspect/renderer
	// changes; the next pause capture resizes its storage to the new source.
	NativeRenderer_UpdatePresentationViewport();
}

void NativeRenderer_EnableGamePresentation(int enabled)
{
	s_gamePresentationEnabled = enabled != 0;
	if (!s_gamePresentationEnabled)
	{
		// Restore the dimensions supplied by the renderer's initializer.
		NativeRenderer_SetPresentationAspect(s_startupAspectW, s_startupAspectH);
		NativeRenderer_UpdatePresentationViewport();
		return;
	}
	NativeRenderer_UpdateGamePresentationAspect();
	NativeRenderer_UpdatePresentationViewport();
}

#ifndef __vita__
// SSAA target holding the presentation-resolution result, and whether it has
// allocated storage yet. Both are only ever touched from this file.
global_variable struct NativeRenderTarget s_supersampleResolveTarget;
global_variable b32 s_supersampleResolveTargetReady = false;

// Blit the multisampled samples into the target's single-sample texture.
internal void NativeRenderer_ResolveMultisample(const struct NativeRenderTarget *target)
{
	if (target->samples <= 1)
	{
		return;
	}

	GLint previousFramebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
	const GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	glDisable(GL_SCISSOR_TEST);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, target->msaaFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
	glBlitFramebuffer(0, 0, target->width, target->height, 0, 0, target->width, target->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);

	if (previousScissorEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
	}
}

// Return the main target as a single-sample image at presentation resolution:
// MSAA samples are resolved and SSAA is box-filtered down. Without either,
// this is the main target itself.
internal const struct NativeRenderTarget *NativeRenderer_ResolveMainRenderTarget(void)
{
	NativeRenderer_ResolveMultisample(&s_mainRenderTarget);

	if ((s_mainResolveWidth <= 0) || (s_mainResolveHeight <= 0) ||
	    ((s_mainRenderTarget.width == s_mainResolveWidth) && (s_mainRenderTarget.height == s_mainResolveHeight)))
	{
		return &s_mainRenderTarget;
	}

	if (!s_supersampleResolveTargetReady)
	{
		NativeRenderer_InitRenderTarget(&s_supersampleResolveTarget);
		s_supersampleResolveTargetReady = true;
	}
	NativeRenderer_EnsureRenderTarget(&s_supersampleResolveTarget, s_mainResolveWidth, s_mainResolveHeight);

	struct NativeRendererPassState state;
	NativeRenderer_BeginUtilityPass(&state, s_supersampleResolveTarget.framebuffer, 0, 0, s_supersampleResolveTarget.width, s_supersampleResolveTarget.height);

	glUseProgram(s_downsampleShader);
	glUniform2f(s_downsampleSrcSizeLoc, (float)s_mainRenderTarget.width, (float)s_mainRenderTarget.height);
	glUniform2f(s_downsampleDstSizeLoc, (float)s_supersampleResolveTarget.width, (float)s_supersampleResolveTarget.height);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_mainRenderTarget.texture);
	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);

	NativeRenderer_EndUtilityPass(&state);
	return &s_supersampleResolveTarget;
}
#endif

void NativeRenderer_PresentMainRenderTarget(void)
{
	if ((s_mainRenderTarget.texture == 0) || (s_mainRenderTarget.width <= 0) || (s_mainRenderTarget.height <= 0))
	{
		return;
	}

#ifndef __vita__
	const struct NativeRenderTarget *source = NativeRenderer_ResolveMainRenderTarget();
#else
	const struct NativeRenderTarget *source = &s_mainRenderTarget;
#endif

	NativeRenderer_SetViewPort(s_presentViewport.x, s_presentViewport.y, s_presentViewport.w, s_presentViewport.h);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	NativeRenderer_SetScissorState(0);
	NativeRenderer_EnableDepth(0);
	NativeRenderer_SetBlendMode(BM_NONE);
	const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);
	glDisable(GL_STENCIL_TEST);

	glUseProgram(s_presentRgbaShader);
	glUniform1f(s_presentRgbaFlipYLoc, 0.0f);
#ifndef __vita__
	if (s_presentRgbaTexelSizeLoc >= 0)
	{
		glUniform2f(s_presentRgbaTexelSizeLoc, 1.0f / (float)source->width, 1.0f / (float)source->height);
	}
	if (s_presentRgbaFxaaLoc >= 0)
	{
		glUniform1i(s_presentRgbaFxaaLoc, s_frameAntiAliasingMode == NATIVE_AA_FXAA);
	}
#endif
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, source->texture);
#ifndef __vita__
	// FXAA uses subpixel samples from the final native-resolution render target.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);
#ifndef __vita__
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#endif

	if (previousStencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}
	glBindVertexArray(0);

	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
}

void NativeRenderer_GetStreamingViewport(int contentHeight, int displayHeight, int *x, int *y, int *width, int *height)
{
	int displayX = s_presentViewport.x;
	int displayY = s_presentViewport.y;
	int displayW = s_presentViewport.w;
	int displayH = s_presentViewport.h;

#if !defined(__vita__) && !defined(__EMSCRIPTEN__)
	if (s_gamePresentationEnabled && NativeAspect_IsActive() && (displayW > 0) && (displayH > 0))
	{
		// Video assets use a 4:3 display area even when game rendering fills a
		// wider selected viewport. Fit and centre that area before applying the
		// video's own vertical content fraction.
		if (displayW * 3 > displayH * 4)
		{
			displayW = (displayH * 4) / 3;
		}
		else
		{
			displayH = (displayW * 3) / 4;
		}
		displayX = s_presentViewport.x + (s_presentViewport.w - displayW) / 2;
		displayY = s_presentViewport.y + (s_presentViewport.h - displayH) / 2;
	}
#endif

	int viewportH = displayH;
	if ((contentHeight > 0) && (displayHeight >= contentHeight))
	{
		viewportH = (displayH * contentHeight + displayHeight / 2) / displayHeight;
		if (viewportH < 1)
		{
			viewportH = 1;
		}
	}
	const int viewportY = displayY + (displayH - viewportH) / 2;

	if (x)
		*x = displayX;
	if (y)
		*y = viewportY;
	if (width)
		*width = displayW;
	if (height)
		*height = viewportH;
}

void NativeRenderer_PresentStreamingTexture(TextureID texture, int contentHeight, int displayHeight)
{
	if ((texture == 0) || (contentHeight <= 0) || (displayHeight < contentHeight))
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	NativeRenderer_SetScissorState(0);
	NativeRenderer_EnableDepth(0);
	NativeRenderer_SetBlendMode(BM_NONE);
	const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);
	glDisable(GL_STENCIL_TEST);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	int viewportX;
	int viewportY;
	int viewportW;
	int viewportH;
	NativeRenderer_GetStreamingViewport(contentHeight, displayHeight, &viewportX, &viewportY, &viewportW, &viewportH);
	NativeRenderer_SetViewPort(viewportX, viewportY, viewportW, viewportH);

	glUseProgram(s_presentRgbaShader);
	glUniform1f(s_presentRgbaFlipYLoc, 1.0f);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);

	if (previousStencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}
	glBindVertexArray(0);
	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
}

void NativeRenderer_PresentVRAMDisplay(void)
{
	// NOTE(aalhendi): ctr-native local divergence. Retail presents this boot
	// splash path by displaying VRAM directly after DR_MOVE packets; the native
	// OpenGL backend otherwise swaps the current framebuffer and never shows
	// those VRAM-only copies.
	NativeRenderer_PresentVRAMRect(NativeGpu_GetRenderDispEnv()->disp.x, NativeGpu_GetRenderDispEnv()->disp.y, NativeGpu_GetRenderDispEnv()->disp.w,
	                               NativeGpu_GetRenderDispEnv()->disp.h);
}

void NativeRenderer_SwapWindow(void)
{
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_SWAP_WINDOW);
#ifdef __vita__
	if (NativeAdhoc_IsDialogRunning())
	{
		vglSwapBuffers(GL_TRUE);
	}
	else
	{
		SDL_GL_SwapWindow(g_window);
	}
#else
	SDL_GL_SwapWindow(g_window);
#endif
	NativePerf_EndScope(NATIVE_PERF_BUCKET_SWAP_WINDOW);
}

// The targets the scene itself renders into. The projected-world and pause
// backdrop targets belong to native_renderer_passes.c and are freed by
// NativeRenderer_DestroyPassTargets; a reset path has to call both.
internal void NativeRenderer_DestroySceneTargets(void)
{
	NativeRenderer_DestroyRenderTarget(&s_mainRenderTarget);
	NativeRenderer_DestroyRenderTarget(&s_offscreenRenderTarget);
#ifndef __vita__
	if (s_supersampleResolveTargetReady)
	{
		NativeRenderer_DestroyRenderTarget(&s_supersampleResolveTarget);
		s_supersampleResolveTargetReady = false;
	}
#endif
}
