#ifndef NATIVE_FONT_H
#define NATIVE_FONT_H

#include <macros.h>
#include <platform/native_options.h>

// Enhancements > Font: replaces the retail 2D text glyphs with a TrueType
// font. The font is rasterised into a cached signed distance field atlas, so
// glyphs stay sharp at any output resolution, and drawn with the retail vertex
// colours plus a black outline (see gte_shader_text_sdf in native_renderer.c).
//
// To add a font, put the .ttf under assets/fonts, add an id below and a row
// to s_nativeFonts in native_font.c. The menu cycles through every id.
// A disposable .ttf.sdf-cache beside the TTF avoids rasterisation on later
// boots. Font/settings changes or a damaged cache trigger regeneration.
#if defined(__vita__)
#define NATIVE_FONT_SUPPORTED 0
#else
#define NATIVE_FONT_SUPPORTED 1
#endif

enum NativeFontId
{
	NATIVE_FONT_ORIGINAL,
	NATIVE_FONT_CRASH_A_LIKE,
	NATIVE_FONT_COUNT,
};


// The atlas is a 16x16 grid of cells addressed with u8 texture coordinates:
// 16 units per cell, of which the glyph uses 15 so the far edge stays reachable.
#define NATIVE_FONT_ATLAS_CELLS       16
#define NATIVE_FONT_ATLAS_CELL_UNITS  16
#define NATIVE_FONT_ATLAS_GLYPH_UNITS 15
#define NATIVE_FONT_ATLAS_UNITS       (NATIVE_FONT_ATLAS_CELLS * NATIVE_FONT_ATLAS_CELL_UNITS)

// Distance field range around the outline, and the outline width, in ems.
// The atlas stores 0.5 on the glyph edge and falls to 0 at the padding.
#define NATIVE_FONT_SDF_PAD_EM        0.16f
#define NATIVE_FONT_OUTLINE_EM        0.07f
#define NATIVE_FONT_SDF_EDGE          (128.0f / 255.0f)
#define NATIVE_FONT_SDF_OUTLINE       (NATIVE_FONT_SDF_EDGE - (NATIVE_FONT_OUTLINE_EM / NATIVE_FONT_SDF_PAD_EM) * (128.0f / 255.0f))

struct NativeFontGlyph
{
	// Horizontal advance in ems, tracking included.
	float advance;
	u8 u0;
	u8 v0;
	u8 u1;
	u8 v1;
};

// Every atlas cell has the same layout, so one box (in ems, y up, relative to
// the pen on the baseline) places any glyph's quad.
struct NativeFontCellBox
{
	float left;
	float top;
	float right;
	float bottom;
};

const char *NativeFont_GetName(int font);

#if NATIVE_FONT_SUPPORTED
// Loads the selected font on first use. Returns 0 while Original is selected
// or when the font file or atlas is unavailable, so callers keep retail text.
int NativeFont_IsActive(void);
// NULL when the active font has no glyph for this character.
const struct NativeFontGlyph *NativeFont_GetGlyph(u8 character);
// Digits share the widest digit advance so timers do not shift as they count.
float NativeFont_GetDigitAdvance(void);
float NativeFont_GetSpaceAdvance(void);
float NativeFont_GetCapHeight(void);
const struct NativeFontCellBox *NativeFont_GetCellBox(void);
u32 NativeFont_GetAtlasTexture(void);
void NativeFont_ReleaseGpu(void);
#else
static inline int NativeFont_IsActive(void)
{
	return 0;
}
static inline void NativeFont_ReleaseGpu(void)
{
}
#endif

#endif
