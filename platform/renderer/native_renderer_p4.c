/*
 * Vita-only texture-page and palette cache: rebuilt 4-bit pages with their
 * CLUT baked in, keyed by page/clut/super-turbo tint, invalidated from VRAM
 * writes.
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

#ifdef __vita__

#define NATIVE_P4_CACHE_CAPACITY  320
#define NATIVE_P4_HASH_CAPACITY   1024
#define NATIVE_P4_PALETTE_BYTES   (16 * 4)
#define NATIVE_P4_INDEX_BYTES     (TPAGE_WIDTH * TPAGE_HEIGHT / 2)
#define NATIVE_P4_UPLOAD_BYTES    (NATIVE_P4_PALETTE_BYTES + NATIVE_P4_INDEX_BYTES)
#define NATIVE_PALETTE_CLUT_COUNT (64 * VRAM_HEIGHT)

struct NativeP4CacheEntry
{
	TextureID texture[2];
	s16 page;
	u16 clut;
	u8 superTurboTint;
	u32 lastUse;
	u32 lastUseFrame;
	u32 pageVersion;
	u32 paletteVersion;
	u32 bufferPageVersion[2];
	u32 bufferPaletteVersion[2];
	u32 bufferWriteFrame[2];
	u8 activeBuffer;
	b32 occupied;
};

global_variable struct NativeP4CacheEntry s_p4Cache[NATIVE_P4_CACHE_CAPACITY];
global_variable u32 s_p4CacheUseCounter;
global_variable u8 s_p4UploadData[NATIVE_P4_UPLOAD_BYTES];
global_variable u32 s_p4FrameSerial = 1;
global_variable s32 s_p4LastLookupIndex = -1;
global_variable u32 s_p4LastLookupKey = 0xffffffffu;

struct NativeP4HashEntry
{
	u32 key;
	s16 cacheIndex;
};

global_variable struct NativeP4HashEntry s_p4Hash[NATIVE_P4_HASH_CAPACITY];
global_variable u32 s_paletteRowGeneration[VRAM_HEIGHT];
global_variable u32 s_paletteCacheGeneration[2][NATIVE_PALETTE_CLUT_COUNT];
global_variable u8 s_paletteCacheProperties[2][NATIVE_PALETTE_CLUT_COUNT];


internal u32 NativeRenderer_P4HashSlot(u32 key)
{
	return (key * 2654435761u) & (NATIVE_P4_HASH_CAPACITY - 1);
}

internal u32 NativeRenderer_P4Key(int page, int clut, int superTurboTint)
{
	return ((u32)(page & 0x1f) << 17) | ((u32)(superTurboTint != 0) << 16) | (u16)clut;
}

internal void NativeRenderer_InsertP4Hash(u32 key, int cacheIndex)
{
	u32 slot = NativeRenderer_P4HashSlot(key);
	for (u32 probe = 0; probe < NATIVE_P4_HASH_CAPACITY; probe++)
	{
		struct NativeP4HashEntry *hashEntry = &s_p4Hash[slot];
		if (hashEntry->key == 0xffffffffu || hashEntry->key == key)
		{
			hashEntry->key = key;
			hashEntry->cacheIndex = (s16)cacheIndex;
			return;
		}
		slot = (slot + 1) & (NATIVE_P4_HASH_CAPACITY - 1);
	}
}

internal void NativeRenderer_RebuildP4Hash(void)
{
	memset(s_p4Hash, 0xff, sizeof(s_p4Hash));
	for (int cacheIndex = 0; cacheIndex < NATIVE_P4_CACHE_CAPACITY; cacheIndex++)
	{
		const struct NativeP4CacheEntry *entry = &s_p4Cache[cacheIndex];
		if (entry->occupied)
		{
			const u32 key = NativeRenderer_P4Key(entry->page, entry->clut, entry->superTurboTint);
			NativeRenderer_InsertP4Hash(key, cacheIndex);
		}
	}
}

internal int NativeRenderer_FindP4Hash(u32 key)
{
	u32 slot = NativeRenderer_P4HashSlot(key);
	for (u32 probe = 0; probe < NATIVE_P4_HASH_CAPACITY; probe++)
	{
		const struct NativeP4HashEntry *hashEntry = &s_p4Hash[slot];
		if (hashEntry->key == 0xffffffffu)
		{
			return -1;
		}
		if (hashEntry->key == key)
		{
			const int cacheIndex = hashEntry->cacheIndex;
			if (cacheIndex >= 0 && cacheIndex < NATIVE_P4_CACHE_CAPACITY)
			{
				const struct NativeP4CacheEntry *entry = &s_p4Cache[cacheIndex];
				if (entry->occupied && NativeRenderer_P4Key(entry->page, entry->clut, entry->superTurboTint) == key)
				{
					return cacheIndex;
				}
			}
		}
		slot = (slot + 1) & (NATIVE_P4_HASH_CAPACITY - 1);
	}
	return -1;
}

internal void NativeRenderer_GetP4SourceRects(int page, int clut, RECT16 *pageRect, RECT16 *clutRect)
{
	pageRect->x = (s16)((page & 15) * 64);
	pageRect->y = (s16)((page >> 4) * 256);
	pageRect->w = 64;
	pageRect->h = 256;

	clutRect->x = (s16)((clut & 0x3f) << 4);
	clutRect->y = (s16)(clut >> 6);
	clutRect->w = 16;
	clutRect->h = 1;
}

internal void NativeRenderer_InvalidatePaletteCacheRows(const RECT16 *rect)
{
	const int rowStart = rect->y < 0 ? 0 : rect->y;
	const int rowEnd = rect->y + rect->h > VRAM_HEIGHT ? VRAM_HEIGHT : rect->y + rect->h;
	for (int row = rowStart; row < rowEnd; row++)
	{
		u32 generation = ++s_paletteRowGeneration[row];
		if (generation == 0)
		{
			s_paletteRowGeneration[row] = 1;
			for (int clutX = 0; clutX < 64; clutX++)
			{
				const int clut = row * 64 + clutX;
				s_paletteCacheGeneration[0][clut] = 0;
				s_paletteCacheGeneration[1][clut] = 0;
			}
		}
	}
}

internal void NativeRenderer_InvalidateP4CacheRect(const RECT16 *rect)
{
	for (int cacheIndex = 0; cacheIndex < NATIVE_P4_CACHE_CAPACITY; cacheIndex++)
	{
		struct NativeP4CacheEntry *entry = &s_p4Cache[cacheIndex];
		if (!entry->occupied)
		{
			continue;
		}

		RECT16 pageRect;
		RECT16 clutRect;
		NativeRenderer_GetP4SourceRects(entry->page, entry->clut, &pageRect, &clutRect);
		if (NativeRenderer_VRAMRectsOverlap(rect, &pageRect))
		{
			if (++entry->pageVersion == 0)
			{
				entry->pageVersion = 1;
				entry->bufferPageVersion[0] = 0;
				entry->bufferPageVersion[1] = 0;
			}
		}
		if (NativeRenderer_VRAMRectsOverlap(rect, &clutRect))
		{
			if (++entry->paletteVersion == 0)
			{
				entry->paletteVersion = 1;
				entry->bufferPaletteVersion[0] = 0;
				entry->bufferPaletteVersion[1] = 0;
			}
		}
	}
}

internal int NativeRenderer_EnsureP4Buffer(struct NativeP4CacheEntry *entry, int bufferIndex)
{
	if (entry->texture[bufferIndex] != 0)
	{
		return 1;
	}

	glGenTextures(1, &entry->texture[bufferIndex]);
	if (entry->texture[bufferIndex] == 0)
	{
		return 0;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, entry->texture[bufferIndex]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	s_lastBoundTexture = (TextureID)-1;
	return 1;
}

internal void NativeRenderer_UpdateP4Buffer(struct NativeP4CacheEntry *entry, int bufferIndex, const RECT16 *pageRect, const RECT16 *clutRect)
{
	u8 *paletteOut = s_p4UploadData;
	const u16 *palette = s_vram.cpuPixels + (size_t)clutRect->y * VRAM_WIDTH + clutRect->x;
	for (int colorIndex = 0; colorIndex < 16; colorIndex++)
	{
		const u16 color = palette[colorIndex];
		u8 r = (u8)(color & 31);
		u8 g = (u8)((color >> 5) & 31);
		u8 b = (u8)((color >> 10) & 31);
		if (entry->superTurboTint && color != 0)
		{
			const u8 luminance = r > g ? (r > b ? r : b) : (g > b ? g : b);
			const int boosted = (luminance * 5 + 3) / 4;
			r = (u8)((luminance * 3) / 10);
			g = (u8)(boosted > 31 ? 31 : boosted);
			b = (u8)(boosted + 2 > 31 ? 31 : boosted + 2);
		}
		paletteOut[colorIndex * 4 + 0] = (u8)(r << 3);
		paletteOut[colorIndex * 4 + 1] = (u8)(g << 3);
		paletteOut[colorIndex * 4 + 2] = (u8)(b << 3);
		paletteOut[colorIndex * 4 + 3] = color == 0 ? 0 : (color & 0x8000) != 0 ? 255 : 128;
	}

	u8 *indicesOut = s_p4UploadData + NATIVE_P4_PALETTE_BYTES;
	for (int row = 0; row < TPAGE_HEIGHT; row++)
	{
		const u8 *src = (const u8 *)(s_vram.cpuPixels + (size_t)(pageRect->y + row) * VRAM_WIDTH + pageRect->x);
		u8 *dst = indicesOut + row * (TPAGE_WIDTH / 2);
		for (int packedIndex = 0; packedIndex < TPAGE_WIDTH / 2; packedIndex++)
		{
			const u8 packed = src[packedIndex];
			dst[packedIndex] = (u8)((packed << 4) | (packed >> 4));
		}
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, entry->texture[bufferIndex]);
	glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_PALETTE4_RGBA8_OES, TPAGE_WIDTH, TPAGE_HEIGHT, 0, NATIVE_P4_UPLOAD_BYTES, s_p4UploadData);
	s_lastBoundTexture = (TextureID)-1;
	entry->bufferPageVersion[bufferIndex] = entry->pageVersion;
	entry->bufferPaletteVersion[bufferIndex] = entry->paletteVersion;
}

TextureID NativeRenderer_GetCachedP4Texture(int page, int clut, int superTurboTint)
{
	if (page < 0 || page >= 32 || clut < 0)
	{
		return (0);
	}

	RECT16 pageRect;
	RECT16 clutRect;
	NativeRenderer_GetP4SourceRects(page, clut, &pageRect, &clutRect);
	if (clutRect.y < 0 || clutRect.y >= VRAM_HEIGHT || clutRect.x < 0 || clutRect.x + clutRect.w > VRAM_WIDTH)
	{
		return (0);
	}

	const u8 tint = (u8)(superTurboTint != 0);
	const u32 key = NativeRenderer_P4Key(page, clut, tint);
	struct NativeP4CacheEntry *entry = NULL;
	struct NativeP4CacheEntry *replacement = NULL;

	if (s_p4LastLookupIndex >= 0 && s_p4LastLookupIndex < NATIVE_P4_CACHE_CAPACITY && s_p4LastLookupKey == key)
	{
		struct NativeP4CacheEntry *candidate = &s_p4Cache[s_p4LastLookupIndex];
		if (candidate->occupied && candidate->page == page && candidate->clut == (u16)clut && candidate->superTurboTint == tint)
		{
			entry = candidate;
		}
	}

	if (entry == NULL)
	{
		const int cacheIndex = NativeRenderer_FindP4Hash(key);
		if (cacheIndex >= 0)
		{
			entry = &s_p4Cache[cacheIndex];
			s_p4LastLookupIndex = cacheIndex;
			s_p4LastLookupKey = key;
		}
	}

	if (entry != NULL && entry->texture[entry->activeBuffer] != 0 && entry->bufferPageVersion[entry->activeBuffer] == entry->pageVersion &&
	    entry->bufferPaletteVersion[entry->activeBuffer] == entry->paletteVersion)
	{
		entry->lastUse = ++s_p4CacheUseCounter;
		entry->lastUseFrame = s_p4FrameSerial;
		return (entry->texture[entry->activeBuffer]);
	}

	if (NativeRenderer_HasGpuNewerVRAMTiles(pageRect.x, pageRect.y, pageRect.w, pageRect.h) ||
	    NativeRenderer_HasGpuNewerVRAMTiles(clutRect.x, clutRect.y, clutRect.w, clutRect.h))
	{
		return (0);
	}

	if (entry == NULL)
	{
		for (int cacheIndex = 0; cacheIndex < NATIVE_P4_CACHE_CAPACITY; cacheIndex++)
		{
			struct NativeP4CacheEntry *candidate = &s_p4Cache[cacheIndex];
			if (!candidate->occupied)
			{
				if (replacement == NULL || replacement->occupied)
				{
					replacement = candidate;
				}
			}
			else if ((candidate->lastUseFrame + 2u < s_p4FrameSerial) &&
			         (replacement == NULL || (replacement->occupied && candidate->lastUse < replacement->lastUse)))
			{
				replacement = candidate;
			}
		}
	}

	if (entry == NULL)
	{
		entry = replacement;
		if (entry == NULL)
		{
			return (0);
		}

		const int entryIndex = (int)(entry - s_p4Cache);
		entry->page = (s16)page;
		entry->clut = (u16)clut;
		entry->superTurboTint = tint;
		entry->occupied = true;
		if (++entry->pageVersion == 0)
			entry->pageVersion = 1;
		if (++entry->paletteVersion == 0)
			entry->paletteVersion = 1;
		s_p4LastLookupIndex = entryIndex;
		s_p4LastLookupKey = key;
		NativeRenderer_InsertP4Hash(key, entryIndex);
	}

	entry->lastUse = ++s_p4CacheUseCounter;
	entry->lastUseFrame = s_p4FrameSerial;

	int targetBuffer = entry->activeBuffer ^ 1;
	if (entry->texture[entry->activeBuffer] == 0)
	{
		targetBuffer = entry->activeBuffer;
	}
	if (entry->bufferWriteFrame[targetBuffer] == s_p4FrameSerial)
	{
		targetBuffer ^= 1;
		if (entry->bufferWriteFrame[targetBuffer] == s_p4FrameSerial)
		{
			return (0);
		}
	}
	if (!NativeRenderer_EnsureP4Buffer(entry, targetBuffer))
	{
		return (0);
	}
	NativeRenderer_UpdateP4Buffer(entry, targetBuffer, &pageRect, &clutRect);
	entry->bufferWriteFrame[targetBuffer] = s_p4FrameSerial;
	entry->activeBuffer = (u8)targetBuffer;
	return (entry->texture[targetBuffer]);
}


int NativeRenderer_GetPaletteProperties(TexFormat format, int clut)
{
	const int formatIndex = format == TF_4_BIT ? 0 : format == TF_8_BIT ? 1 : -1;
	const int width = formatIndex == 0 ? 16 : formatIndex == 1 ? 256 : 0;
	const int x = (clut & 0x3f) << 4;
	const int y = clut >> 6;
	if (formatIndex < 0 || clut < 0 || clut >= NATIVE_PALETTE_CLUT_COUNT || y < 0 || y >= VRAM_HEIGHT || x < 0 || x + width > VRAM_WIDTH)
	{
		return NATIVE_PALETTE_HAS_TRANSPARENT | NATIVE_PALETTE_HAS_OPAQUE | NATIVE_PALETTE_HAS_STP;
	}

	const u32 generation = s_paletteRowGeneration[y];
	if (s_paletteCacheGeneration[formatIndex][clut] == generation)
	{
		return s_paletteCacheProperties[formatIndex][clut];
	}

	if (NativeRenderer_HasGpuNewerVRAMTiles(x, y, width, 1))
	{
		return NATIVE_PALETTE_HAS_TRANSPARENT | NATIVE_PALETTE_HAS_OPAQUE | NATIVE_PALETTE_HAS_STP;
	}

	int properties = 0;
	const u16 *palette = s_vram.cpuPixels + y * VRAM_WIDTH + x;
	for (int colorIndex = 0; colorIndex < width; colorIndex++)
	{
		const u16 color = palette[colorIndex];
		if (color == 0)
		{
			properties |= NATIVE_PALETTE_HAS_TRANSPARENT;
		}
		else if ((color & 0x8000) != 0)
		{
			properties |= NATIVE_PALETTE_HAS_STP;
		}
		else
		{
			properties |= NATIVE_PALETTE_HAS_OPAQUE;
		}
		if (properties == (NATIVE_PALETTE_HAS_TRANSPARENT | NATIVE_PALETTE_HAS_OPAQUE | NATIVE_PALETTE_HAS_STP))
		{
			break;
		}
	}

	s_paletteCacheProperties[formatIndex][clut] = (u8)properties;
	s_paletteCacheGeneration[formatIndex][clut] = generation;
	return properties;
}

// Cache lifetime, driven by the renderer lifecycle rather than any lookup.
internal void NativeRenderer_ResetP4Cache(void)
{
	SDL_memset(s_p4Cache, 0, sizeof(s_p4Cache));
	s_p4CacheUseCounter = 0;
	s_p4FrameSerial = 1;
	s_p4LastLookupIndex = -1;
	s_p4LastLookupKey = 0xffffffffu;
	SDL_memset(s_paletteCacheGeneration, 0, sizeof(s_paletteCacheGeneration));
	SDL_memset(s_paletteCacheProperties, 0, sizeof(s_paletteCacheProperties));
	for (int row = 0; row < VRAM_HEIGHT; row++)
	{
		s_paletteRowGeneration[row] = 1;
	}
}

// Wraps the frame serial used to tell this frame's cache use from a previous
// frame's, and rebuilds the lookup hash over the entries still held.
internal void NativeRenderer_BeginP4Frame(void)
{
	if (++s_p4FrameSerial == 0)
	{
		s_p4FrameSerial = 1;
		for (int cacheIndex = 0; cacheIndex < NATIVE_P4_CACHE_CAPACITY; cacheIndex++)
		{
			s_p4Cache[cacheIndex].bufferWriteFrame[0] = 0;
			s_p4Cache[cacheIndex].bufferWriteFrame[1] = 0;
		}
	}
	NativeRenderer_RebuildP4Hash();
}

internal void NativeRenderer_DestroyP4Textures(void)
{
	for (int cacheIndex = 0; cacheIndex < NATIVE_P4_CACHE_CAPACITY; cacheIndex++)
	{
		for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++)
		{
			if (s_p4Cache[cacheIndex].texture[bufferIndex] != 0)
			{
				NativeRenderer_DestroyTexture(s_p4Cache[cacheIndex].texture[bufferIndex]);
			}
		}
	}
}

#endif
