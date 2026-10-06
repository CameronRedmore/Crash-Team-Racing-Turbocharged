#ifndef NATIVE_RENDERER_H
#define NATIVE_RENDERER_H

#include <platform/native_renderer_types.h>
#include <platform/native_projection.h>

int NativeRenderer_InitialiseRender(char *windowName, int width, int height, int fullscreen);
int NativeRenderer_InitialisePSX(void);
void NativeRenderer_Shutdown(void);
void NativeRenderer_ResetDevice(void);
// Opt the main game presentation into NativeAspect's selected ratio. Kept
// explicit so renderer integration tests and utility windows retain the
// dimensions passed to NativeRenderer_InitialiseRender.
void NativeRenderer_EnableGamePresentation(int enabled);
void NativeRenderer_BeginScene(void);
void NativeRenderer_EndScene(void);
void NativeRenderer_EndGpuFrame(void);
void NativeRenderer_FinishGpuMeasurements(void);
void NativeRenderer_UpdateSwapIntervalState(int swapInterval);
void NativeRenderer_SwapWindow(void);
void NativeRenderer_StoreFrameBuffer(int x, int y, int w, int h);
#ifndef __vita__
int NativeRenderer_CapturePauseBackground(const u16 *bgr555Palette16, int smooth);
TextureID NativeRenderer_GetPauseBackgroundTexture(void);
u32 NativeRenderer_CreateFontAtlasTexture(int width, int height, const u8 *pixels);
u32 NativeRenderer_CreateMinimapTexture(int width, int height, const u8 *pixels);
void NativeRenderer_DestroyFontAtlasTexture(u32 texture);
#endif
void NativeRenderer_PresentMainRenderTarget(void);
// Compute the viewport used by streaming/video frames without changing GL state.
void NativeRenderer_GetStreamingViewport(int contentHeight, int displayHeight, int *x, int *y, int *width, int *height);
// Route onscreen world draws through a persistent colour/depth target. Resolve
// each logical camera rectangle once at its world/UI boundary.
int NativeRenderer_BindProjectedWorld(int enable);
void NativeRenderer_ResolveProjectedWorld(const RECT16 *cameraRect, const NativeProjectionParams *params);
void NativeRenderer_DrawGhostReplayOverlay(void);
#ifndef __vita__
// F6 debug overlay (settings and resolution readout).
extern int gNativeDebugOverlayEnabled;
void NativeRenderer_DrawDebugOverlayFrame(void);
// Black screen with a centred title, detail line and 0-100 progress bar, presented immediately.
// Safe mid-frame: restores the GL state the scene was using.
void NativeRenderer_ShowBusyMessage(const char *title, const char *detail, int percent);
#endif
void NativeRenderer_PresentVRAMDisplay(void);
void NativeRenderer_PresentVRAMRect(int x, int y, int w, int h);
void NativeRenderer_PresentStreamingTexture(TextureID texture, int contentHeight, int displayHeight);
void NativeRenderer_SaveVRAM(const char *outputFileName, int x, int y, int width, int height, int readFromFramebuffer);
void NativeRenderer_Clear(int x, int y, int w, int h, u8 r, u8 g, u8 b);
void NativeRenderer_ClearVRAM(int x, int y, int w, int h, u8 r, u8 g, u8 b);
void NativeRenderer_CopyVRAM(u16 *src, int x, int y, int w, int h, int dstX, int dstY);
void NativeRenderer_ReadVRAM(u16 *dst, int x, int y, int dstW, int dstH);
void NativeRenderer_SyncVRAMToCPU(int x, int y, int w, int h);
void NativeRenderer_UpdateVRAM(void);
int NativeRenderer_GetVRAMStateSize(void);
int NativeRenderer_CaptureVRAMState(void *dst, int dstSize);
int NativeRenderer_RestoreVRAMState(const void *src, int srcSize);
TextureID NativeRenderer_GetVRAMTexture(void);
TextureID NativeRenderer_GetWhiteTexture(void);
#ifdef __vita__
#define NATIVE_PALETTE_HAS_TRANSPARENT 0x1
#define NATIVE_PALETTE_HAS_OPAQUE      0x2
#define NATIVE_PALETTE_HAS_STP         0x4
int NativeRenderer_GetPaletteProperties(TexFormat format, int clut);
TextureID NativeRenderer_GetCachedP4Texture(int page, int clut, int superTurboTint);
#endif
TextureID NativeRenderer_CreateStreamingTexture(int width, int height);
void NativeRenderer_UpdateStreamingTexture(TextureID texture, int width, int height, const u8 *rgbaPixels);
void NativeRenderer_DestroyStreamingTexture(TextureID texture);
void NativeRenderer_SetBlendMode(BlendMode blendMode);
void NativeRenderer_SetMixedSTPBlendMode(BlendMode blendMode);
void NativeRenderer_SetDepthState(int enable, int write);
void NativeRenderer_SetDepthAlwaysPass(int alwaysPass);
#if NATIVE_DRAW3D_SUPPORTED
// Share colour and stencil with the current target, use fresh private depth.
int NativeRenderer_BeginIsolatedDepth(const float bounds[4], int logicalWidth, int logicalHeight);
void NativeRenderer_EndIsolatedDepth(void);
#endif
void NativeRenderer_SetStencilMode(int drawPrim);
void NativeRenderer_SetOffscreenState(const RECT16 *offscreenRect, int enable);
void NativeRenderer_SetProjection(const RECT16 *drawRect, const DISPENV *displayEnv, int offscreen);
void NativeRenderer_SetupClipMode(const RECT16 *clipRect, const DISPENV *displayEnv, int enable);
void NativeRenderer_SetTexture(TextureID texture, TexFormat texFormat, int semiTransPass, BlendMode blendMode, int textured, int superTurboTint,
                               int textureFullyOpaque, int cachedP4);
void NativeRenderer_SetOverrideTextureSize(int width, int height);
void NativeRenderer_SetPSXTextureOutputSTP(int enabled);
void NativeRenderer_SetPSXDrawMaskSet(int maskSet);
GrVertex *NativeRenderer_AllocateVertexBuffer(int count);
void NativeRenderer_UpdateVertexBuffer(const GrVertex *vertices, int count);
void NativeRenderer_DrawTriangles(int startVertex, int triangles);
#if NATIVE_DRAW3D_SUPPORTED
void NativeRenderer_DrawObjectTriangles(int startVertex, int triangles, u32 cullMode);
void NativeRenderer_SetObjectGeometry(const NativeDraw3DTransform *transform, const float *view);
// Replaces the static geometry buffer; NULL/0 releases it.
void NativeRenderer_UploadStaticVertices(const GrVertex *vertices, int count);
void NativeRenderer_DrawStaticObjectTriangles(const s32 *firstVertex, const s32 *vertexCount, int draws, u32 cullMode);
#endif
#ifndef __vita__
void NativeRenderer_GetUploadCounts(u32 *vertices, u32 *uploads);
void NativeRenderer_BeginUploadFrame(void);
#endif
void NativeRenderer_PushDebugLabel(const char *label);
void NativeRenderer_PopDebugLabel(void);

#endif
