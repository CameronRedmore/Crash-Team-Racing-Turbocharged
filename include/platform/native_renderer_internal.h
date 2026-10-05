/*
 * Renderer-internal contract.
 *
 * platform/renderer/ splits the old single-file renderer into subsystems:
 * core (shared state), device, shaders, gpu_state, targets, vram, p4, passes,
 * textures, submit and overlays. This header is what they share. It is not
 * game API -- game code and the rest of the platform layer use
 * platform/native_renderer.h.
 *
 * The renderer is one translation unit in this build (main.c includes every
 * .c file), so declaring a variable here is enough for every module to see it.
 * That is why the shared state below is declared global_variable (static), not
 * extern: it keeps exactly the internal linkage these had in the single-file
 * renderer, so splitting the file cannot widen who may touch them. The plain
 * externs are the handful of names that other translation units share -- either
 * defined here and read elsewhere, or defined elsewhere and read here.
 * Anything used by only one module stays static in that module and is not
 * declared here at all.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#ifndef NATIVE_RENDERER_INTERNAL_H
#define NATIVE_RENDERER_INTERNAL_H

#include <macros.h>
#include <SDL3/SDL.h>

#include <platform/native_draw3d.h>
#include <platform/native_pgxp.h>
#include <platform/native_renderer.h>
#include <platform/native_renderer_types.h>

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include "platform/native_glad.h"
#endif

#ifdef __vita__
#define VRAM_FORMAT          GL_RGBA
#define VRAM_INTERNAL_FORMAT GL_RGBA
#else
#define VRAM_FORMAT          GL_RG
// NOTE(penta3): VRAM holds packed 16-bit PSX pixels as two bytes (R=low,
// G=high), uploaded as GL_RG + GL_UNSIGNED_BYTE. RG8 is the faithful storage:
// 2 bytes/texel = real PS1's 1MB VRAM, vs RG32F's 8 bytes/texel (4MB). With
// NEAREST sampling RG8 returns byte/255 exactly, identical to what the shader
// got from RG32F, so CLUT/texture-page reconstruction is unchanged.
#define VRAM_INTERNAL_FORMAT GL_RG8
#endif

#define NATIVE_RENDER_TARGET_TYPE GL_UNSIGNED_BYTE

#ifdef __vita__
#define MAX_NUM_VERTEX_BUFFERS (1)
#else
#define MAX_NUM_VERTEX_BUFFERS (2)
#endif

#define NATIVE_RENDERER_LOG(fmt, ...)   Platform_Log("[CTR Renderer] " fmt, __VA_ARGS__)
#define NATIVE_RENDERER_ERROR(fmt, ...) Platform_LogError("[CTR Renderer] [%s] - " fmt, __func__, __VA_ARGS__)

extern SDL_Window *g_window;

#ifndef __vita__
extern int gNativeDitheringEnabled;
#endif

// ---------------------------------------------------------------------------
// Types shared between renderer modules
// ---------------------------------------------------------------------------

// NOTE(penta3): Single persistent VRAM texture, matching real PS1's single
// 1MB VRAM. PS1 page-flips two windows *inside* one VRAM; it never keeps two
// full copies. The old double buffer existed only to orphan the texture on the
// per-frame full re-upload, which no longer happens (we upload dirty rects).
#define NATIVE_VRAM_DIRTY_RECT_CAP 128
#define NATIVE_VRAM_TILE_SIZE      8
#define NATIVE_VRAM_TILE_COLS      (VRAM_WIDTH / NATIVE_VRAM_TILE_SIZE)
#define NATIVE_VRAM_TILE_ROWS      (VRAM_HEIGHT / NATIVE_VRAM_TILE_SIZE)
#define NATIVE_VRAM_TILE_COUNT     (NATIVE_VRAM_TILE_COLS * NATIVE_VRAM_TILE_ROWS)
#define NATIVE_VRAM_TILE_WORDS     (NATIVE_VRAM_TILE_COUNT / 32)

// NOTE(aalhendi): Native splits PS1 VRAM between a CPU mirror and one packed
// GPU texture. CPU writes are uploaded before GPU reads; GPU-newer tiles are
// resolved into the CPU mirror only when game code reads those VRAM regions.
struct NativeVramState
{
	TextureID texture;
	u16 cpuPixels[VRAM_WIDTH * VRAM_HEIGHT];
	RECT16 cpuDirtyRects[NATIVE_VRAM_DIRTY_RECT_CAP];
	u32 gpuNewerTiles[NATIVE_VRAM_TILE_WORDS];
	s32 cpuDirtyRectCount;
};

struct NativeRenderTarget
{
	TextureID texture;
	GLuint framebuffer;
	GLuint stencilBuffer;
	s32 width;
	s32 height;
	s32 logicalWidth;
	s32 logicalHeight;
#ifndef __vita__
	// MSAA: draws go to the multisampled framebuffer, which is resolved into
	// texture/framebuffer above before anything samples the target.
#if NATIVE_DRAW3D_SUPPORTED
	GLuint isolatedFramebuffer;
	GLuint isolatedDepthStencilBuffer;
	s32 isolatedWidth, isolatedHeight, isolatedSamples;
#endif
	GLuint msaaFramebuffer;
	GLuint msaaColorBuffer;
	GLuint msaaDepthStencilBuffer;
	s32 samples;
	s32 msaaWidth;
	s32 msaaHeight;
#endif
};

typedef struct
{
	// shader itself
	ShaderID shader;

	GLint projectionLoc;
	GLint bilinearFilterLoc;
	GLint texelSizeLoc;
	GLint texLoc;
	GLint lutLoc;
#ifndef __vita__
	GLint psxSemiTransPassLoc;
	GLint psxDitherEnabledLoc;
	GLint psxColorDepth15Loc;
#endif
#if NATIVE_DRAW3D_SUPPORTED
	GLint objectToCameraLoc;
	GLint nativeViewLoc;
#endif
	GLint psxDrawMaskSetLoc;
	GLint psxTextureOutputStpLoc;
} GTEShader;

// Render state cached across the PSX submit run. Native full-screen passes
// (VRAM pack, pause backdrop, SSAA resolve) save it before drawing and restore
// it afterwards so the submit run continues unaffected.
struct NativeRendererPassState
{
	ShaderID shader;
	TextureID texture;
	BlendMode blendMode;
	int mixedSTPBlend;
	int depthMode;
	int depthWrite;
	int scissorState;
	GLboolean stencilEnabled;
};

// ---------------------------------------------------------------------------
// Shared state, grouped by owning module. Everything here is defined in
// native_renderer_core.c; state that belongs to one module is declared in that
// module instead and does not appear in this list.
// global_variable here means the same object the single-file renderer had; it
// is not visible outside this translation unit.
// ---------------------------------------------------------------------------

// Renderer-owned knobs that other translation units read. These are defined in
// native_renderer_core.c and keep external linkage because platform code and the
// option tests reach them by name.
extern int g_windowWidth;
extern int g_windowHeight;
extern int g_dbg_wireframeMode;
extern int g_dbg_texturelessMode;
extern int g_cfg_bilinearFiltering;

// Defined outside the renderer and read by it.
extern SDL_Window *g_window;

// gpu_state: last value pushed to GL, so repeated state changes stay cheap.
// Invalidation goes through the Invalidate* helpers below rather than a direct
// write, but three modules still read and restore these fields themselves --
// native_renderer_targets.c around LoadRenderTargetFromVRAM, native_renderer_vram.c
// around RestoreVRAMState, and native_renderer_device.c for the always-pass depth
// knob -- so they are not yet private to gpu_state. The scissor-rect and stencil
// caches, which nothing else touches, are.
global_variable BlendMode s_previousBlendMode;
global_variable int s_previousMixedSTPBlend;
global_variable int s_previousDepthMode;
global_variable int s_previousDepthWrite;
global_variable int s_previousDepthAlwaysPass;
global_variable int s_previousScissorState;
global_variable int s_previousOffscreenState;
global_variable RECT16 s_previousOffscreen;
global_variable ShaderID s_previousShader;

// vram: the CPU mirror of PSX VRAM. Vita's transfer scratch buffer lives in
// native_renderer_vram.c, which is its only user.
global_variable struct NativeVramState s_vram;

// targets: presentation size and aspect.
global_variable struct NativeRenderTarget s_mainRenderTarget;
global_variable struct NativeRenderTarget s_offscreenRenderTarget;
global_variable int s_startupAspectW;
global_variable int s_startupAspectH;
global_variable SDL_Rect s_presentViewport;
global_variable int s_gamePresentationEnabled;

// Desktop only: the anti-aliasing mode latched for the frame, the driver's
// MSAA limits, the presentation size SSAA resolves to, and the persistent
// projected-world target. Vita keeps Classic and has no MSAA/SSAA path, so
// none of this exists there.
#ifndef __vita__
global_variable int s_frameAntiAliasingMode; // latched at BeginScene
global_variable b32 s_framePs1Resolution;    // latched at BeginScene
global_variable GLint s_maxSamples;
global_variable GLint s_maxRenderTargetSize;
global_variable s32 s_mainResolveWidth;
global_variable s32 s_mainResolveHeight;
global_variable struct NativeRenderTarget s_projectedWorldTarget;
global_variable b32 s_projectedWorldTargetReady;
global_variable b32 s_projectedWorldBound;
#endif

// textures: the fixed textures every scene starts with.
global_variable TextureID s_whiteTexture;
global_variable TextureID s_rgLutTexture;
#ifdef __vita__
global_variable TextureID s_presentLutTexture;
#endif

// The bound-texture half of the gpu_state cache, listed here next to the rest
// of that cache's state; see the gpu_state note above.
global_variable TextureID s_lastBoundTexture;

// shaders: the 15-bit colour lookup table shared with the LUT texture.
global_variable u8 rgLUT[LUT_WIDTH * LUT_HEIGHT * sizeof(u32)];

// gpu_state: the full-screen blit pipelines and their uniform locations,
// compiled by native_renderer_shaders.c and driven by whoever runs a pass.
global_variable GLuint s_packShader;
global_variable GLint s_packFlipYLoc;
#ifndef __vita__
global_variable GLuint s_pauseBackgroundShader;
global_variable GLint s_pauseBackgroundFlipYLoc;
global_variable GLint s_pauseBackgroundPaletteLoc;
global_variable GLint s_pauseBackgroundSmoothLoc;
global_variable GLuint s_projectedWorldShader;
global_variable GLint s_projectedWorldRectLoc;
global_variable GLint s_projectedWorldTexelLoc;
global_variable GLint s_projectedWorldModeLoc;
global_variable GLint s_projectedWorldStrengthLoc;
global_variable GLint s_projectedWorldTanHalfLoc;
global_variable GLint s_projectedWorldOverscanLoc;
global_variable GLint s_projectedWorldEndpointLoc;
#endif
global_variable GLuint s_presentVramShader;
global_variable GLint s_presentVramSourceRectLoc;
global_variable GLuint s_presentRgbaShader;
global_variable GLint s_presentRgbaFlipYLoc;
#ifndef __vita__
global_variable GLint s_presentRgbaTexelSizeLoc;
global_variable GLint s_presentRgbaFxaaLoc;
global_variable GLint s_presentRgbaSharpPrescaleLoc;
global_variable GLuint s_downsampleShader;
global_variable GLint s_downsampleSrcSizeLoc;
global_variable GLint s_downsampleDstSizeLoc;
#endif
global_variable GLuint s_vramQuadVAO;
global_variable GLuint s_vramQuadVBO;

// submit / device: the PSX vertex buffers and the VRAM staging framebuffer.
global_variable GLuint s_glVertexArray[MAX_NUM_VERTEX_BUFFERS];
global_variable GLuint s_glVertexBuffer[MAX_NUM_VERTEX_BUFFERS];
global_variable int s_boundVertexBuffer;
global_variable GLuint s_glVramFramebuffer;
// device: GrVertex attribute layout for the bound vertex array and buffer.
internal void NativeRenderer_SetupVertexAttributes(void);

// ---------------------------------------------------------------------------
// Cross-module operations
// ---------------------------------------------------------------------------

// native_renderer_targets.c -- render targets, MSAA/SSAA, presentation.
internal void NativeRenderer_DestroySceneTargets(void);
internal void NativeRenderer_InitRenderTarget(struct NativeRenderTarget *target);
internal void NativeRenderer_DestroyRenderTarget(struct NativeRenderTarget *target);
internal void NativeRenderer_EnsureRenderTarget(struct NativeRenderTarget *target, int width, int height);
internal GLuint NativeRenderer_GetDrawFramebuffer(const struct NativeRenderTarget *target);
internal void NativeRenderer_BindMainRenderTarget(void);
internal void NativeRenderer_LoadRenderTargetFromVRAM(struct NativeRenderTarget *target, int x, int y, int logicalWidth, int logicalHeight);
internal void NativeRenderer_DrawVRAMRegion(int x, int y, int width, int height);
internal void NativeRenderer_SetPresentationAspect(int width, int height);
internal void NativeRenderer_UpdatePresentationViewport(void);
internal void NativeRenderer_UpdateGamePresentationAspect(void);
internal void NativeRenderer_ClearPresentationBars(void);

// MSAA and supersample resolve exist only where the renderer owns a
// multisampled main target; Vita has neither, so neither do these.
#ifndef __vita__
internal void NativeRenderer_EnsureMultisampleStorage(struct NativeRenderTarget *target, int samples);
internal void NativeRenderer_ResolveMultisample(const struct NativeRenderTarget *target);
internal const struct NativeRenderTarget *NativeRenderer_ResolveMainRenderTarget(void);
internal float NativeRenderer_SupersampleScale(int mode);
#endif

// native_renderer_shaders.c -- shader objects and the blit pipelines.
internal void NativeRenderer_DestroyPSXShaders(void);
internal void NativeRenderer_InitialisePSXShaders(void);
internal void NativeRenderer_InitVRAMPipelines(void);
internal void NativeRenderer_InitRG8LUT(void);

// native_renderer_gpu_state.c -- cached GL state.
// Invalidation entry points: how the rest of the renderer drops a cache when it
// changes GL state behind a setter's back. The scissor-rect and stencil caches
// are private to that module; the rest are still read directly by targets, vram
// and device, as noted where they are declared.
internal void NativeRenderer_InvalidateTextureBinding(void);
internal void NativeRenderer_InvalidateTextureBindingIfCurrent(TextureID texture);
internal void NativeRenderer_InvalidateBindingCache(void);
internal void NativeRenderer_ResetTextureBinding(void);
internal void NativeRenderer_InvalidateScissorRectCache(void);
internal void NativeRenderer_SetScissorState(int enable);
internal void NativeRenderer_EnableDepth(int enable);
internal void NativeRenderer_SetViewPort(int x, int y, int width, int height);
internal void NativeRenderer_SetWireframe(int enable);
// The save/restore that wraps a full-screen pass lives with the state it saves.
internal void NativeRenderer_BeginUtilityPass(struct NativeRendererPassState *state, GLuint framebuffer, int x, int y, int width, int height);
internal void NativeRenderer_EndUtilityPass(const struct NativeRendererPassState *state);

// native_renderer_vram.c -- CPU mirror and its dirty tracking.
internal int NativeRenderer_ClipVRAMRect(RECT16 *out, int x, int y, int w, int h);
internal void NativeRenderer_MarkVRAMDirty(int x, int y, int w, int h);
internal void NativeRenderer_MarkGpuVRAMNewer(int x, int y, int w, int h);
internal void NativeRenderer_FlushOffscreenToVRAM(void);
internal int NativeRenderer_RectEquals(const RECT16 *a, const RECT16 *b);
internal void NativeRenderer_SyncGpuVRAMToCPU(int x, int y, int w, int h);
internal void NativeRenderer_ResolveVRAMRead(int x, int y, int w, int h);
#ifdef __vita__
internal int NativeRenderer_HasGpuNewerVRAMTiles(int x, int y, int w, int h);
internal int NativeRenderer_VRAMRectsOverlap(const RECT16 *a, const RECT16 *b);
#endif

// native_renderer_p4.c -- Vita texture-page and palette cache.
#ifdef __vita__
internal void NativeRenderer_InvalidateP4CacheRect(const RECT16 *rect);
internal void NativeRenderer_InvalidatePaletteCacheRows(const RECT16 *rect);
internal void NativeRenderer_ResetP4Cache(void);
internal void NativeRenderer_BeginP4Frame(void);
internal void NativeRenderer_DestroyP4Textures(void);
#endif

// native_renderer_passes.c -- full-screen offscreen passes. The state
// save/restore around them is in native_renderer_gpu_state.c, listed above.
internal void NativeRenderer_GpuPackTextureToVRAM(TextureID sourceTexture, int x, int y, int w, int h, b32 flipY);
#ifndef __vita__
internal void NativeRenderer_DestroyPassTargets(void);
internal void NativeRenderer_BeginPassFrame(void);
#endif

// native_renderer_textures.c -- texture objects and PSX colour helpers.
internal void NativeRenderer_DestroyTexture(TextureID texture);
// Shared texture factory for the overlay, minimap and font-atlas textures.
internal TextureID NativeRenderer_CreateGhostReplayTexture(int width, int height, const u8 *pixels);
internal void NativeRenderer_GenerateCommonTextures(void);
internal u16 NativeRenderer_PackRGB24ToPSX15(u8 r, u8 g, u8 b);
internal float NativeRenderer_PSXColorComponentFloat(u8 value);

// native_renderer_overlays.c -- renderer-drawn screen overlays.
internal void NativeRenderer_DestroyGhostReplayTextures(void);

#endif
