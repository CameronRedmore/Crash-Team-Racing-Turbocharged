#include <common.h>
#include <platform/native_kart_color.h>
#include <platform/native_renderer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gNativeKartHue = 0;

#define KART_CLUT_ENTRIES   16
#define KART_CLUT_KEY_COUNT 32768
#define KART_RECORD_MAX     192

// Hue range, in degrees, of each character's kart paint. A range with lo > hi
// wraps through 0. Entries must also reach minSat and minVal (percent), which
// keeps greys, shadows and skin out of the rotation. Paint is told apart from
// skin and clothes by hue alone because the karts do not share texture layouts.
// Penta's kart is white, so there is no hue to rotate and it is left alone.
struct NativeKartPaintBand
{
	const char *model;
	s16 hueLo;
	s16 hueHi;
	u8 minSat;
	u8 minVal;
};

static const struct NativeKartPaintBand s_nativeKartBands[] = {
    {"crash", 190, 240, 30, 25}, {"cortex", 340, 15, 30, 25}, {"tiny", 70, 170, 30, 25},    {"coco", 270, 340, 30, 25},     {"ngin", 265, 295, 30, 25},
    {"dingo", 55, 85, 30, 15},   {"polar", 165, 200, 30, 25}, {"pura", 230, 290, 30, 25},   {"pinstripe", 195, 225, 30, 8}, {"papu", 52, 70, 30, 25},
    {"roo", 12, 35, 30, 25},     {"joe", 30, 70, 30, 25},     {"ntropy", 170, 230, 30, 25}, {"oxide", 80, 160, 30, 25},     {"fake", 190, 240, 30, 15},
};

// Palettes shared by three or more characters (tyres, exhausts, chassis trim);
// they are never rotated.
static const u16 s_nativeKartSharedCluts[] = {0x37e6, 0x3ef0, 0x3ef1, 0x3f70, 0x7ef3, 0x7f33, 0x7f73, 0x7fb3};

struct NativeKartPalette
{
	u16 clut;
	u16 hue;
	u32 frame;
	u16 original[KART_CLUT_ENTRIES];
	u16 written[KART_CLUT_ENTRIES];
};

static struct NativeKartPalette s_palettes[KART_RECORD_MAX];
static u8 s_paletteIndex[KART_CLUT_KEY_COUNT]; // record + 1, 0 = none
static int s_paletteCount;
static u32 s_frame = 1;

void NativeKartColor_BeginFrame(void)
{
	s_frame++;
}

static const struct NativeKartPaintBand *NativeKartColor_FindBand(const char *name)
{
	for (unsigned int i = 0; i < sizeof(s_nativeKartBands) / sizeof(s_nativeKartBands[0]); i++)
	{
		if (strncmp(name, s_nativeKartBands[i].model, MODEL_NAME_BYTE_COUNT) == 0)
		{
			return &s_nativeKartBands[i];
		}
	}
	return NULL;
}

// Menu previews have no thread; in the hub and in races human players sit in thread bucket 0.
static int NativeKartColor_IsPlayerKart(const struct Instance *inst)
{
	return (inst->thread == NULL) || ((inst->thread->flags & 0xff) == 0);
}

static u16 NativeKartColor_RotateEntry(u16 color, const struct NativeKartPaintBand *band, int degrees)
{
	const float r = (color & 31) / 31.0f;
	const float g = ((color >> 5) & 31) / 31.0f;
	const float b = ((color >> 10) & 31) / 31.0f;
	const float mx = fmaxf(r, fmaxf(g, b));
	const float mn = fminf(r, fminf(g, b));
	const float d = mx - mn;
	if (color == 0 || mx < band->minVal / 100.0f || d < (band->minSat / 100.0f) * mx)
	{
		return color;
	}

	float h = (mx == r) ? fmodf((g - b) / d + 6.0f, 6.0f) : (mx == g) ? (b - r) / d + 2.0f : (r - g) / d + 4.0f;
	h *= 60.0f;
	const int inBand = (band->hueLo <= band->hueHi) ? (h >= band->hueLo && h <= band->hueHi) : (h >= band->hueLo || h <= band->hueHi);
	if (!inBand)
	{
		return color;
	}

	h = fmodf(h + (float)degrees + 360.0f, 360.0f) / 60.0f;
	const float s = d / mx;
	const int sector = (int)h;
	const float f = h - (float)sector;
	const float p = mx * (1.0f - s);
	const float q = mx * (1.0f - s * f);
	const float t = mx * (1.0f - s * (1.0f - f));
	float nr, ng, nb;
	switch (sector % 6)
	{
	case 0:
		nr = mx;
		ng = t;
		nb = p;
		break;
	case 1:
		nr = q;
		ng = mx;
		nb = p;
		break;
	case 2:
		nr = p;
		ng = mx;
		nb = t;
		break;
	case 3:
		nr = p;
		ng = q;
		nb = mx;
		break;
	case 4:
		nr = t;
		ng = p;
		nb = mx;
		break;
	default:
		nr = mx;
		ng = p;
		nb = q;
		break;
	}
	return (u16)((color & 0x8000) | (int)(nr * 31.0f + 0.5f) | ((int)(ng * 31.0f + 0.5f) << 5) | ((int)(nb * 31.0f + 0.5f) << 10));
}

// Development aid: CTR_DUMP_KART_LAYOUTS=1 logs every distinct texture layout of every model.
static void NativeKartColor_DumpLayout(const struct Instance *inst, const struct TextureLayout *layout)
{
	static u64 seen[4096];
	static int seenCount;
	u64 key =
	    ((u64)layout->clut << 48) ^ ((u64)layout->tpage << 32) ^ ((u64)layout->u0 << 24) ^ ((u64)layout->v0 << 16) ^ ((u64)layout->u3 << 8) ^ (u64)layout->v3;
	for (int i = 0; i < 16 && inst->model->name[i]; i++)
		key = key * 1099511628211ull + (u8)inst->model->name[i];
	for (int i = 0; i < seenCount; i++)
		if (seen[i] == key)
			return;
	if (seenCount >= 4096)
		return;
	seen[seenCount++] = key;
	u16 pal[KART_CLUT_ENTRIES];
	NativeRenderer_ReadVRAM(pal, (layout->clut & 0x3f) << 4, layout->clut >> 6, KART_CLUT_ENTRIES, 1);
	fprintf(stderr, "KARTLAYOUT model=%.16s clut=%04x tpage=%04x uv=%d,%d %d,%d %d,%d %d,%d pal:", inst->model->name, layout->clut, layout->tpage, layout->u0,
	        layout->v0, layout->u1, layout->v1, layout->u2, layout->v2, layout->u3, layout->v3);
	for (int i = 0; i < KART_CLUT_ENTRIES; i++)
		fprintf(stderr, " %04x", pal[i]);
	fprintf(stderr, "\n");
}

// Development aid: CTR_KART_CLUT_PAINT=1 paints every palette of a character model in its own hue and
// logs which, so screenshots show which palette is which part of the model.
static void NativeKartColor_PaintLayout(const struct Instance *inst, const struct TextureLayout *layout)
{
	static u16 painted[1024];
	static int paintedCount;
	if (NativeKartColor_FindBand(inst->model->name) == NULL)
		return;
	for (int i = 0; i < paintedCount; i++)
		if (painted[i] == layout->clut)
			return;
	if (paintedCount >= 1024)
		return;
	painted[paintedCount] = layout->clut;
	const float hue = fmodf(paintedCount * 137.5f, 360.0f) / 60.0f;
	paintedCount++;
	u16 pal[KART_CLUT_ENTRIES];
	const int x = (layout->clut & 0x3f) << 4;
	const int y = layout->clut >> 6;
	NativeRenderer_ReadVRAM(pal, x, y, KART_CLUT_ENTRIES, 1);
	const int sector = (int)hue;
	const float f = hue - (float)sector;
	for (int i = 0; i < KART_CLUT_ENTRIES; i++)
	{
		if (pal[i] == 0)
			continue;
		const float r = (pal[i] & 31) / 31.0f, g = ((pal[i] >> 5) & 31) / 31.0f, b = ((pal[i] >> 10) & 31) / 31.0f;
		const float v = fmaxf(0.35f, fmaxf(r, fmaxf(g, b)));
		const float p = 0.0f, q = v * (1.0f - f), t = v * f;
		float nr, ng, nb;
		switch (sector % 6)
		{
		case 0:
			nr = v;
			ng = t;
			nb = p;
			break;
		case 1:
			nr = q;
			ng = v;
			nb = p;
			break;
		case 2:
			nr = p;
			ng = v;
			nb = t;
			break;
		case 3:
			nr = p;
			ng = q;
			nb = v;
			break;
		case 4:
			nr = t;
			ng = p;
			nb = v;
			break;
		default:
			nr = v;
			ng = p;
			nb = q;
			break;
		}
		pal[i] = (u16)((pal[i] & 0x8000) | (int)(nr * 31.0f + 0.5f) | ((int)(ng * 31.0f + 0.5f) << 5) | ((int)(nb * 31.0f + 0.5f) << 10));
	}
	NativeRenderer_CopyVRAM(pal, 0, 0, KART_CLUT_ENTRIES, 1, x, y);
	fprintf(stderr, "KARTPAINT model=%.16s clut=%04x hue=%d\n", inst->model->name, layout->clut, (int)(hue * 60.0f));
}

void NativeKartColor_OnLayout(const struct Instance *inst, const struct TextureLayout *layout)
{
	static int paint = -1;
	if (paint < 0)
		paint = getenv("CTR_KART_CLUT_PAINT") != NULL;
	if (paint)
	{
		NativeKartColor_PaintLayout(inst, layout);
		return;
	}
	static int dump = -1;
	if (dump < 0)
		dump = getenv("CTR_DUMP_KART_LAYOUTS") != NULL;
	if (dump)
		NativeKartColor_DumpLayout(inst, layout);
	const u16 clut = layout->clut;
	if (clut >= KART_CLUT_KEY_COUNT)
	{
		return;
	}

	int record = (int)s_paletteIndex[clut] - 1;
	if (record >= 0 && s_palettes[record].frame == s_frame)
	{
		return;
	}
	if (record < 0 && gNativeKartHue == 0)
	{
		return;
	}

	for (unsigned int i = 0; i < sizeof(s_nativeKartSharedCluts) / sizeof(s_nativeKartSharedCluts[0]); i++)
	{
		if (s_nativeKartSharedCluts[i] == clut)
		{
			return;
		}
	}

	const struct NativeKartPaintBand *band = NativeKartColor_FindBand(inst->model->name);
	if (band == NULL || !NativeKartColor_IsPlayerKart(inst))
	{
		return;
	}

	if (record < 0)
	{
		if (s_paletteCount >= KART_RECORD_MAX)
		{
			return;
		}
		record = s_paletteCount++;
		memset(&s_palettes[record], 0, sizeof(s_palettes[record]));
		s_palettes[record].clut = clut;
		s_paletteIndex[clut] = (u8)(record + 1);
	}

	struct NativeKartPalette *palette = &s_palettes[record];
	u16 current[KART_CLUT_ENTRIES];
	const int x = (clut & 0x3f) << 4;
	const int y = clut >> 6;
	NativeRenderer_ReadVRAM(current, x, y, KART_CLUT_ENTRIES, 1);

	// Anything other than what we last wrote is a fresh retail palette (first sight or a VRAM reload).
	if (palette->frame == 0 || memcmp(current, palette->written, sizeof(current)) != 0)
	{
		memcpy(palette->original, current, sizeof(current));
	}

	u16 desired[KART_CLUT_ENTRIES];
	const int degrees = gNativeKartHue * NATIVE_KART_HUE_STEP_DEGREES;
	for (int i = 0; i < KART_CLUT_ENTRIES; i++)
	{
		desired[i] = degrees ? NativeKartColor_RotateEntry(palette->original[i], band, degrees) : palette->original[i];
	}
	if (memcmp(current, desired, sizeof(current)) != 0)
	{
		NativeRenderer_CopyVRAM(desired, 0, 0, KART_CLUT_ENTRIES, 1, x, y);
	}
	memcpy(palette->written, desired, sizeof(desired));
	palette->hue = (u16)gNativeKartHue;
	palette->frame = s_frame;
}
