#include <macros.h>
#include "platform/native_font.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "platform/native_assets.h"
#include "platform/native_log.h"
#include "platform/native_renderer.h"

int gNativeFont = NATIVE_FONT_ORIGINAL;

struct NativeFontDesc
{
	const char *menuName;
	// Relative to the assets directory; NULL for the retail font.
	const char *path;
	// Extra letter spacing in ems. The outline grows every glyph, so fonts
	// with tight side bearings need room to keep outlines off neighbours.
	float tracking;
	// Word space in ems, or 0 for the font's own.
	float space;
};

static const struct NativeFontDesc s_nativeFonts[NATIVE_FONT_COUNT] = {
    [NATIVE_FONT_ORIGINAL] = {"ORIGINAL", NULL, 0.0f, 0.0f},
    [NATIVE_FONT_CRASH_A_LIKE] = {"CRASH-A-LIKE", "fonts/crash-a-like.ttf", 0.05f, 0.25f},
};

const char *NativeFont_GetName(int font)
{
	if ((font < 0) || (font >= NATIVE_FONT_COUNT))
	{
		font = NATIVE_FONT_ORIGINAL;
	}
	return s_nativeFonts[font].menuName;
}

#if NATIVE_FONT_SUPPORTED

#define STB_TRUETYPE_IMPLEMENTATION
#include "../externals/stb/stb_truetype.h"

#define NATIVE_FONT_ATLAS_UNIT_PIXELS 8
#define NATIVE_FONT_ATLAS_SIZE        (NATIVE_FONT_ATLAS_UNITS * NATIVE_FONT_ATLAS_UNIT_PIXELS)
#define NATIVE_FONT_CELL_PIXELS       (NATIVE_FONT_ATLAS_CELL_UNITS * NATIVE_FONT_ATLAS_UNIT_PIXELS)
#define NATIVE_FONT_GLYPH_PIXELS      (NATIVE_FONT_ATLAS_GLYPH_UNITS * NATIVE_FONT_ATLAS_UNIT_PIXELS)

// Printable ASCII, then Latin-1 from 0xA0.
#define NATIVE_FONT_ASCII_FIRST       0x20
#define NATIVE_FONT_ASCII_LAST        0x7e
#define NATIVE_FONT_LATIN1_FIRST      0xa0
#define NATIVE_FONT_LATIN1_CELL       (NATIVE_FONT_ASCII_LAST - NATIVE_FONT_ASCII_FIRST + 1)
#define NATIVE_FONT_GLYPH_COUNT       256

struct NativeFontState
{
	// Font the atlas below was built for, or -1.
	int loadedFont;
	// Set when loading failed, so the file is not retried every string.
	int failedFont;
	u32 atlasTexture;
	struct NativeFontGlyph glyphs[NATIVE_FONT_GLYPH_COUNT];
	b32 present[NATIVE_FONT_GLYPH_COUNT];
	struct NativeFontCellBox cellBox;
	float digitAdvance;
	float spaceAdvance;
	float capHeight;
};

global_variable struct NativeFontState s_nativeFont = {.loadedFont = -1, .failedFont = -1};

internal int NativeFont_CellForCharacter(int c)
{
	if ((c >= NATIVE_FONT_ASCII_FIRST) && (c <= NATIVE_FONT_ASCII_LAST))
	{
		return c - NATIVE_FONT_ASCII_FIRST;
	}
	if (c >= NATIVE_FONT_LATIN1_FIRST)
	{
		return NATIVE_FONT_LATIN1_CELL + (c - NATIVE_FONT_LATIN1_FIRST);
	}
	return -1;
}

// Disposable, versioned cache. Keep the original TTF as the source of truth.
// Bump the revision when changing rasterisation or glyph layout. The settings
// and font bytes are also hashed, so replacing a TTF invalidates its cache.
#define NATIVE_FONT_CACHE_REVISION 1

internal u32 NativeFont_Hash(u32 hash, const void *data, size_t size)
{
	const u8 *bytes = (const u8 *)data;
	for (size_t i = 0; i < size; i++)
	{
		hash = (hash ^ bytes[i]) * 16777619u;
	}
	return hash;
}

internal u32 NativeFont_CacheKey(const struct NativeFontDesc *desc, const struct NativeAssetsByteBuffer *bytes)
{
	const u32 layout[] = {NATIVE_FONT_CACHE_REVISION,
	                      NATIVE_FONT_ATLAS_SIZE,
	                      NATIVE_FONT_CELL_PIXELS,
	                      NATIVE_FONT_GLYPH_PIXELS,
	                      sizeof(struct NativeFontState),
	                      sizeof(float),
	                      0x01020304};
	const float settings[] = {desc->tracking, desc->space, NATIVE_FONT_SDF_PAD_EM};
	u32 hash = NativeFont_Hash(2166136261u, layout, sizeof(layout));
	hash = NativeFont_Hash(hash, settings, sizeof(settings));
	return NativeFont_Hash(hash, bytes->data, (size_t)bytes->size);
}

internal b32 NativeFont_ReadCache(const char *path, u32 key)
{
	FILE *file = NativeAssets_OpenHost(path, "rb");
	if (!file)
		return false;
	u32 header[3];
	struct NativeFontState cached;
	const size_t atlasSize = (size_t)NATIVE_FONT_ATLAS_SIZE * NATIVE_FONT_ATLAS_SIZE;
	u8 *atlas = NULL;
	b32 ok = false;
	if (fread(header, sizeof(header), 1, file) != 1 || header[0] != 0x43544643 || header[1] != key)
		goto done;
	atlas = (u8 *)malloc(atlasSize);
	if (!atlas || fread(&cached, sizeof(cached), 1, file) != 1 || fread(atlas, atlasSize, 1, file) != 1 || fgetc(file) != EOF || ferror(file))
		goto done;
	u32 checksum = NativeFont_Hash(2166136261u, &cached, sizeof(cached));
	checksum = NativeFont_Hash(checksum, atlas, atlasSize);
	if (checksum != header[2])
		goto done;
	// Never restore GPU handles from disk.
	cached.atlasTexture = NativeRenderer_CreateFontAtlasTexture(NATIVE_FONT_ATLAS_SIZE, NATIVE_FONT_ATLAS_SIZE, atlas);
	if (!cached.atlasTexture)
		goto done;
	cached.loadedFont = -1;
	cached.failedFont = -1;
	s_nativeFont = cached;
	ok = true;
done:
	free(atlas);
	fclose(file);
	return ok;
}

internal void NativeFont_WriteCache(const char *path, u32 key, const u8 *atlas)
{
	char finalPath[1024], tempPath[1030];
	if (!NativeAssets_BuildPath(path, finalPath, sizeof(finalPath)))
		return;
	if (snprintf(tempPath, sizeof(tempPath), "%s.tmp", finalPath) >= (int)sizeof(tempPath))
		return;
	FILE *file = fopen(tempPath, "wb");
	if (!file)
		return; // Read-only assets still work without caching.
	struct NativeFontState cached = s_nativeFont;
	cached.loadedFont = cached.failedFont = -1;
	cached.atlasTexture = 0;
	const size_t atlasSize = (size_t)NATIVE_FONT_ATLAS_SIZE * NATIVE_FONT_ATLAS_SIZE;
	u32 checksum = NativeFont_Hash(2166136261u, &cached, sizeof(cached));
	checksum = NativeFont_Hash(checksum, atlas, atlasSize);
	const u32 header[] = {0x43544643, key, checksum};
	b32 ok = fwrite(header, sizeof(header), 1, file) == 1 && fwrite(&cached, sizeof(cached), 1, file) == 1 && fwrite(atlas, atlasSize, 1, file) == 1;
	if (fclose(file) != 0)
		ok = false;
	if (ok && rename(tempPath, finalPath) == 0)
		return;
	remove(tempPath);
}

internal b32 NativeFont_Build(int font)
{
	const struct NativeFontDesc *desc = &s_nativeFonts[font];
	struct NativeAssetsByteBuffer bytes = {0};
	stbtt_fontinfo info;
	u8 *atlas = NULL;
	b32 ok = false;

	if (!NativeAssets_ReadBytes(desc->path, NATIVE_ASSET_READ_DATA_FILE, &bytes))
	{
		Platform_LogError("[CTR Font] Failed to read assets/%s\n", desc->path);
		return false;
	}
	char cachePath[256];
	const u32 cacheKey = NativeFont_CacheKey(desc, &bytes);
	const b32 cachePathValid = snprintf(cachePath, sizeof(cachePath), "%s.sdf-cache", desc->path) < (int)sizeof(cachePath);
	if (cachePathValid && NativeFont_ReadCache(cachePath, cacheKey))
	{
		Platform_Log("[CTR Font] Loaded %s from atlas cache\n", desc->menuName);
		ok = true;
		goto done;
	}
	if (!stbtt_InitFont(&info, bytes.data, stbtt_GetFontOffsetForIndex(bytes.data, 0)))
	{
		Platform_LogError("[CTR Font] assets/%s is not a usable TrueType font\n", desc->path);
		goto done;
	}

	// One scale and one cell layout for every glyph: fit the union of all
	// glyph boxes, plus the distance field padding, into a glyph region.
	int xMin = 0x7fff, yMin = 0x7fff, xMax = -0x7fff, yMax = -0x7fff;
	for (int c = 0; c < NATIVE_FONT_GLYPH_COUNT; c++)
	{
		int x0, y0, x1, y1;
		if ((NativeFont_CellForCharacter(c) < 0) || (stbtt_FindGlyphIndex(&info, c) == 0) || !stbtt_GetCodepointBox(&info, c, &x0, &y0, &x1, &y1))
		{
			continue;
		}
		xMin = x0 < xMin ? x0 : xMin;
		yMin = y0 < yMin ? y0 : yMin;
		xMax = x1 > xMax ? x1 : xMax;
		yMax = y1 > yMax ? y1 : yMax;
	}
	if (xMax <= xMin || yMax <= yMin)
	{
		Platform_LogError("[CTR Font] assets/%s has no Latin glyphs\n", desc->path);
		goto done;
	}

	const float emUnits = 1.0f / stbtt_ScaleForMappingEmToPixels(&info, 1.0f);
	const float boxEm = fmaxf((float)(xMax - xMin), (float)(yMax - yMin)) / emUnits;
	const float emPixels = (float)NATIVE_FONT_GLYPH_PIXELS / (boxEm + 2.0f * NATIVE_FONT_SDF_PAD_EM);
	const float scale = emPixels / emUnits;
	const float padPixels = NATIVE_FONT_SDF_PAD_EM * emPixels;
	const float originX = padPixels - (float)xMin * scale;
	const float baselineY = padPixels + (float)yMax * scale;

	atlas = (u8 *)calloc(NATIVE_FONT_ATLAS_SIZE, NATIVE_FONT_ATLAS_SIZE);
	if (atlas == NULL)
	{
		goto done;
	}

	memset(s_nativeFont.present, 0, sizeof(s_nativeFont.present));
	int ascent;
	stbtt_GetFontVMetrics(&info, &ascent, NULL, NULL);

	for (int c = 0; c < NATIVE_FONT_GLYPH_COUNT; c++)
	{
		const int cell = NativeFont_CellForCharacter(c);
		if ((cell < 0) || (stbtt_FindGlyphIndex(&info, c) == 0))
		{
			continue;
		}

		int advance, leftBearing;
		stbtt_GetCodepointHMetrics(&info, c, &advance, &leftBearing);

		const int cellX = (cell % NATIVE_FONT_ATLAS_CELLS) * NATIVE_FONT_CELL_PIXELS;
		const int cellY = (cell / NATIVE_FONT_ATLAS_CELLS) * NATIVE_FONT_CELL_PIXELS;
		int width = 0, height = 0, xoff = 0, yoff = 0;
		u8 *sdf = stbtt_GetCodepointSDF(&info, scale, c, (int)ceilf(padPixels), 128, 128.0f / padPixels, &width, &height, &xoff, &yoff);
		if (sdf != NULL)
		{
			const int dstX = (int)floorf(originX + 0.5f) + xoff;
			const int dstY = (int)floorf(baselineY + 0.5f) + yoff;
			for (int y = 0; y < height; y++)
			{
				const int py = dstY + y;
				if (py < 0 || py >= NATIVE_FONT_GLYPH_PIXELS)
				{
					continue;
				}
				for (int x = 0; x < width; x++)
				{
					const int px = dstX + x;
					if (px < 0 || px >= NATIVE_FONT_GLYPH_PIXELS)
					{
						continue;
					}
					atlas[(cellY + py) * NATIVE_FONT_ATLAS_SIZE + cellX + px] = sdf[y * width + x];
				}
			}
			stbtt_FreeSDF(sdf, NULL);
		}

		struct NativeFontGlyph *glyph = &s_nativeFont.glyphs[c];
		glyph->advance = (float)advance / emUnits + desc->tracking;
		glyph->u0 = (u8)((cell % NATIVE_FONT_ATLAS_CELLS) * NATIVE_FONT_ATLAS_CELL_UNITS);
		glyph->v0 = (u8)((cell / NATIVE_FONT_ATLAS_CELLS) * NATIVE_FONT_ATLAS_CELL_UNITS);
		glyph->u1 = (u8)(glyph->u0 + NATIVE_FONT_ATLAS_GLYPH_UNITS);
		glyph->v1 = (u8)(glyph->v0 + NATIVE_FONT_ATLAS_GLYPH_UNITS);
		s_nativeFont.present[c] = true;
	}

	s_nativeFont.cellBox.left = -originX / emPixels;
	s_nativeFont.cellBox.right = ((float)NATIVE_FONT_GLYPH_PIXELS - originX) / emPixels;
	s_nativeFont.cellBox.top = baselineY / emPixels;
	s_nativeFont.cellBox.bottom = (baselineY - (float)NATIVE_FONT_GLYPH_PIXELS) / emPixels;

	s_nativeFont.digitAdvance = 0.0f;
	for (int c = '0'; c <= '9'; c++)
	{
		if (s_nativeFont.present[c] && s_nativeFont.glyphs[c].advance > s_nativeFont.digitAdvance)
		{
			s_nativeFont.digitAdvance = s_nativeFont.glyphs[c].advance;
		}
	}
	int spaceAdvance, spaceBearing;
	stbtt_GetCodepointHMetrics(&info, ' ', &spaceAdvance, &spaceBearing);
	s_nativeFont.spaceAdvance = (desc->space > 0.0f ? desc->space : (float)spaceAdvance / emUnits) + desc->tracking;

	int hx0, hy0, hx1, hy1;
	s_nativeFont.capHeight = stbtt_GetCodepointBox(&info, 'H', &hx0, &hy0, &hx1, &hy1) ? (float)hy1 / emUnits : (float)ascent / emUnits;

	s_nativeFont.atlasTexture = NativeRenderer_CreateFontAtlasTexture(NATIVE_FONT_ATLAS_SIZE, NATIVE_FONT_ATLAS_SIZE, atlas);
	ok = s_nativeFont.atlasTexture != 0;
	if (ok)
	{
		Platform_Log("[CTR Font] Loaded %s (%.1f px em atlas)\n", desc->menuName, emPixels);
		if (cachePathValid)
			NativeFont_WriteCache(cachePath, cacheKey, atlas);
	}

done:
	free(atlas);
	NativeAssets_FreeBytes(&bytes);
	return ok;
}

int NativeFont_IsActive(void)
{
	const int font = gNativeFont;
	if ((font <= NATIVE_FONT_ORIGINAL) || (font >= NATIVE_FONT_COUNT))
	{
		return 0;
	}
	if (font == s_nativeFont.loadedFont)
	{
		return 1;
	}
	if (font == s_nativeFont.failedFont)
	{
		return 0;
	}

	NativeFont_ReleaseGpu();
	if (!NativeFont_Build(font))
	{
		s_nativeFont.failedFont = font;
		return 0;
	}
	s_nativeFont.loadedFont = font;
	return 1;
}

const struct NativeFontGlyph *NativeFont_GetGlyph(u8 character)
{
	return s_nativeFont.present[character] ? &s_nativeFont.glyphs[character] : NULL;
}

float NativeFont_GetDigitAdvance(void)
{
	return s_nativeFont.digitAdvance;
}

float NativeFont_GetSpaceAdvance(void)
{
	return s_nativeFont.spaceAdvance;
}

float NativeFont_GetCapHeight(void)
{
	return s_nativeFont.capHeight;
}

const struct NativeFontCellBox *NativeFont_GetCellBox(void)
{
	return &s_nativeFont.cellBox;
}

u32 NativeFont_GetAtlasTexture(void)
{
	return s_nativeFont.atlasTexture;
}

void NativeFont_ReleaseGpu(void)
{
	if (s_nativeFont.atlasTexture != 0)
	{
		NativeRenderer_DestroyFontAtlasTexture(s_nativeFont.atlasTexture);
		s_nativeFont.atlasTexture = 0;
	}
	s_nativeFont.loadedFont = -1;
}

#endif
