/*
 * Shared renderer state.
 *
 * The renderer used to be one translation unit, so every subsystem reached
 * straight into whatever globals it needed. Splitting it means naming that
 * sharing instead of relying on textual order: the declarations live in
 * platform/native_renderer_internal.h, grouped by the module that owns each
 * one, and this file owns the definitions.
 *
 * They stay global_variable (static), exactly as they were in the single file.
 * Because the build is a unity build, a static definition here is still visible
 * to every renderer module through the header, so the split neither widens nor
 * narrows who can read or write them.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>

#include "platform/native_renderer_internal.h"

global_variable BlendMode s_previousBlendMode = BM_NONE;
global_variable int s_previousMixedSTPBlend = 0;
global_variable int s_previousDepthMode = 0;
global_variable int s_previousDepthWrite = 1;
global_variable int s_previousDepthAlwaysPass = 0;
global_variable int s_previousStencilMode = 0;
global_variable int s_previousScissorState = 0;
global_variable int s_previousScissorRectValid = 0;
global_variable int s_previousScissorX = 0;
global_variable int s_previousScissorY = 0;
global_variable int s_previousScissorW = 0;
global_variable int s_previousScissorH = 0;
global_variable int s_previousOffscreenState = 0;
global_variable RECT16 s_previousOffscreen = {0, 0, 0, 0};

global_variable ShaderID s_previousShader = (ShaderID)-1;


global_variable TextureID s_rgLutTexture = (TextureID)-1;
#ifdef __vita__
global_variable TextureID s_presentLutTexture = (TextureID)-1;
#endif

global_variable struct NativeVramState s_vram;

#ifdef __vita__
global_variable u8 s_vitaVramTransferPixels[VRAM_WIDTH * VRAM_HEIGHT * 4];
#endif


global_variable struct NativeRenderTarget s_mainRenderTarget;
global_variable struct NativeRenderTarget s_offscreenRenderTarget;
#ifndef __vita__
global_variable struct NativeRenderTarget s_projectedWorldTarget;
global_variable b32 s_projectedWorldTargetReady = false;
global_variable b32 s_projectedWorldBound = false;
// Full-resolution greyscale copy of the main target, used as the pause backdrop.
global_variable struct NativeRenderTarget s_pauseBackgroundTarget;
global_variable b32 s_pauseBackgroundTargetReady = false;

// SSAA renders the main target above presentation resolution and box-filters
// it down into this target. Width/height below are the presentation size.
global_variable struct NativeRenderTarget s_supersampleResolveTarget;
global_variable s32 s_mainResolveWidth;
global_variable s32 s_mainResolveHeight;

// Latched at BeginScene so a menu change cannot resize the target mid-frame.
global_variable int s_frameAntiAliasingMode = NATIVE_AA_OFF;
global_variable GLint s_maxSamples = 0;
global_variable GLint s_maxRenderTargetSize = 4096;
#endif

global_variable TextureID s_whiteTexture = (TextureID)-1;
global_variable TextureID s_lastBoundTexture = (TextureID)-1;


int g_windowWidth = 0;
int g_windowHeight = 0;

global_variable int s_presentAspectW = 4;
global_variable int s_presentAspectH = 3;
global_variable int s_startupAspectW = 4;
global_variable int s_startupAspectH = 3;
global_variable SDL_Rect s_presentViewport = {0, 0, 0, 0};
// Generic renderer users (including the renderer tests) retain their startup
// aspect until the game explicitly opts into the player-selected ratio.
global_variable int s_gamePresentationEnabled = 0;

int g_dbg_wireframeMode = 0;
int g_dbg_texturelessMode = 0;

int g_cfg_bilinearFiltering = 0;

// NOTE(aalhendi): Pack native RGBA render targets into the persistent RG8 VRAM
// texture on the GPU instead of a GPU-to-CPU-to-GPU round trip.
global_variable GLuint s_packShader = 0;
global_variable GLint s_packFlipYLoc = -1;
#ifndef __vita__
global_variable GLuint s_pauseBackgroundShader = 0;
global_variable GLint s_pauseBackgroundFlipYLoc = -1;
global_variable GLint s_pauseBackgroundPaletteLoc = -1;
global_variable GLint s_pauseBackgroundSmoothLoc = -1;
global_variable GLuint s_projectedWorldShader = 0;
global_variable GLint s_projectedWorldSourceLoc = -1;
global_variable GLint s_projectedWorldRectLoc = -1;
global_variable GLint s_projectedWorldTexelLoc = -1;
global_variable GLint s_projectedWorldModeLoc = -1;
global_variable GLint s_projectedWorldStrengthLoc = -1;
global_variable GLint s_projectedWorldTanHalfLoc = -1;
global_variable GLint s_projectedWorldOverscanLoc = -1;
global_variable GLint s_projectedWorldEndpointLoc = -1;
#endif
global_variable GLuint s_presentVramShader = 0;
global_variable GLint s_presentVramSourceRectLoc = -1;
global_variable GLuint s_presentRgbaShader = 0;
global_variable GLint s_presentRgbaFlipYLoc = -1;
#ifndef __vita__
global_variable GLint s_presentRgbaTexelSizeLoc = -1;
global_variable GLint s_presentRgbaFxaaLoc = -1;
global_variable GLuint s_downsampleShader = 0;
global_variable GLint s_downsampleSrcSizeLoc = -1;
global_variable GLint s_downsampleDstSizeLoc = -1;
#endif
global_variable GLuint s_vramQuadVAO = 0;
global_variable GLuint s_vramQuadVBO = 0;


global_variable GLuint s_glVertexArray[MAX_NUM_VERTEX_BUFFERS];
global_variable GLuint s_glVertexBuffer[MAX_NUM_VERTEX_BUFFERS];
global_variable int s_curVertexBuffer = 0;
global_variable int s_boundVertexBuffer = -1;

global_variable GLuint s_glVramFramebuffer;


//----------------------------------------------------------------------------------------

global_variable u8 rgLUT[LUT_WIDTH * LUT_HEIGHT * sizeof(u32)];
