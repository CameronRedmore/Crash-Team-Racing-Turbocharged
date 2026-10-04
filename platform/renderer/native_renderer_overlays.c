/*
 * Screen overlays drawn by the renderer itself: the F6 debug overlay and the
 * ghost-replay controller overlay.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>
#include <string.h>

#include "platform/native_adhoc.h"
#include "platform/native_assets.h"
#include "platform/native_debug_font.h"
#include "platform/native_engine.h"
#include "platform/native_font.h"
#include "platform/native_kart_color.h"
#include "platform/native_log.h"
#include "platform/native_options.h"
#include "platform/native_perf.h"
#include "platform/native_pgxp.h"
#include "platform/native_renderer_internal.h"

#ifdef __vita__
#include <png.h>
#else
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
// STB_IMAGE_STATIC leaves the loaders we don't call as unused statics.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../externals/SDL/src/video/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

global_variable TextureID s_ghostReplayControllerTexture;
global_variable TextureID s_ghostReplayHighlightTexture;
#ifdef __vita__
global_variable TextureID s_ghostReplayShoulderTexture[2];
#endif
global_variable int s_ghostReplayControllerWidth;
global_variable int s_ghostReplayControllerHeight;
global_variable b32 s_ghostReplayOverlayLoadAttempted;


internal b32 NativeRenderer_LoadGhostReplayOverlay(void)
{
	u8 *pixels = NULL;
	u8 highlight[32 * 32 * 4];
#ifdef __vita__
	u8 shoulder[2][48 * 16 * 4];
#endif

	if (s_ghostReplayOverlayLoadAttempted)
	{
#ifdef __vita__
		return (s_ghostReplayControllerTexture != 0) && (s_ghostReplayHighlightTexture != 0) && (s_ghostReplayShoulderTexture[0] != 0) &&
		       (s_ghostReplayShoulderTexture[1] != 0);
#else
		return (s_ghostReplayControllerTexture != 0) && (s_ghostReplayHighlightTexture != 0);
#endif
	}
	s_ghostReplayOverlayLoadAttempted = true;

#ifdef __vita__
	png_image image;
	memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&image, "app0:/vita.png"))
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to load app0:/vita.png for Ghost Replay overlay");
		return false;
	}

	image.format = PNG_FORMAT_RGBA;
	pixels = (u8 *)malloc(PNG_IMAGE_SIZE(image));
	if ((pixels == NULL) || !png_image_finish_read(&image, NULL, pixels, 0, NULL))
	{
		free(pixels);
		png_image_free(&image);
		NATIVE_RENDERER_ERROR("%s\n", "Failed to decode app0:/vita.png for Ghost Replay overlay");
		return false;
	}
	s_ghostReplayControllerWidth = (int)image.width;
	s_ghostReplayControllerHeight = (int)image.height;
#else
	struct NativeAssetsByteBuffer overlayBytes = {0};
	int imageWidth = 0;
	int imageHeight = 0;
	int imageChannels = 0;
	if (!NativeAssets_ReadBytes("dualshock.png", NATIVE_ASSET_READ_DATA_FILE, &overlayBytes))
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to load assets/dualshock.png for Ghost Replay overlay");
		return false;
	}
	pixels = (u8 *)stbi_load_from_memory(overlayBytes.data, overlayBytes.size, &imageWidth, &imageHeight, &imageChannels, 4);
	NativeAssets_FreeBytes(&overlayBytes);
	if (pixels == NULL)
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to decode assets/dualshock.png for Ghost Replay overlay");
		return false;
	}
	s_ghostReplayControllerWidth = imageWidth;
	s_ghostReplayControllerHeight = imageHeight;
#endif
	s_ghostReplayControllerTexture = NativeRenderer_CreateGhostReplayTexture(s_ghostReplayControllerWidth, s_ghostReplayControllerHeight, pixels);

	for (int y = 0; y < 32; y++)
	{
		for (int x = 0; x < 32; x++)
		{
			const int dx = (x * 2) - 31;
			const int dy = (y * 2) - 31;
			const int dist2 = dx * dx + dy * dy;
			const int pixel = (y * 32 + x) * 4;
			highlight[pixel + 0] = 255;
			highlight[pixel + 1] = 128;
			highlight[pixel + 2] = 0;
			highlight[pixel + 3] = (dist2 <= 729) ? 190 : ((dist2 <= 961) ? 100 : 0);
		}
	}
	s_ghostReplayHighlightTexture = NativeRenderer_CreateGhostReplayTexture(32, 32, highlight);

#ifdef __vita__
	const int shoulderImageX[2] = {16, 237};
	for (int side = 0; side < 2; side++)
	{
		for (int y = 0; y < 16; y++)
		{
			for (int x = 0; x < 48; x++)
			{
				const int sourceX = shoulderImageX[side] + x;
				const int sourcePixel = (y * s_ghostReplayControllerWidth + sourceX) * 4;
				const int dstPixel = (y * 48 + x) * 4;
				const u8 sourceAlpha = pixels[sourcePixel + 3];
				const int luminance = ((int)pixels[sourcePixel + 0] + (int)pixels[sourcePixel + 1] + (int)pixels[sourcePixel + 2]) / 3;
				const u8 maskAlpha = (sourceAlpha > 32 && luminance > 70) ? (u8)((sourceAlpha * 220) / 255) : 0;

				shoulder[side][dstPixel + 0] = 255;
				shoulder[side][dstPixel + 1] = 128;
				shoulder[side][dstPixel + 2] = 0;
				shoulder[side][dstPixel + 3] = maskAlpha;
			}
		}
		s_ghostReplayShoulderTexture[side] = NativeRenderer_CreateGhostReplayTexture(48, 16, shoulder[side]);
	}
#endif

#ifdef __vita__
	free(pixels);
	png_image_free(&image);
#else
	stbi_image_free(pixels);
#endif

#ifdef __vita__
	return (s_ghostReplayControllerTexture != 0) && (s_ghostReplayHighlightTexture != 0) && (s_ghostReplayShoulderTexture[0] != 0) &&
	       (s_ghostReplayShoulderTexture[1] != 0);
#else
	return (s_ghostReplayControllerTexture != 0) && (s_ghostReplayHighlightTexture != 0);
#endif
}

internal void NativeRenderer_DrawGhostReplayQuad(TextureID texture, int x, int y, int width, int height)
{
	if ((texture == 0) || (width <= 0) || (height <= 0))
	{
		return;
	}

	NativeRenderer_SetViewPort(x, y, width, height);
	glUseProgram(s_presentRgbaShader);
	glUniform1f(s_presentRgbaFlipYLoc, 1.0f);
#ifndef __vita__
	if (s_presentRgbaFxaaLoc >= 0)
	{
		glUniform1i(s_presentRgbaFxaaLoc, 0);
	}
#endif
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);
}

internal void NativeRenderer_DrawGhostReplayHighlight(int overlayX, int overlayY, int overlayW, int overlayH, int imageX, int imageY, int width, int height)
{
	const int centerX = overlayX + (imageX * overlayW) / s_ghostReplayControllerWidth;
	const int centerY = overlayY + overlayH - (imageY * overlayH) / s_ghostReplayControllerHeight;
	const int scaledW = (width * overlayW) / s_ghostReplayControllerWidth;
	const int scaledH = (height * overlayH) / s_ghostReplayControllerHeight;
	NativeRenderer_DrawGhostReplayQuad(s_ghostReplayHighlightTexture, centerX - scaledW / 2, centerY - scaledH / 2, scaledW, scaledH);
}

#ifdef __vita__
internal void NativeRenderer_DrawGhostReplayImageRegion(TextureID texture, int overlayX, int overlayY, int overlayW, int overlayH, int imageX, int imageY,
                                                        int imageW, int imageH)
{
	const int x = overlayX + (imageX * overlayW) / s_ghostReplayControllerWidth;
	const int y = overlayY + overlayH - ((imageY + imageH) * overlayH) / s_ghostReplayControllerHeight;
	const int width = (imageW * overlayW) / s_ghostReplayControllerWidth;
	const int height = (imageH * overlayH) / s_ghostReplayControllerHeight;
	NativeRenderer_DrawGhostReplayQuad(texture, x, y, width, height);
}
#endif

#ifndef __vita__
// F6 debug overlay: a compact text readout of every renderer / enhancement
// option, rendered on the CPU into a small RGBA texture and drawn over the frame.
int gNativeDebugOverlayEnabled = 0;

extern int gNativeMirrorModeEnabled;
extern int gNativeDefaultCameraFar;
extern int gNativeDefaultHudSpeedometer;
extern int gNativeAIRacersMode;
extern int gNativeMaxLodEnabled;
extern int gNativeBorderlessEnabled;
extern int g_cfg_bilinearFiltering;
extern u32 gNativeCheatConfigMask;
#ifdef CTR_INTERNAL
extern int g_dbg_wireframeMode;
extern int g_dbg_texturelessMode;
#endif

#define NATIVE_DEBUG_OVERLAY_MAX_LINES  40
#define NATIVE_DEBUG_OVERLAY_LINE_CHARS 64
#define NATIVE_DEBUG_OVERLAY_GLYPH      8
#define NATIVE_DEBUG_OVERLAY_PAD        3

global_variable TextureID s_debugOverlayTexture = 0;
global_variable int s_debugOverlayTextureW = 0;
global_variable int s_debugOverlayTextureH = 0;
global_variable u64 s_debugOverlayLastCounter = 0;
global_variable float s_debugOverlayFps = 0.0f;

internal const char *NativeRenderer_DebugOnOff(int value)
{
	return value ? "ON" : "off";
}

internal int NativeRenderer_BuildDebugOverlayLines(char lines[][NATIVE_DEBUG_OVERLAY_LINE_CHARS])
{
	static const char *const aaNames[NATIVE_AA_MODE_COUNT] = {"Off", "FXAA", "MSAA 2x", "MSAA 4x", "MSAA 8x", "SSAA 2x", "SSAA 4x"};
	static const char *const pgxpNames[NATIVE_PGXP_MODE_COUNT] = {"Off", "Geometry", "Perspective"};
	static const char *const aiNames[NATIVE_AI_RACERS_MODE_COUNT] = {"Retail", "Extended", "Extended+Custom"};
	static const char *const pauseNames[3] = {"Retail", "HD posterised", "HD smooth"};
	int n = 0;
#define DBG_LINE(...)                                                           \
	do                                                                          \
	{                                                                           \
		if (n < NATIVE_DEBUG_OVERLAY_MAX_LINES)                                 \
			snprintf(lines[n++], NATIVE_DEBUG_OVERLAY_LINE_CHARS, __VA_ARGS__); \
	} while (0)

	const int aaConfigured = gNativeAntiAliasingMode;
	const int aaFrame = s_frameAntiAliasingMode;
	DBG_LINE("CTR DEBUG [F6]  %.1f fps", s_debugOverlayFps);
	DBG_LINE("-- Player 1 --");
	DBG_LINE("Engine %s", NativeEngine_GetProfileName(NativeEngine_GetEffectiveProfile(0)));
	DBG_LINE("Kart hue rotation %d deg", gNativeKartHue * NATIVE_KART_HUE_STEP_DEGREES);
	DBG_LINE("-- Resolution --");
	DBG_LINE("Window  %dx%d", g_windowWidth, g_windowHeight);
	DBG_LINE("Present %dx%d @%d,%d", s_presentViewport.w, s_presentViewport.h, s_presentViewport.x, s_presentViewport.y);
	DBG_LINE("Render  %dx%d (logical %dx%d)", s_mainRenderTarget.width, s_mainRenderTarget.height, s_mainRenderTarget.logicalWidth,
	         s_mainRenderTarget.logicalHeight);
	DBG_LINE("Resolve %dx%d  maxRT %d", s_mainResolveWidth, s_mainResolveHeight, s_maxRenderTargetSize);
	DBG_LINE("Samples %d  SS scale %.2f", (int)s_mainRenderTarget.samples, NativeRenderer_SupersampleScale(aaFrame));
	DBG_LINE("-- Anti-aliasing --");
	DBG_LINE("AA  %s%s%s", ((aaConfigured >= 0) && (aaConfigured < NATIVE_AA_MODE_COUNT)) ? aaNames[aaConfigured] : "?",
	         (aaFrame != aaConfigured) ? "  frame:" : "",
	         (aaFrame != aaConfigured) ? (((aaFrame >= 0) && (aaFrame < NATIVE_AA_MODE_COUNT)) ? aaNames[aaFrame] : "?") : "");
	DBG_LINE("-- Geometry --");
	DBG_LINE("Renderer %s", NATIVE_DRAW3D_ACTIVE() ? "Native 3D" : "Classic");
	DBG_LINE("PGXP  %s  int-nclip %s", ((gNativePgxpMode >= 0) && (gNativePgxpMode < NATIVE_PGXP_MODE_COUNT)) ? pgxpNames[gNativePgxpMode] : "?",
	         NativeRenderer_DebugOnOff(gNativePgxpIntegerNclipEnabled));
	DBG_LINE("Depth buf %s", NativeRenderer_DebugOnOff(NATIVE_DEPTH_BUFFER_ACTIVE()));
	DBG_LINE("Max LOD %s", NativeRenderer_DebugOnOff(gNativeMaxLodEnabled));
	DBG_LINE("-- Look --");
	DBG_LINE("Colour %s", gNativeColorDepth == NATIVE_COLOR_DEPTH_15BIT ? "15-bit" : "24-bit");
	DBG_LINE("Dither  %s", NativeRenderer_DebugOnOff(gNativeDitheringEnabled));
	DBG_LINE("Texture %s", g_cfg_bilinearFiltering ? "bilinear" : "nearest");
	DBG_LINE("HD pause %s", ((gNativeHdPauseMode >= 0) && (gNativeHdPauseMode < 3)) ? pauseNames[gNativeHdPauseMode] : "?");
#ifdef CTR_INTERNAL
	DBG_LINE("Wire %s  Untextured %s", NativeRenderer_DebugOnOff(g_dbg_wireframeMode), NativeRenderer_DebugOnOff(g_dbg_texturelessMode));
#endif
	DBG_LINE("-- Enhancements --");
	DBG_LINE("Modern minimap %s", NativeRenderer_DebugOnOff(gNativeModernMapEnabled));
	DBG_LINE("Modern HUD icons %s", NativeRenderer_DebugOnOff(gNativeModernHudIconsEnabled));
	DBG_LINE("Frame rate %d (sel %d)%s", CTR_FRAMES_PER_SECOND, CTR_NATIVE_60FPS_SELECTED, gNativeForce30Fps ? " forced30" : "");
	DBG_LINE("Phys %s AI %s Coll %s Steer %s", NativeRenderer_DebugOnOff(gNativeSmoothedPhysicsEnabled), NativeRenderer_DebugOnOff(gNativeSmoothedAIEnabled),
	         NativeRenderer_DebugOnOff(gNativeSmoothedCollisionEnabled), NativeRenderer_DebugOnOff(gNativeSmoothedSteeringEnabled));
	DBG_LINE("AI racers %s", ((gNativeAIRacersMode >= 0) && (gNativeAIRacersMode < NATIVE_AI_RACERS_MODE_COUNT)) ? aiNames[gNativeAIRacersMode] : "?");
	DBG_LINE("Cam far %s  Speedo %s", NativeRenderer_DebugOnOff(gNativeDefaultCameraFar), NativeRenderer_DebugOnOff(gNativeDefaultHudSpeedometer));
	DBG_LINE("Mirror %s  Borderless %s", NativeRenderer_DebugOnOff(gNativeMirrorModeEnabled), NativeRenderer_DebugOnOff(gNativeBorderlessEnabled));
	DBG_LINE("Cheats mask %08X", (unsigned)gNativeCheatConfigMask);
#undef DBG_LINE
	return n;
}

internal void NativeRenderer_DrawDebugOverlay(void)
{
	static char lines[NATIVE_DEBUG_OVERLAY_MAX_LINES][NATIVE_DEBUG_OVERLAY_LINE_CHARS];
	static u8 pixels[(NATIVE_DEBUG_OVERLAY_LINE_CHARS * NATIVE_DEBUG_OVERLAY_GLYPH + 2 * NATIVE_DEBUG_OVERLAY_PAD) *
	                 (NATIVE_DEBUG_OVERLAY_MAX_LINES * NATIVE_DEBUG_OVERLAY_GLYPH + 2 * NATIVE_DEBUG_OVERLAY_PAD) * 4];

	if (!gNativeDebugOverlayEnabled)
	{
		s_debugOverlayLastCounter = 0;
		return;
	}

	const u64 now = SDL_GetPerformanceCounter();
	if (s_debugOverlayLastCounter != 0)
	{
		const float dt = (float)((double)(now - s_debugOverlayLastCounter) / (double)SDL_GetPerformanceFrequency());
		if (dt > 0.0f)
		{
			s_debugOverlayFps = (s_debugOverlayFps == 0.0f) ? (1.0f / dt) : (s_debugOverlayFps * 0.95f + (1.0f / dt) * 0.05f);
		}
	}
	s_debugOverlayLastCounter = now;

	const int lineCount = NativeRenderer_BuildDebugOverlayLines(lines);
	int maxChars = 0;
	for (int i = 0; i < lineCount; i++)
	{
		const int len = (int)strlen(lines[i]);
		if (len > maxChars)
			maxChars = len;
	}

	const int texW = maxChars * NATIVE_DEBUG_OVERLAY_GLYPH + 2 * NATIVE_DEBUG_OVERLAY_PAD;
	const int texH = lineCount * NATIVE_DEBUG_OVERLAY_GLYPH + 2 * NATIVE_DEBUG_OVERLAY_PAD;
	if ((texW <= 0) || (texH <= 0))
	{
		return;
	}

	// Translucent dark backdrop, then white glyphs with a 1px shadow.
	for (int i = 0; i < texW * texH; i++)
	{
		pixels[i * 4 + 0] = 0;
		pixels[i * 4 + 1] = 0;
		pixels[i * 4 + 2] = 0;
		pixels[i * 4 + 3] = 170;
	}
	for (int pass = 0; pass < 2; pass++)
	{
		for (int line = 0; line < lineCount; line++)
		{
			const char *text = lines[line];
			const int isHeader = (text[0] == '-') || (text[0] == 'C');
			for (int ci = 0; text[ci] != '\0'; ci++)
			{
				const int c = (unsigned char)text[ci];
				if ((c < NATIVE_DEBUG_FONT_FIRST) || (c > NATIVE_DEBUG_FONT_LAST))
					continue;
				const unsigned char *glyph = &s_nativeDebugFont[(c - NATIVE_DEBUG_FONT_FIRST) * 8];
				for (int gy = 0; gy < 8; gy++)
				{
					for (int gx = 0; gx < 8; gx++)
					{
						if (!(glyph[gy] & (1 << gx)))
							continue;
						const int px = NATIVE_DEBUG_OVERLAY_PAD + ci * NATIVE_DEBUG_OVERLAY_GLYPH + gx + (pass == 0 ? 1 : 0);
						const int py = NATIVE_DEBUG_OVERLAY_PAD + line * NATIVE_DEBUG_OVERLAY_GLYPH + gy + (pass == 0 ? 1 : 0);
						if ((px >= texW) || (py >= texH))
							continue;
						u8 *dst = &pixels[(py * texW + px) * 4];
						dst[0] = (pass == 0) ? 0 : (isHeader ? 255 : 230);
						dst[1] = (pass == 0) ? 0 : (isHeader ? 220 : 230);
						dst[2] = (pass == 0) ? 0 : (isHeader ? 80 : 230);
						dst[3] = 255;
					}
				}
			}
		}
	}

	if (s_debugOverlayTexture == 0)
	{
		glGenTextures(1, &s_debugOverlayTexture);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_debugOverlayTexture);
	if ((texW != s_debugOverlayTextureW) || (texH != s_debugOverlayTextureH))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		s_debugOverlayTextureW = texW;
		s_debugOverlayTextureH = texH;
	}
	else
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	}
	s_lastBoundTexture = (TextureID)-1;

	// Integer scale, kept small: 1x until the window is tall enough for 2x.
	const int scale = (s_presentViewport.h >= 1440) ? 2 : 1;
	const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	NativeRenderer_SetScissorState(0);
	NativeRenderer_SetBlendMode(BM_NONE);
	NativeRenderer_EnableDepth(0);
	glEnable(GL_BLEND);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_STENCIL_TEST);

	// GL origin is bottom-left: anchor to the top-left of the presented image.
	NativeRenderer_DrawGhostReplayQuad(s_debugOverlayTexture, s_presentViewport.x + 6, s_presentViewport.y + s_presentViewport.h - texH * scale - 6,
	                                   texW * scale, texH * scale);

	// Put GL back in the state the cached BM_NONE blend mode and the next frame's
	// draws assume: the raw alpha blend above bypassed the blend-mode cache.
	glDisable(GL_BLEND);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
	if (previousStencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
}
#endif

#ifndef __vita__
void NativeRenderer_DrawDebugOverlayFrame(void)
{
	NativeRenderer_DrawDebugOverlay();
}
#endif

void NativeRenderer_DrawGhostReplayOverlay(void)
{
	u32 buttonsHeld;
	u8 stickLX;
	u8 stickLY;
	u8 stickRX;
	u8 stickRY;

	if (!NativeGhostInput_GetReplayOverlayState(&buttonsHeld, &stickLX, &stickLY, &stickRX, &stickRY) || !NativeRenderer_LoadGhostReplayOverlay())
	{
		return;
	}

#ifdef __vita__
	const int overlayW = (s_presentViewport.w * 23) / 100;
#else
	const int overlayW = (s_presentViewport.w * 25) / 100;
#endif
	const int overlayH = (overlayW * s_ghostReplayControllerHeight) / s_ghostReplayControllerWidth;
	const int overlayX = s_presentViewport.x + 14;
	const int overlayY = s_presentViewport.y + 12;
	const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	NativeRenderer_SetScissorState(0);
#ifdef __vita__
	NativeRenderer_EnableDepth(0);
	NativeRenderer_SetBlendMode(BM_AVERAGE);
#else
	// Ghost Replay is modern RGBA UI, not a PS1 semi-transparency primitive.
	// BM_AVERAGE on desktop deliberately uses a constant 0.5 alpha to emulate
	// the PS1 ABR mode, which darkens/tints the entire controller overlay. Keep the
	// renderer state cache at BM_NONE and use ordinary source-alpha blending.
	NativeRenderer_SetBlendMode(BM_NONE);
	NativeRenderer_EnableDepth(0);
	glEnable(GL_BLEND);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
#endif
	glDisable(GL_STENCIL_TEST);

	NativeRenderer_DrawGhostReplayQuad(s_ghostReplayControllerTexture, overlayX, overlayY, overlayW, overlayH);

#ifdef __vita__
	if ((buttonsHeld & BTN_UP) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 28, 39, 14, 14);
	if ((buttonsHeld & BTN_DOWN) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 28, 62, 14, 14);
	if ((buttonsHeld & BTN_LEFT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 17, 51, 14, 14);
	if ((buttonsHeld & BTN_RIGHT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 39, 51, 14, 14);
	if ((buttonsHeld & BTN_TRIANGLE) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 272, 38, 14, 14);
	if ((buttonsHeld & BTN_CIRCLE) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 284, 51, 14, 14);
	if ((buttonsHeld & BTN_CROSS) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 272, 64, 14, 14);
	if ((buttonsHeld & BTN_SQUARE) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 260, 51, 14, 14);
	if ((buttonsHeld & (BTN_L1 | BTN_L2)) != 0)
		NativeRenderer_DrawGhostReplayImageRegion(s_ghostReplayShoulderTexture[0], overlayX, overlayY, overlayW, overlayH, 16, 0, 48, 16);
	if ((buttonsHeld & (BTN_R1 | BTN_R2)) != 0)
		NativeRenderer_DrawGhostReplayImageRegion(s_ghostReplayShoulderTexture[1], overlayX, overlayY, overlayW, overlayH, 237, 0, 48, 16);

	if ((buttonsHeld & BTN_SELECT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 224, 128, 12, 6);
	if ((buttonsHeld & BTN_START) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 242, 128, 12, 6);
	if ((buttonsHeld & NATIVE_GHOST_OVERLAY_TOUCH_FRONT_LEFT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 66, 18, 82, 5);
	if ((buttonsHeld & NATIVE_GHOST_OVERLAY_TOUCH_FRONT_RIGHT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 152, 18, 82, 5);
	if ((buttonsHeld & NATIVE_GHOST_OVERLAY_TOUCH_REAR_LEFT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 66, 127, 82, 5);
	if ((buttonsHeld & NATIVE_GHOST_OVERLAY_TOUCH_REAR_RIGHT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 152, 127, 82, 5);

	const int stickTravel = 8;
	const int leftStickX = 38 + (((int)stickLX - 128) * stickTravel) / 127;
	const int leftStickY = 90 + (((int)stickLY - 128) * stickTravel) / 127;
	const int rightStickX = 262 + (((int)stickRX - 128) * stickTravel) / 127;
	const int rightStickY = 90 + (((int)stickRY - 128) * stickTravel) / 127;
	NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, leftStickX, leftStickY, 8, 8);
	NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, rightStickX, rightStickY, 8, 8);
	if ((buttonsHeld & BTN_L3) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, leftStickX - 2, leftStickY - 2, 12, 12);
	if ((buttonsHeld & BTN_R3) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, rightStickX - 2, rightStickY - 2, 12, 12);
#else
	// Coordinates below are measured directly in assets/dualshock.png.
	// NativeRenderer_DrawGhostReplayHighlight takes top-origin image-space Y
	// coordinates and converts them to the OpenGL bottom-origin viewport.
	if ((buttonsHeld & BTN_UP) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 418, 312, 104, 104);
	if ((buttonsHeld & BTN_DOWN) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 418, 461, 104, 104);
	if ((buttonsHeld & BTN_LEFT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 342, 387, 104, 104);
	if ((buttonsHeld & BTN_RIGHT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 494, 387, 104, 104);
	if ((buttonsHeld & BTN_TRIANGLE) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 1253, 277, 112, 112);
	if ((buttonsHeld & BTN_CIRCLE) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 1369, 387, 112, 112);
	if ((buttonsHeld & BTN_CROSS) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 1253, 500, 112, 112);
	if ((buttonsHeld & BTN_SQUARE) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 1138, 387, 112, 112);

	// The front-facing asset exposes a single shoulder cap per side. CTR labels
	// these controls simply as L/R, so both shoulder inputs highlight the same
	// visible cap instead of a synthetic lower half.
	if ((buttonsHeld & (BTN_L1 | BTN_L2)) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 418, 70, 170, 64);
	if ((buttonsHeld & (BTN_R1 | BTN_R2)) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 1248, 70, 170, 64);

	if ((buttonsHeld & BTN_SELECT) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 742, 407, 86, 54);
	if ((buttonsHeld & BTN_START) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, 929, 408, 88, 58);

	const int stickTravel = 45;
	const int leftStickX = 623 + (((int)stickLX - 128) * stickTravel) / 127;
	const int leftStickY = 613 + (((int)stickLY - 128) * stickTravel) / 127;
	const int rightStickX = 1047 + (((int)stickRX - 128) * stickTravel) / 127;
	const int rightStickY = 613 + (((int)stickRY - 128) * stickTravel) / 127;
	NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, leftStickX, leftStickY, 58, 58);
	NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, rightStickX, rightStickY, 58, 58);
	if ((buttonsHeld & BTN_L3) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, leftStickX, leftStickY, 116, 116);
	if ((buttonsHeld & BTN_R3) != 0)
		NativeRenderer_DrawGhostReplayHighlight(overlayX, overlayY, overlayW, overlayH, rightStickX, rightStickY, 116, 116);
#endif

	if (previousStencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}
#ifdef __vita__
	NativeRenderer_SetBlendMode(BM_NONE);
#else
	glDisable(GL_BLEND);
	NativeRenderer_EnableDepth(1);
#endif
	NativeRenderer_SetViewPort(s_presentViewport.x, s_presentViewport.y, s_presentViewport.w, s_presentViewport.h);
	glBindVertexArray(0);
	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
}

internal void NativeRenderer_DestroyGhostReplayTextures(void)
{
	NativeRenderer_DestroyTexture(s_ghostReplayControllerTexture);
	NativeRenderer_DestroyTexture(s_ghostReplayHighlightTexture);
#ifdef __vita__
	NativeRenderer_DestroyTexture(s_ghostReplayShoulderTexture[0]);
	NativeRenderer_DestroyTexture(s_ghostReplayShoulderTexture[1]);
#endif
}
