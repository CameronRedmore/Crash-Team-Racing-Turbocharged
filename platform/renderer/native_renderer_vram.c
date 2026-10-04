/*
 * PS1 VRAM: the CPU mirror, its dirty-rect tracking, the packed GPU texture,
 * and the transfers between them.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>
#include <string.h>

#include "platform/native_log.h"
#include "platform/native_renderer_internal.h"

TextureID NativeRenderer_GetVRAMTexture(void)
{
	return s_vram.texture;
}


internal int NativeRenderer_ClipVRAMRect(RECT16 *out, int x, int y, int w, int h)
{
	if ((w <= 0) || (h <= 0))
	{
		return 0;
	}

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > VRAM_WIDTH)
	{
		w = VRAM_WIDTH - x;
	}
	if (y + h > VRAM_HEIGHT)
	{
		h = VRAM_HEIGHT - y;
	}
	if ((w <= 0) || (h <= 0))
	{
		return 0;
	}

	out->x = (s16)x;
	out->y = (s16)y;
	out->w = (s16)w;
	out->h = (s16)h;
	return 1;
}

#ifdef __vita__
internal int NativeRenderer_HasGpuNewerVRAMTiles(int x, int y, int w, int h)
{
	const int tileX0 = x / NATIVE_VRAM_TILE_SIZE;
	const int tileY0 = y / NATIVE_VRAM_TILE_SIZE;
	const int tileX1 = (x + w - 1) / NATIVE_VRAM_TILE_SIZE;
	const int tileY1 = (y + h - 1) / NATIVE_VRAM_TILE_SIZE;
	for (int tileY = tileY0; tileY <= tileY1; tileY++)
	{
		for (int tileX = tileX0; tileX <= tileX1; tileX++)
		{
			const int tileIndex = tileY * NATIVE_VRAM_TILE_COLS + tileX;
			if ((s_vram.gpuNewerTiles[tileIndex >> 5] & (1u << (tileIndex & 31))) != 0)
			{
				return 1;
			}
		}
	}
	return 0;
}

internal int NativeRenderer_VRAMRectsOverlap(const RECT16 *a, const RECT16 *b)
{
	return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

#endif
internal int NativeRenderer_DirtyRectContains(const RECT16 *outer, const RECT16 *inner)
{
	const int outerRight = outer->x + outer->w;
	const int outerBottom = outer->y + outer->h;
	const int innerRight = inner->x + inner->w;
	const int innerBottom = inner->y + inner->h;

	return (outer->x <= inner->x) && (outer->y <= inner->y) && (outerRight >= innerRight) && (outerBottom >= innerBottom);
}

internal int NativeRenderer_TryMergeDirtyRect(RECT16 *dst, const RECT16 *src)
{
	const int dstRight = dst->x + dst->w;
	const int dstBottom = dst->y + dst->h;
	const int srcRight = src->x + src->w;
	const int srcBottom = src->y + src->h;

	if ((dst->y == src->y) && (dst->h == src->h) && (src->x <= dstRight) && (dst->x <= srcRight))
	{
		const int x0 = (src->x < dst->x) ? src->x : dst->x;
		const int x1 = (srcRight > dstRight) ? srcRight : dstRight;
		dst->x = (s16)x0;
		dst->w = (s16)(x1 - x0);
		return 1;
	}

	if ((dst->x == src->x) && (dst->w == src->w) && (src->y <= dstBottom) && (dst->y <= srcBottom))
	{
		const int y0 = (src->y < dst->y) ? src->y : dst->y;
		const int y1 = (srcBottom > dstBottom) ? srcBottom : dstBottom;
		dst->y = (s16)y0;
		dst->h = (s16)(y1 - y0);
		return 1;
	}

	return 0;
}

internal void NativeRenderer_AppendCpuDirtyRect(const RECT16 *rect)
{
	for (s32 i = 0; i < s_vram.cpuDirtyRectCount; i++)
	{
		if (NativeRenderer_DirtyRectContains(&s_vram.cpuDirtyRects[i], rect))
		{
			return;
		}

		if (NativeRenderer_DirtyRectContains(rect, &s_vram.cpuDirtyRects[i]))
		{
			s_vram.cpuDirtyRects[i] = *rect;
			return;
		}

		if (NativeRenderer_TryMergeDirtyRect(&s_vram.cpuDirtyRects[i], rect))
		{
			return;
		}
	}

	if (s_vram.cpuDirtyRectCount >= NATIVE_VRAM_DIRTY_RECT_CAP)
	{
		NativeRenderer_UpdateVRAM();
	}

	if (s_vram.cpuDirtyRectCount < NATIVE_VRAM_DIRTY_RECT_CAP)
	{
		s_vram.cpuDirtyRects[s_vram.cpuDirtyRectCount] = *rect;
		s_vram.cpuDirtyRectCount++;
	}
}

internal void NativeRenderer_MarkVRAMDirty(int x, int y, int w, int h)
{
	RECT16 rect;

	if (!NativeRenderer_ClipVRAMRect(&rect, x, y, w, h))
	{
		return;
	}

#ifdef __vita__
	NativeRenderer_InvalidateP4CacheRect(&rect);
	NativeRenderer_InvalidatePaletteCacheRows(&rect);
#endif
	NativeRenderer_AppendCpuDirtyRect(&rect);
}

internal void NativeRenderer_MarkGpuVRAMNewer(int x, int y, int w, int h)
{
	RECT16 rect;

	if (!NativeRenderer_ClipVRAMRect(&rect, x, y, w, h))
	{
		return;
	}

#ifdef __vita__
	NativeRenderer_InvalidateP4CacheRect(&rect);
	NativeRenderer_InvalidatePaletteCacheRows(&rect);
#endif
	const int tileX0 = rect.x / NATIVE_VRAM_TILE_SIZE;
	const int tileY0 = rect.y / NATIVE_VRAM_TILE_SIZE;
	const int tileX1 = (rect.x + rect.w - 1) / NATIVE_VRAM_TILE_SIZE;
	const int tileY1 = (rect.y + rect.h - 1) / NATIVE_VRAM_TILE_SIZE;

	for (int tileY = tileY0; tileY <= tileY1; tileY++)
	{
		for (int tileX = tileX0; tileX <= tileX1; tileX++)
		{
			const int tileIndex = tileY * NATIVE_VRAM_TILE_COLS + tileX;
			s_vram.gpuNewerTiles[tileIndex >> 5] |= 1u << (tileIndex & 31);
		}
	}
}

void NativeRenderer_ClearVRAM(int x, int y, int w, int h, u8 r, u8 g, u8 b)
{
	u16 *dst = s_vram.cpuPixels + x + y * VRAM_WIDTH;
	const u16 color = NativeRenderer_PackRGB24ToPSX15(r, g, b);

	if (x + w > VRAM_WIDTH)
	{
		w = VRAM_WIDTH - x;
	}

	if (y + h > VRAM_HEIGHT)
	{
		h = VRAM_HEIGHT - y;
	}

	// clear VRAM region with given color
	for (int i = 0; i < h; i++)
	{
		u16 *tmp = dst;

		for (int j = 0; j < w; j++)
		{
			*tmp++ = color;
		}

		dst += VRAM_WIDTH;
	}

	NativeRenderer_MarkVRAMDirty(x, y, w, h);
}

void NativeRenderer_Clear(int x, int y, int w, int h, u8 r, u8 g, u8 b)
{
	if ((w <= 0) || (h <= 0))
	{
		return;
	}

	int displayX = NativeGpu_GetRenderDispEnv()->disp.x;
	int displayY = NativeGpu_GetRenderDispEnv()->disp.y;
	int displayW = NativeGpu_GetRenderDispEnv()->disp.w;
	int displayH = NativeGpu_GetRenderDispEnv()->disp.h;

	if ((displayW <= 0) || (displayH <= 0))
	{
		displayX = NativeGpu_GetRenderDrawEnv()->clip.x;
		displayY = NativeGpu_GetRenderDrawEnv()->clip.y;
		displayW = NativeGpu_GetRenderDrawEnv()->clip.w;
		displayH = NativeGpu_GetRenderDrawEnv()->clip.h;
	}

	if ((displayW <= 0) || (displayH <= 0))
	{
		return;
	}

	const int clearRight = x + w;
	const int clearBottom = y + h;
	const int displayRight = displayX + displayW;
	const int displayBottom = displayY + displayH;

	const int overlapX = x > displayX ? x : displayX;
	const int overlapY = y > displayY ? y : displayY;
	const int overlapRight = clearRight < displayRight ? clearRight : displayRight;
	const int overlapBottom = clearBottom < displayBottom ? clearBottom : displayBottom;

	if ((overlapRight <= overlapX) || (overlapBottom <= overlapY))
	{
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
	const int scissorW = scissorRight - scissorX;
	const int scissorH = scissorTop - scissorY;

	if ((scissorW <= 0) || (scissorH <= 0))
	{
		return;
	}

	GLint previousScissorBox[4];
	const GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_SCISSOR_BOX, previousScissorBox);

	glEnable(GL_SCISSOR_TEST);
	glScissor(scissorX, scissorY, scissorW, scissorH);
	glClearColor(NativeRenderer_PSXColorComponentFloat(r), NativeRenderer_PSXColorComponentFloat(g), NativeRenderer_PSXColorComponentFloat(b), 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (previousScissorEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(previousScissorBox[0], previousScissorBox[1], previousScissorBox[2], previousScissorBox[3]);
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}

	s_previousScissorState = previousScissorEnabled ? 1 : 0;
	NativeRenderer_InvalidateScissorRectCache();
}

void NativeRenderer_SaveVRAM(const char *outputFileName, int x, int y, int width, int height, int bReadFromFrameBuffer)
{
#define FLIP_Y (VRAM_HEIGHT - i - 1)

	(void)x;
	(void)y;
	(void)bReadFromFrameBuffer;

	NativeRenderer_SyncGpuVRAMToCPU(0, 0, VRAM_WIDTH, VRAM_HEIGHT);

	FILE *fp = fopen(outputFileName, "wb");
	if (fp == NULL)
	{
		return;
	}

	u8 TGAheader[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	u8 header[6];
	header[0] = (width % 256);
	header[1] = (width / 256);
	header[2] = (height % 256);
	header[3] = (height / 256);
	header[4] = 16;
	header[5] = 0;

	fwrite(TGAheader, sizeof(u8), 12, fp);
	fwrite(header, sizeof(u8), 6, fp);

	for (int i = 0; i < VRAM_HEIGHT; i++)
	{
		fwrite(s_vram.cpuPixels + VRAM_WIDTH * FLIP_Y, sizeof(u16), VRAM_WIDTH, fp);
	}

	fclose(fp);

#undef FLIP_Y
}

internal void NativeRenderer_SyncGpuVRAMToCPU(int x, int y, int w, int h)
{
	RECT16 readRect;

	if (!NativeRenderer_ClipVRAMRect(&readRect, x, y, w, h))
	{
		return;
	}

	const int tileX0 = readRect.x / NATIVE_VRAM_TILE_SIZE;
	const int tileY0 = readRect.y / NATIVE_VRAM_TILE_SIZE;
	const int tileX1 = (readRect.x + readRect.w - 1) / NATIVE_VRAM_TILE_SIZE;
	const int tileY1 = (readRect.y + readRect.h - 1) / NATIVE_VRAM_TILE_SIZE;
	b32 needsReadback = false;

	for (int tileY = tileY0; tileY <= tileY1 && !needsReadback; tileY++)
	{
		for (int tileX = tileX0; tileX <= tileX1; tileX++)
		{
			const int tileIndex = tileY * NATIVE_VRAM_TILE_COLS + tileX;
			if ((s_vram.gpuNewerTiles[tileIndex >> 5] & (1u << (tileIndex & 31))) != 0)
			{
				needsReadback = true;
				break;
			}
		}
	}

	if (!needsReadback)
	{
		return;
	}

	readRect.x = (s16)(tileX0 * NATIVE_VRAM_TILE_SIZE);
	readRect.y = (s16)(tileY0 * NATIVE_VRAM_TILE_SIZE);
	readRect.w = (s16)((tileX1 - tileX0 + 1) * NATIVE_VRAM_TILE_SIZE);
	readRect.h = (s16)((tileY1 - tileY0 + 1) * NATIVE_VRAM_TILE_SIZE);

	// CPU writes must reach the persistent texture before a GPU-authored region
	// is read back, preserving PS1 VRAM command order in the split host mirror.
	NativeRenderer_UpdateVRAM();

	NativePerf_BeginScope(NATIVE_PERF_BUCKET_FRAMEBUFFER_READBACK);
	GLint previousReadFramebuffer;
	GLint previousPackRowLength;
	GLint previousPackAlignment;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
#ifndef __vita__
	glGetIntegerv(GL_PACK_ROW_LENGTH, &previousPackRowLength);
	glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
#endif

	glBindFramebuffer(GL_READ_FRAMEBUFFER, s_glVramFramebuffer);
#ifndef __vita__
	glPixelStorei(GL_PACK_ROW_LENGTH, VRAM_WIDTH);
	glPixelStorei(GL_PACK_ALIGNMENT, sizeof(u16));
	glReadPixels(readRect.x, readRect.y, readRect.w, readRect.h, VRAM_FORMAT, GL_UNSIGNED_BYTE,
	             s_vram.cpuPixels + (size_t)readRect.y * VRAM_WIDTH + readRect.x);
#else
	// READBACKS_SPEEDHACK deliberately omits vitaGL's implicit scene finish.
	// CPU consumers such as the pause-screen compressor access the returned
	// bytes immediately, so make this explicit at the native synchronization
	// boundary instead of disabling the speedhack globally.
	glFinish();
	// vitaGL always packs rows tightly.  Passing a pointer into cpuPixels here
	// would advance each subsequent row by readRect.w instead of VRAM_WIDTH.
	glReadPixels(readRect.x, readRect.y, readRect.w, readRect.h, VRAM_FORMAT, GL_UNSIGNED_BYTE, s_vitaVramTransferPixels);
	for (int row = 0; row < readRect.h; row++)
	{
		const u8 *src = s_vitaVramTransferPixels + (size_t)row * readRect.w * 4;
		u16 *dst = s_vram.cpuPixels + (size_t)(readRect.y + row) * VRAM_WIDTH + readRect.x;
		for (int column = 0; column < readRect.w; column++)
		{
			dst[column] = (u16)(src[column * 4] | ((u16)src[column * 4 + 1] << 8));
		}
	}
#endif

	for (int tileY = tileY0; tileY <= tileY1; tileY++)
	{
		for (int tileX = tileX0; tileX <= tileX1; tileX++)
		{
			const int tileIndex = tileY * NATIVE_VRAM_TILE_COLS + tileX;
			s_vram.gpuNewerTiles[tileIndex >> 5] &= ~(1u << (tileIndex & 31));
		}
	}

#ifndef __vita__
	glPixelStorei(GL_PACK_ROW_LENGTH, previousPackRowLength);
	glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
#endif
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previousReadFramebuffer);
	NativePerf_EndScope(NATIVE_PERF_BUCKET_FRAMEBUFFER_READBACK);
}

internal void NativeRenderer_ResolveVRAMRead(int x, int y, int w, int h)
{
	NativeRenderer_SyncGpuVRAMToCPU(x, y, w, h);
}

void NativeRenderer_SyncVRAMToCPU(int x, int y, int w, int h)
{
	NativeRenderer_SyncGpuVRAMToCPU(x, y, w, h);
}

internal int NativeRenderer_RectEquals(const RECT16 *a, const RECT16 *b)
{
	return a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h;
}

internal void NativeRenderer_FlushOffscreenToVRAM(void)
{
	if (s_previousOffscreen.w <= 0 || s_previousOffscreen.h <= 0)
	{
		return;
	}

	// NOTE(aalhendi): Native offscreen draws produce RGBA pixels. Pack them into
	// the persistent 5:5:5:1 VRAM texture instead of reading them through the CPU.
	NativeRenderer_GpuPackTextureToVRAM(s_offscreenRenderTarget.texture, s_previousOffscreen.x, s_previousOffscreen.y, s_previousOffscreen.w,
	                                    s_previousOffscreen.h, true);
}

void NativeRenderer_CopyVRAM(u16 *src, int x, int y, int w, int h, int dst_x, int dst_y)
{
	int stride = w;

	if (!src)
	{
		// NOTE(aalhendi): MoveImage reads exactly its PS1 VRAM source rectangle. Resolve only that
		// GPU-authored region into the CPU mirror before copying it.
		NativeRenderer_ResolveVRAMRead(x, y, w, h);
		src = s_vram.cpuPixels;
		stride = VRAM_WIDTH;
	}

	src += x + y * stride;

	u16 *dst = s_vram.cpuPixels + dst_x + dst_y * VRAM_WIDTH;

	for (int i = 0; i < h; i++)
	{
		SDL_memcpy(dst, src, w * sizeof(u16));
		dst += VRAM_WIDTH;
		src += stride;
	}

	NativeRenderer_MarkVRAMDirty(dst_x, dst_y, w, h);
}

void NativeRenderer_ReadVRAM(u16 *dst, int x, int y, int dst_w, int dst_h)
{
	NativeRenderer_ResolveVRAMRead(x, y, dst_w, dst_h);

	u16 *src = s_vram.cpuPixels + x + VRAM_WIDTH * y;

	for (int i = 0; i < dst_h; i++)
	{
		SDL_memcpy(dst, src, dst_w * sizeof(u16));
		dst += dst_w;
		src += VRAM_WIDTH;
	}
}

int NativeRenderer_GetVRAMStateSize(void)
{
	return (int)sizeof(s_vram.cpuPixels);
}

int NativeRenderer_CaptureVRAMState(void *dst, int dstSize)
{
	if ((dst == NULL) || (dstSize < (int)sizeof(s_vram.cpuPixels)))
	{
		return 0;
	}

	// NOTE(aalhendi): Save-states own the CPU-side PSX VRAM mirror, not GL
	// textures. Pull pending GPU-authored VRAM into the mirror first.
	NativeRenderer_SyncGpuVRAMToCPU(0, 0, VRAM_WIDTH, VRAM_HEIGHT);
	SDL_memcpy(dst, s_vram.cpuPixels, sizeof(s_vram.cpuPixels));
	return 1;
}

int NativeRenderer_RestoreVRAMState(const void *src, int srcSize)
{
	local_persist const RECT16 zeroRect = {0, 0, 0, 0};

	if ((src == NULL) || (srcSize < (int)sizeof(s_vram.cpuPixels)))
	{
		return 0;
	}

	SDL_memcpy(s_vram.cpuPixels, src, sizeof(s_vram.cpuPixels));
	// NOTE(aalhendi): Restored VRAM is authoritative PSX state. Host GL caches
	// are rebuildable, so mark all of VRAM dirty and drop stale bindings.
	s_vram.cpuDirtyRectCount = 0;
	SDL_memset(s_vram.gpuNewerTiles, 0, sizeof(s_vram.gpuNewerTiles));
	NativeRenderer_MarkVRAMDirty(0, 0, VRAM_WIDTH, VRAM_HEIGHT);
	s_mainRenderTarget.width = 0;
	s_mainRenderTarget.height = 0;
	s_mainRenderTarget.logicalWidth = 0;
	s_mainRenderTarget.logicalHeight = 0;
	s_offscreenRenderTarget.width = 0;
	s_offscreenRenderTarget.height = 0;
	s_offscreenRenderTarget.logicalWidth = 0;
	s_offscreenRenderTarget.logicalHeight = 0;
	s_previousOffscreen = zeroRect;
	s_previousOffscreenState = 0;
	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
	return 1;
}

void NativeRenderer_UpdateVRAM(void)
{
	if (s_vram.cpuDirtyRectCount == 0)
	{
		return;
	}

	NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDERER_UPDATE_VRAM);

	const s32 rectCount = s_vram.cpuDirtyRectCount;
	s_vram.cpuDirtyRectCount = 0;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_vram.texture);
#ifdef __vita__
	// Expand each packed PSX word to the RGBA render-target representation.
	// The shaders consume only .r/.g, which retain the low/high PSX bytes.
	for (s32 i = 0; i < rectCount; i++)
	{
		const RECT16 r = s_vram.cpuDirtyRects[i];
		u8 *dst = s_vitaVramTransferPixels;
		for (int y = 0; y < r.h; y++)
		{
			const u16 *src = s_vram.cpuPixels + (size_t)(r.y + y) * VRAM_WIDTH + r.x;
			for (int x = 0; x < r.w; x++)
			{
				const u16 pixel = src[x];
				*dst++ = (u8)pixel;
				*dst++ = (u8)(pixel >> 8);
				*dst++ = 0;
				*dst++ = 255;
			}
		}
		glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, VRAM_FORMAT, GL_UNSIGNED_BYTE, s_vitaVramTransferPixels);
	}
#else
	glPixelStorei(GL_UNPACK_ROW_LENGTH, VRAM_WIDTH);
	for (s32 i = 0; i < rectCount; i++)
	{
		const RECT16 r = s_vram.cpuDirtyRects[i];
		glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, VRAM_FORMAT, GL_UNSIGNED_BYTE, s_vram.cpuPixels + (size_t)r.y * VRAM_WIDTH + r.x);
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	s_lastBoundTexture = (TextureID)-1;

	NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_UPDATE_VRAM);
}

void NativeRenderer_PresentVRAMRect(int displayX, int displayY, int displayW, int displayH)
{
	if (displayW <= 0 || displayH <= 0)
	{
		return;
	}

	NativeRenderer_UpdateVRAM();

	NativeRenderer_SetViewPort(s_presentViewport.x, s_presentViewport.y, s_presentViewport.w, s_presentViewport.h);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	NativeRenderer_SetScissorState(0);
	NativeRenderer_EnableDepth(0);
	NativeRenderer_SetBlendMode(BM_NONE);
	const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);
	glDisable(GL_STENCIL_TEST);

	NativeRenderer_DrawVRAMRegion(displayX, displayY, displayW, displayH);
	if (previousStencilEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}
	glBindVertexArray(0);

	s_previousShader = (ShaderID)-1;
	s_lastBoundTexture = (TextureID)-1;
}
