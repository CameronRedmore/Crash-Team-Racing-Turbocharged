/*
 * Device lifecycle: the SDL window and GL context, renderer start-up and
 * teardown, and the per-frame scene boundaries.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>
#include <string.h>

#include "platform/native_aspect.h"
#include "platform/native_font.h"
#include "platform/native_gpu.h"
#include "platform/native_log.h"
#include "platform/native_minimap.h"
#include "platform/native_options.h"
#include "platform/native_perf.h"
#include "platform/native_renderer_internal.h"

#ifdef _WIN32
#include "platform/native_win32.h"

__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#endif // def WIN32

#if defined(CTR_INTERNAL)
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#define NATIVE_GPU_TIMER_QUERY_COUNT 8

struct NativeGpuTimerQuery
{
	GLuint id;
	u32 frameIndex;
	b32 pending;
};

global_variable struct NativeGpuTimerQuery s_gpuTimerQueries[NATIVE_GPU_TIMER_QUERY_COUNT];
global_variable u32 s_gpuTimerFrameIndex;
global_variable s32 s_gpuTimerNextQuery;
global_variable b32 s_gpuTimerSupported;
global_variable b32 s_gpuTimerActive;
#endif
internal int NativeRenderer_InitialiseGLContext(char *windowName, int fullscreen)
{
	SDL_WindowFlags windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifndef __vita__
	windowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

	if (fullscreen)
	{
		windowFlags |= SDL_WINDOW_FULLSCREEN;
	}

	g_window = SDL_CreateWindow(windowName, g_windowWidth, g_windowHeight, windowFlags);

	if (g_window == NULL)
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to initialise SDL window!");
		return 0;
	}

	int major_version = 3;
#ifdef __EMSCRIPTEN__
	int minor_version = 0;
	int profile = SDL_GL_CONTEXT_PROFILE_ES;
#else
	int minor_version = 3;
	int profile = SDL_GL_CONTEXT_PROFILE_CORE;
#endif

	// find best OpenGL version
	do
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major_version);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_version);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

		if (SDL_GL_CreateContext(g_window))
		{
			break;
		}

		minor_version--;

	} while (minor_version >= 0);

	if (minor_version == -1)
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to initialise - OpenGL 3.x is not supported. Please update video drivers.");
		return 0;
	}

	return 1;
}

internal int NativeRenderer_InitialiseGLExt(void)
{
#if !defined(__vita__) && !defined(__EMSCRIPTEN__)
	GLenum err = gladLoadGL();

	if (err == 0)
	{
		return 0;
	}
#endif

	const char *rend = (const char *)glGetString(GL_RENDERER);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	NATIVE_RENDERER_LOG("*Video adapter: %s by %s\n", rend, vendor);

	const char *versionStr = (const char *)glGetString(GL_VERSION);
	NATIVE_RENDERER_LOG("*OpenGL version: %s\n", versionStr);

	const char *glslVersionStr = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	NATIVE_RENDERER_LOG("*GLSL version: %s\n", glslVersionStr);
	return 1;
}

int NativeRenderer_InitialiseRender(char *windowName, int width, int height, int fullscreen)
{
	g_windowWidth = width;
	g_windowHeight = height;
	s_gamePresentationEnabled = 0;
	s_startupAspectW = width;
	s_startupAspectH = height;
	NativeRenderer_SetPresentationAspect(width, height);

	// Due to debugging in fullscreen
	SDL_SetHint(SDL_HINT_WINDOW_ALLOW_TOPMOST, "0");
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	if (!NativeRenderer_InitialiseGLContext(windowName, fullscreen))
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to Initialise GL Context!");
		return 0;
	}
#ifndef __vita__
	SDL_GetWindowSizeInPixels(g_window, &g_windowWidth, &g_windowHeight);
#endif

	if (!NativeRenderer_InitialiseGLExt())
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to Intialise GL extensions");
		return 0;
	}

	return 1;
}

void NativeRenderer_Shutdown(void)
{
	NativeFont_ReleaseGpu();
	NativeMinimap_ReleaseGpu();
	glDeleteVertexArrays(MAX_NUM_VERTEX_BUFFERS, s_glVertexArray);
	glDeleteBuffers(MAX_NUM_VERTEX_BUFFERS, s_glVertexBuffer);

	NativeRenderer_DestroyAllTargets();
#ifndef __vita__
	NativeRenderer_DestroyPassTargets();
#endif
	glDeleteFramebuffers(1, &s_glVramFramebuffer);

	NativeRenderer_DestroyTexture(s_vram.texture);

	NativeRenderer_DestroyTexture(s_whiteTexture);
	NativeRenderer_DestroyTexture(s_rgLutTexture);
	NativeRenderer_DestroyGhostReplayTextures();
#ifdef __vita__
	NativeRenderer_DestroyP4Textures();
	NativeRenderer_DestroyTexture(s_presentLutTexture);
#endif
	NativeRenderer_DestroyPSXShaders();
	glDeleteProgram(s_packShader);
#ifndef __vita__
	glDeleteProgram(s_pauseBackgroundShader);
	glDeleteProgram(s_projectedWorldShader);
	glDeleteProgram(s_downsampleShader);
#endif
	glDeleteProgram(s_presentVramShader);
	glDeleteProgram(s_presentRgbaShader);
	glDeleteVertexArrays(1, &s_vramQuadVAO);
	glDeleteBuffers(1, &s_vramQuadVBO);
}

#if defined(CTR_INTERNAL)
internal void NativeRenderer_ResolveGpuMeasurements(b32 waitForResults)
{
	for (s32 i = 0; i < NATIVE_GPU_TIMER_QUERY_COUNT; i++)
	{
		struct NativeGpuTimerQuery *query = &s_gpuTimerQueries[i];
		if (!query->pending)
		{
			continue;
		}

		GLint available = 0;
		if (!waitForResults)
		{
			glGetQueryObjectiv(query->id, GL_QUERY_RESULT_AVAILABLE, &available);
			if (!available)
			{
				continue;
			}
		}

		GLuint elapsedNanoseconds;
		glGetQueryObjectuiv(query->id, GL_QUERY_RESULT, &elapsedNanoseconds);
		NativePerf_RecordGpuFrame(query->frameIndex, (f64)elapsedNanoseconds / 1000000.0);
		query->pending = false;
	}
}
#endif

void NativeRenderer_UpdateSwapIntervalState(int swapInterval)
{
	SDL_GL_SetSwapInterval(swapInterval);
}

void NativeRenderer_BeginScene(void)
{
#if defined(CTR_INTERNAL)
	NativeRenderer_ResolveGpuMeasurements(false);
	const u32 gpuFrameIndex = s_gpuTimerFrameIndex++;
	if (s_gpuTimerSupported && NativePerf_IsEnabled())
	{
		struct NativeGpuTimerQuery *query = &s_gpuTimerQueries[s_gpuTimerNextQuery];
		if (!query->pending)
		{
			query->frameIndex = gpuFrameIndex;
			query->pending = true;
			glBeginQuery(GL_TIME_ELAPSED, query->id);
			s_gpuTimerActive = true;
			s_gpuTimerNextQuery = (s_gpuTimerNextQuery + 1) % NATIVE_GPU_TIMER_QUERY_COUNT;
		}
	}
#endif

	NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDERER_BEGIN_SCENE);
#ifndef __vita__
	s_frameAntiAliasingMode =
	    ((gNativeAntiAliasingMode >= NATIVE_AA_OFF) && (gNativeAntiAliasingMode < NATIVE_AA_MODE_COUNT)) ? gNativeAntiAliasingMode : NATIVE_AA_OFF;
	NativeRenderer_BeginPassFrame();
#endif
#ifdef __vita__
	NativeRenderer_BeginP4Frame();
#endif
	s_lastBoundTexture = 0;

	NativeRenderer_UpdateGamePresentationAspect();
	NativeRenderer_UpdatePresentationViewport();
	NativeRenderer_ClearPresentationBars();
	NativeRenderer_BindMainRenderTarget();
	NativeRenderer_SetDepthState(0, 1);

	NativeRenderer_UpdateVRAM();
	if (!NativeGpu_GetRenderDrawEnv()->isbg)
	{
		NativeRenderer_LoadRenderTargetFromVRAM(&s_mainRenderTarget, NativeGpu_GetRenderDispEnv()->disp.x, NativeGpu_GetRenderDispEnv()->disp.y,
		                                        s_mainRenderTarget.logicalWidth, s_mainRenderTarget.logicalHeight);
	}
	else
	{
		const GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
		glDisable(GL_SCISSOR_TEST);
		glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (previousScissorEnabled)
		{
			glEnable(GL_SCISSOR_TEST);
		}
	}
	NativeRenderer_SetViewPort(0, 0, s_mainRenderTarget.width, s_mainRenderTarget.height);

	if (g_dbg_wireframeMode)
	{
		NativeRenderer_SetWireframe(1);

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_BEGIN_SCENE);
}

void NativeRenderer_EndGpuFrame(void)
{
#if defined(CTR_INTERNAL)
	if (s_gpuTimerActive)
	{
		glEndQuery(GL_TIME_ELAPSED);
		s_gpuTimerActive = false;
	}
#endif
}

void NativeRenderer_FinishGpuMeasurements(void)
{
#if defined(CTR_INTERNAL)
	NativeRenderer_EndGpuFrame();
	if (!s_gpuTimerSupported)
	{
		return;
	}

	NativeRenderer_ResolveGpuMeasurements(true);
	for (s32 i = 0; i < NATIVE_GPU_TIMER_QUERY_COUNT; i++)
	{
		glDeleteQueries(1, &s_gpuTimerQueries[i].id);
	}
	SDL_memset(s_gpuTimerQueries, 0, sizeof(s_gpuTimerQueries));
	s_gpuTimerSupported = false;
#endif
}

void NativeRenderer_EndScene(void)
{
	if (s_previousOffscreenState)
	{
		NativeRenderer_SetOffscreenState(&s_previousOffscreen, 0);
	}

	if (g_dbg_wireframeMode)
	{
		NativeRenderer_SetWireframe(0);
	}

	glBindVertexArray(0);
}

void NativeRenderer_ResetDevice(void)
{
	NativeRenderer_UpdatePresentationViewport();
	NativeRenderer_UpdateSwapIntervalState(0);
}

int NativeRenderer_InitialisePSX(void)
{
	SDL_memset(s_vram.cpuPixels, 0, sizeof(s_vram.cpuPixels));
	s_vram.cpuDirtyRects[0].x = 0;
	s_vram.cpuDirtyRects[0].y = 0;
	s_vram.cpuDirtyRects[0].w = VRAM_WIDTH;
	s_vram.cpuDirtyRects[0].h = VRAM_HEIGHT;
	s_vram.cpuDirtyRectCount = 1;
	SDL_memset(s_vram.gpuNewerTiles, 0, sizeof(s_vram.gpuNewerTiles));
#ifdef __vita__
	NativeRenderer_ResetP4Cache();
#endif
	NativeRenderer_InitRG8LUT();
	NativeRenderer_GenerateCommonTextures();
	NativeRenderer_InitialisePSXShaders();
	NativeRenderer_InitVRAMPipelines();

#if defined(CTR_INTERNAL)
	GLint glMajor = 0;
	GLint glMinor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glMinor);
	s_gpuTimerSupported = (glMajor > 3) || ((glMajor == 3) && (glMinor >= 3)) || SDL_GL_ExtensionSupported("GL_ARB_timer_query");
	if (s_gpuTimerSupported)
	{
		GLuint queryIds[NATIVE_GPU_TIMER_QUERY_COUNT];
		glGenQueries(NATIVE_GPU_TIMER_QUERY_COUNT, queryIds);
		for (s32 i = 0; i < NATIVE_GPU_TIMER_QUERY_COUNT; i++)
		{
			s_gpuTimerQueries[i].id = queryIds[i];
		}
	}
#endif

	glDepthFunc(GL_LEQUAL);
	s_previousDepthAlwaysPass = 0;
	glEnable(GL_STENCIL_TEST);
#ifndef __vita__
	glBlendColor(0.5f, 0.5f, 0.5f, 0.25f);

	GLint maxTextureSize = 0;
	GLint maxRenderbufferSize = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &s_maxSamples);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
	s_maxRenderTargetSize = (maxTextureSize < maxRenderbufferSize) ? maxTextureSize : maxRenderbufferSize;
	NATIVE_RENDERER_LOG("*Anti-aliasing limits: %d MSAA samples, %d max render target size\n", s_maxSamples, s_maxRenderTargetSize);
#endif

	// Main and offscreen draws share one explicit render-target contract. The
	// main target stays at CTR's logical display size; host scaling is deferred
	// to presentation.
	NativeRenderer_InitRenderTarget(&s_mainRenderTarget);
	NativeRenderer_InitRenderTarget(&s_offscreenRenderTarget);

	// gen VRAM texture (single, persistent - mirrors PS1's single 1MB VRAM)
	{
		glGenTextures(1, &s_vram.texture);

		glBindTexture(GL_TEXTURE_2D, s_vram.texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		// set storage size
		glTexImage2D(GL_TEXTURE_2D, 0, VRAM_INTERNAL_FORMAT, VRAM_WIDTH, VRAM_HEIGHT, 0, VRAM_FORMAT, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_2D, 0);

		// VRAM framebuffer for offscreen blitting to VRAM
		glGenFramebuffers(1, &s_glVramFramebuffer);
		{
			glBindFramebuffer(GL_FRAMEBUFFER, s_glVramFramebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_vram.texture, 0);
#ifndef __vita__
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
#else
			GLuint depthstencil;
			glGenRenderbuffers(1, &depthstencil);
			glBindRenderbuffer(GL_RENDERBUFFER, depthstencil);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, VRAM_WIDTH, VRAM_HEIGHT);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthstencil);
#endif

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	// gen vertex buffer and index buffer
	{
		int i;

		glGenBuffers(MAX_NUM_VERTEX_BUFFERS, s_glVertexBuffer);
		glGenVertexArrays(MAX_NUM_VERTEX_BUFFERS, s_glVertexArray);
		for (i = 0; i < MAX_NUM_VERTEX_BUFFERS; i++)
		{
			glBindVertexArray(s_glVertexArray[i]);
			glBindBuffer(GL_ARRAY_BUFFER, s_glVertexBuffer[i]);
#ifdef __vita__
			// Initialise vitaGL's VBO metadata. Each submitted batch later
			// replaces this pointer with mapped scratch storage, without a copy.
			glBufferData(GL_ARRAY_BUFFER, sizeof(GrVertex), NULL, GL_STREAM_DRAW);
#else
			glBufferData(GL_ARRAY_BUFFER, sizeof(GrVertex) * MAX_VERTEX_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);
#endif
			glEnableVertexAttribArray(a_position);
			glEnableVertexAttribArray(a_texcoord);
			glEnableVertexAttribArray(a_color);
			glEnableVertexAttribArray(a_extra);
#ifdef __vita__
			glEnableVertexAttribArray(a_order_depth);
#endif

#if NATIVE_PGXP_SUPPORTED
			glEnableVertexAttribArray(a_page_clut);
			glVertexAttribPointer(a_position, 4, GL_FLOAT, GL_FALSE, sizeof(GrVertex), &((GrVertex *)NULL)->x);
			glVertexAttribPointer(a_page_clut, 2, GL_SHORT, GL_FALSE, sizeof(GrVertex), &((GrVertex *)NULL)->page);
#else
			glVertexAttribPointer(a_position, 4, GL_SHORT, GL_FALSE, sizeof(GrVertex), &((GrVertex *)NULL)->x);
#endif
			glVertexAttribPointer(a_texcoord, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(GrVertex), &((GrVertex *)NULL)->u);
			glVertexAttribPointer(a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GrVertex), &((GrVertex *)NULL)->r);
			glVertexAttribPointer(a_extra, 4, GL_BYTE, GL_FALSE, sizeof(GrVertex), &((GrVertex *)NULL)->tcx);
#ifdef __vita__
			glVertexAttribPointer(a_order_depth, 1, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(GrVertex), &((GrVertex *)NULL)->orderDepth);
#endif
		}
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	NativeRenderer_ResetDevice();

	return 1;
}
