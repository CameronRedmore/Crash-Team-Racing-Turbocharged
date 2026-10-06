#include <common.h>

#ifdef CTR_NATIVE
#include <platform/native_font.h>
#include <platform/native_hud_icons.h>
#define DECAL_FONT_TTF NATIVE_FONT_SUPPORTED
#else
#define DECAL_FONT_TTF 0
#endif

static void DecalFont_DrawGlyph(u8 character, struct Icon *icon, s16 posX, s16 posY, struct PrimMem *primMem, u32 *ot, u32 color0, u32 color1, u32 color2, u32 color3,
                                char transparency, s16 scale)
{

#ifdef CTR_NATIVE
	if (NativeHudIcons_DrawButton(character, icon, posX, posY, scale, primMem, ot)) return;

#else
	(void)character;

#endif
	POLY_GT4 *p = P32_GET(void *, primMem->cursor);
	DecalHUD_DrawPolyGT4(icon, posX, posY, primMem, ot, color0, color1, color2, color3, transparency, scale);

#if CTR_NATIVE_WIDESCREEN
	if ((icon != NULL) && (P32_GET(void *, primMem->cursor) == p + 1))
	{
		const int sourceWidth = FP_Mult(icon->texLayout.u1 - icon->texLayout.u0, scale);
		const int targetWidth = NativeAspect_ScaleXCeil(sourceWidth);
		const int drawnWidth = p->x1 - p->x0;
		const int expandRight = targetWidth - drawnWidth;

		if (expandRight > 0)
		{
			p->x1 += expandRight;
			p->x3 += expandRight;
		}
	}
#endif
}

#if DECAL_FONT_TTF
// Enhancements > Font: lines are laid out with TrueType advances at subpixel
// precision and drawn from the font's distance field atlas. PS1 button glyphs,
// and characters the font lacks, keep their retail icons at the pen position.

// Cap height, and the gap from posY to the cap line, in PS1 pixels per font.
static const float sDecalFont_TtfCapHeight[FONT_NUM] = {0.0f, 12.0f, 5.5f, 12.0f};
static const float sDecalFont_TtfCapTop[FONT_NUM] = {0.0f, 1.5f, 1.25f, 1.5f};

#if CTR_NATIVE_WIDESCREEN
// TrueType geometry is already in HUD coordinates; correct its pixel aspect
// directly rather than applying the retail glyph squeeze a second time.
// Classic keeps its established font metrics.
#define DECAL_FONT_TTF_ASPECT (NativeAspect_IsActive() ? (float)NativeAspect_GetHudPixelAspectX() : (45.0f / 34.0f))
#else
// 512x216 shown at 4:3.
#define DECAL_FONT_TTF_ASPECT (512.0f / 216.0f * 3.0f / 4.0f)
#endif

// Set while a retail icon stands in for one character of a TrueType line.
static b32 sDecalFont_TtfRetailCharacter;

static b32 DecalFont_TtfActive(void)
{
	return !sDecalFont_TtfRetailCharacter && NativeFont_IsActive();
}

static float DecalFont_TtfEmY(int fontType)
{
	return sDecalFont_TtfCapHeight[fontType] / NativeFont_GetCapHeight();
}

static b32 DecalFont_IsButton(u8 c)
{
	return (c == '@') || (c == '[') || (c == '^') || (c == '*');
}

// Advance in PS1 pixels. Sets glyph when the TrueType font draws the
// character, or leaves it NULL for a space or a retail icon.
static float DecalFont_TtfCharacter(u8 c, int fontType, const struct NativeFontGlyph **glyph)
{
	const float emX = DecalFont_TtfEmY(fontType) * DECAL_FONT_TTF_ASPECT;

	*glyph = NULL;
	if (c == ' ')
	{
		return NativeFont_GetSpaceAdvance() * emX;
	}
	if (!DecalFont_IsButton(c) && (c > ' ') && (c < 0x7f))
	{
		*glyph = NativeFont_GetGlyph(c);
	}
	if (*glyph != NULL)
	{
		return (((c >= '0') && (c <= '9')) ? NativeFont_GetDigitAdvance() : (*glyph)->advance) * emX;
	}

	char retail[2] = {(char)c, 0};
	sDecalFont_TtfRetailCharacter = true;
	const int width = DecalFont_GetLineWidthStrlen(retail, 1, fontType);
	sDecalFont_TtfRetailCharacter = false;
	return (float)width;
}

static int DecalFont_TtfLineWidth(char *str, int len, int fontType)
{
	const struct NativeFontGlyph *glyph;
	float width = 0.0f;

	for (; (*str != 0) && (len != 0); str++, len--)
	{
#if BUILD > UsaRetail
		if (*str == '~')
		{
			str += 2;
			len -= 2;
			continue;
		}
#endif
		width += DecalFont_TtfCharacter((u8)*str, fontType, &glyph);
	}

	return (int)(width + 0.5f);
}

// Retail colours belong to the corners of the glyph's cap box; the quad also
// covers the outline and padding, so extend the gradient out to its corners.
static u32 DecalFont_TtfCornerColor(const u32 *color, float fx, float fy)
{
	u32 out = 0;

	for (int shift = 0; shift < 24; shift += 8)
	{
		const float tl = (float)((color[0] >> shift) & 0xff);
		const float tr = (float)((color[1] >> shift) & 0xff);
		const float bl = (float)((color[2] >> shift) & 0xff);
		const float br = (float)((color[3] >> shift) & 0xff);
		const float top = tl + (tr - tl) * fx;
		const float bottom = bl + (br - bl) * fx;
		int value = (int)(top + (bottom - top) * fy + 0.5f);
		value = value < 0 ? 0 : (value > 0xff ? 0xff : value);
		out |= (u32)value << shift;
	}

	return out;
}

// Links packets after the atlas texture packet, in draw order.
struct DecalFontTtfChain
{
	DR_PSYX_TEX *head;
	u32 *tailTag;
	u32 tailLength;
};

static void DecalFont_TtfChainAppend(struct DecalFontTtfChain *chain, void *packet, u32 *tag, u32 length)
{
	*chain->tailTag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(packet), chain->tailLength << 24);
	chain->tailTag = tag;
	chain->tailLength = length;
}

static void DecalFont_TtfDrawLine(char *str, s16 len, int posX, s16 posY, s16 fontType, int flags)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	struct PrimMem *primMem = &P32_GET(struct DB *, gGT->backBuffer)->primMem;
	struct IconGroup *iconGroup = P32_GET(struct IconGroup *, gGT->iconGroup[data.font_IconGroupID[fontType]]);

	// Glyph quads borrow the retail font page for their draw mode, so a
	// line drawn before the font icons load stays empty, as in retail.
	if ((iconGroup == NULL) || (iconGroup->numIcons == 0))
	{
		return;
	}
	struct Icon **icons = ICONGROUP_GETICONS(iconGroup);
	if (icons[0] == NULL)
	{
		return;
	}
	const u16 tpage = icons[0]->texLayout.tpage;

	if (flags & (JUSTIFY_CENTER | JUSTIFY_RIGHT))
	{
		int alignX = DecalFont_GetLineWidthStrlen(str, len, fontType);

		if (flags & JUSTIFY_CENTER)
		{
			alignX /= 2;
		}

		posX -= alignX;
	}

#if BUILD >= JpnTrial
	flags &= 0x7ff;
#else
	flags &= 0xfff;
#endif

	const float emY = DecalFont_TtfEmY(fontType);
	const float emX = emY * DECAL_FONT_TTF_ASPECT;
	const struct NativeFontCellBox *cell = NativeFont_GetCellBox();
	const float capTop = (float)posY + sDecalFont_TtfCapTop[fontType];
	const float baseline = capTop + sDecalFont_TtfCapHeight[fontType];
	const float quadTop = baseline - cell->top * emY;
	const float quadBottom = baseline - cell->bottom * emY;
	const float fy0 = (quadTop - capTop) / (baseline - capTop);
	const float fy1 = (quadBottom - capTop) / (baseline - capTop);
	float penX = (float)posX;

	struct DecalFontTtfChain chain = {0};

	for (; (*str != 0) && (len != 0); str++, len--)
	{
		const u8 c = (u8)*str;

#if BUILD >= JpnTrial
		if (c == '~')
		{
			flags = (str[2] + (str[1] - 0x30) * 10) - 0x30;
			str += 2;
			len -= 2;
			continue;
		}
#endif

		const struct NativeFontGlyph *glyph;
		const float advance = DecalFont_TtfCharacter(c, fontType, &glyph);

		if (glyph == NULL)
		{
			if (c != ' ')
			{
				sDecalFont_TtfRetailCharacter = true;
				DecalFont_DrawLineStrlen(str, 1, (int)(penX + 0.5f), posY, fontType, flags);
				sDecalFont_TtfRetailCharacter = false;
			}
			penX += advance;
			continue;
		}

		// Room for this glyph, plus the atlas packet and the closing reset.
		const size_t needed = sizeof(POLY_GT4) + 2 * sizeof(DR_PSYX_TEX);
		if ((size_t)((u8 *)P32_GET(void *, primMem->end) - (u8 *)P32_GET(void *, primMem->cursor)) < needed)
		{
			break;
		}

		if (chain.head == NULL)
		{
			chain.head = (DR_PSYX_TEX *)P32_GET(void *, primMem->cursor);
			SetPsyXTexture(chain.head, NativeFont_GetAtlasTexture(), NATIVE_FONT_ATLAS_UNITS, NATIVE_FONT_ATLAS_UNITS);
			chain.head->code[1] |= PSYX_TEX_FLAG_TEXT_SDF;
			chain.tailTag = &chain.head->tag;
			chain.tailLength = 2;
			primMem->cursor = chain.head + 1;
		}

		// Tabular digits sit centred in the shared digit advance.
		const float glyphX = penX + (advance - glyph->advance * emX) * 0.5f;
		const float quadLeft = glyphX + cell->left * emX;
		const float quadRight = glyphX + cell->right * emX;
		const float boxWidth = advance > 0.0f ? advance : 1.0f;
		const float fx0 = (quadLeft - penX) / boxWidth;
		const float fx1 = (quadRight - penX) / boxWidth;
		const u32 *color = P32_GET(u32 *, data.ptrColor[flags]);

		POLY_GT4 *p = (POLY_GT4 *)P32_GET(void *, primMem->cursor);
		setInt32RGB4(p, DecalFont_TtfCornerColor(color, fx0, fy0), DecalFont_TtfCornerColor(color, fx1, fy0),
		             DecalFont_TtfCornerColor(color, fx0, fy1), DecalFont_TtfCornerColor(color, fx1, fy1));
		setPolyGT4(p);
		setXY4(p, (s16)(quadLeft + 0.5f), (s16)(quadTop + 0.5f), (s16)(quadRight + 0.5f), (s16)(quadTop + 0.5f),
		       (s16)(quadLeft + 0.5f), (s16)(quadBottom + 0.5f), (s16)(quadRight + 0.5f), (s16)(quadBottom + 0.5f));
		NativePgxp_SetScreenXY(&p->x0, quadLeft, quadTop);
		NativePgxp_SetScreenXY(&p->x1, quadRight, quadTop);
		NativePgxp_SetScreenXY(&p->x2, quadLeft, quadBottom);
		NativePgxp_SetScreenXY(&p->x3, quadRight, quadBottom);
		setUV4(p, glyph->u0, glyph->v0, glyph->u1, glyph->v0, glyph->u0, glyph->v1, glyph->u1, glyph->v1);
		p->tpage = tpage;
		p->clut = 0;
		DecalFont_TtfChainAppend(&chain, p, &p->tag, 12);
		primMem->cursor = p + 1;

		penX += advance;
	}

	if (chain.head != NULL)
	{
		u32 *ot = P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT);
		DR_PSYX_TEX *resetTexture = (DR_PSYX_TEX *)P32_GET(void *, primMem->cursor);
		SetPsyXTexture(resetTexture, 0, 0, 0);
		DecalFont_TtfChainAppend(&chain, resetTexture, &resetTexture->tag, 2);
		resetTexture->tag = CtrGpu_PackOTTag(*ot, 2 << 24);
		*ot = CtrGpu_PrimToOTLink24(chain.head);
		primMem->cursor = resetTexture + 1;
	}
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800223f4-0x800224d0.
int DecalFont_GetLineWidthStrlen(char *character, int len, int fontType)
{
	s16 font_charPixWidth;
	s16 font_buttonPixWidth;
	s16 font_puncPixWidth;
	int pixLength;
	u8 c;
#if BUILD == JpnRetail
	u32 isRacingWheel;
#endif

#if DECAL_FONT_TTF
	if (DecalFont_TtfActive())
	{
		return DecalFont_TtfLineWidth(character, len, fontType);
	}
#endif

	font_charPixWidth = data.font_charPixWidth[fontType];
	font_buttonPixWidth = data.font_buttonPixWidth[fontType];
	font_puncPixWidth = data.font_puncPixWidth[fontType];
	pixLength = 0;

	while ((*character != 0) && (len != 0))
	{
		c = *character;

		// do not use "switch" or "else if" that increases the number of bytes, and makes the function too large

		// if the character is one of the PSX buttons
		// @ is circle, [ is square, ^ is triangle, * is cross
		if ((c == '@') || (c == '[') || (c == '^') || (c == '*'))
		{
#if BUILD == JpnRetail

			isRacingWheel = DecalFont_boolRacingWheel();

			if ((isRacingWheel & 0xffff) == 0)
				pixLength += font_buttonPixWidth;
			else
				pixLength += font_charPixWidth;

#else

			// character width, plus extra spacing for button
			pixLength += font_buttonPixWidth; // + font_charPixWidth

#endif
		}

		// colon or period
		if ((c == ':') || (c == '.'))
		{
			// punctuation spacing
			pixLength += font_puncPixWidth - font_charPixWidth; // + font_charPixWidth
		}

#if BUILD > UsaRetail
		if (c == '~')
		{
			character += 2;
			len -= 2;

			// dont add charPixWidth
			goto NextIteration;
		}
#endif

// if normal character
#if BUILD != EurRetail
		if (c > 2)
#else
		if (c > 3)
#endif
		{
			// normal character spacing
			// this will be added on top of button,
			// and colon, and period, so dont "else if"
			pixLength += font_charPixWidth;
		}

#if BUILD >= JpnTrial
	NextIteration:
#endif
		character++;
		len--;
	}

	return CTR_WIDESCREEN_SCALE_X(pixLength);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800224d0-0x800224fc.
int DecalFont_GetLineWidth(char *str, s16 fontType)
{
	return (s16)DecalFont_GetLineWidthStrlen(str, -1, fontType);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800224fc-0x80022878 for the retail path.
void DecalFont_DrawLineStrlen(char *str, s16 len, int posX, s16 posY, s16 fontType, int flags)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

#if DECAL_FONT_TTF
	if (DecalFont_TtfActive())
	{
		DecalFont_TtfDrawLine(str, len, posX, posY, fontType, flags);
		return;
	}
#endif

	// text is justified left by default
	if (flags & (JUSTIFY_CENTER | JUSTIFY_RIGHT))
	{
		int alignX = DecalFont_GetLineWidthStrlen(str, len, fontType);

		if (flags & JUSTIFY_CENTER)
		{
			alignX /= 2;
		}

		posX -= alignX;
	}

#if CTR_NATIVE_WIDESCREEN
	const int widescreenLineStartX = posX;
	int widescreenPenX = 0;
#define DECAL_FONT_DRAW_X(extra) (widescreenLineStartX + CTR_WIDESCREEN_SCALE_X(widescreenPenX + (extra)))
#else
#define DECAL_FONT_DRAW_X(extra) (posX + (extra))
#endif

// bug fix exclusive to versions after USA Retail
#if BUILD >= JpnTrial

	flags &= 0x7ff;

#else

	flags &= 0xfff;

#endif

	for (; *str != 0 && len != 0; str++, len--)
	{
		u8 *strcopy = (u8 *)str;
		u16 iconID = 0xff;
		s16 charWidth = data.font_charPixWidth[fontType];
		s16 pixWidthExtra = 0;
		s16 pixHeightExtra = 0;
		s16 iconScale = FP(1.0);

#if BUILD == EurRetail

		int numCharacters = 1;
		b32 upsideDownCharacter = false;

#endif

		u32 *ptrColor = P32_GET(u32 *, data.ptrColor[flags]);

#if BUILD >= JpnTrial

		if (*strcopy == '~')
		{
			// if the current character in the string is a tilde, delete the next two characters and color the rest of the word depending on the characters
			// being deleted used with numbers according to the color ID, e.g. ~01 gives the blue-ish gray color seen in the race lap count

			u8 *strnext = (u8 *)str + 1;
			u8 *strnextnext = (u8 *)str + 2;
			str += 2;
			len -= 2;
			flags = (*strnextnext + (*strnext - 0x30) * 10) - 0x30;
			charWidth = 0;

			continue;
		}

#endif

		if (*strcopy == ':' || *strcopy == '.')
		{
			charWidth = data.font_puncPixWidth[fontType];
		}

		// if the character is one of the PSX buttons
		// @ is circle, [ is square, ^ is triangle, * is X
		if ((((*strcopy == '@') || (*strcopy == '[')) || (*strcopy == '^')) || (*strcopy == '*'))
		{
#if BUILD != JpnRetail

			iconScale = data.font_buttonScale[fontType];
			pixHeightExtra = data.font_buttonPixHeight[fontType];
			charWidth = data.font_charPixWidth[fontType] + data.font_buttonPixWidth[fontType];

			// use neutral vertex color for button characters
			ptrColor = P32_GET(u32 *, data.ptrColor[GRAY]);

#else

			// japan retail adds support for the Mad Catz MC2 Racing Wheel (probably the best thing Naughty Dog added to this version to be honest)
			// this replaces the regular face button characters with ones that match the buttons on the steering wheel and recolors them accordingly

			s16 isRacingWheel = DecalFont_boolRacingWheel();

			if (!(isRacingWheel))
			{
				iconScale = data.font_buttonScale[fontType];
				pixHeightExtra = data.font_buttonPixHeight[fontType];
				charWidth = data.font_charPixWidth[fontType] + data.font_buttonPixWidth[fontType];

				// use neutral vertex color for button characters
				ptrColor = data.ptrColor[GRAY];
			}
			else
			{
				// note: the Mad Catz MC2 Racing Wheel does not use the traditional PSX buttons
				// it uses A, B, 1, and 2 instead
				int racingWheelButtonColor = PERIWINKLE;

				if (*strcopy == '@')
					*strcopy = 'A';
				if (*strcopy == '^')
					*strcopy = 'B';

				if (*strcopy == '*' || *strcopy == '[')
				{
					if (*strcopy == '*')
						*strcopy = '1';
					if (*strcopy == '[')
						*strcopy = '2';

					racingWheelButtonColor = RED;
				}

				ptrColor = data.ptrColor[racingWheelButtonColor];
			}

#endif
		}

		// Set character sprite (icon) IDs
		// The first 0x21 (counting 0) ASCII characters don't have icon IDs assigned to them
		// Europe has additional characters composed of existing characters rearranged to resemble new ones
		// USA Retail and prototypes have placeholder data for additional characters found in the Japanese versions

#if BUILD != EurRetail

		// ascii characters + japanese characters
		// 0xE0 characters from 0x20 to 0x100
		// TO DO: figure out why the cast to u32 is necessary --Super
		if (((u32)*strcopy - 0x21) < 0xdf)
		{
			// get iconID based on ascii character
			iconID = data.font_characterIconID[*strcopy - 0x21];
		}

		// Japanese dakuten and handakuten
		// unused in NTSC-U and PAL
		if (*strcopy < 3)
		{
			charWidth = 0;
			iconID = data.font_indentIconID[fontType * 2 + *strcopy - 1];
			pixWidthExtra = data.font_indentPixDimensions[fontType * 2];
			pixHeightExtra = data.font_indentPixDimensions[(fontType * 2) + 1];
		}

#else

		// europe only has 0x60 characters
		if (((u32)*strcopy - 0x21) < 0x5f)
		{
			// get iconID based on ascii character
			iconID = data.font_characterIconID[*strcopy - 0x21];
		}

		if (*strcopy == '')
		{
			// The tilde as a diacritical mark
			// It's implemented in CTR using the underscore character, shrunk down and placed above the character that follows
			// Seen in the Spanish language for the Ñ, although because of the above mentioned you can use it on anything

			iconID = 0x2f;
			pixWidthExtra = data.font_EurDiacriticalTilde[fontType * 3];
			pixHeightExtra = data.font_EurDiacriticalTilde[(fontType * 3) + 1];
			iconScale = data.font_EurDiacriticalTilde[(fontType * 3) + 2];
			charWidth = 0;
		}
		if (*strcopy == '"')
		{
			// Quotation mark
			// Unlike the other new characters its representing character is actually used for a quotation mark
			// Quotation marks in the PAL version are two apostrophes joined together
			iconID = 0x24;
			numCharacters = 2;
			pixWidthExtra = data.font_EurQuotationMarkWidth[fontType];
		}

		// the following characters make use of additional width and height padding
		if (*strcopy < 3 || *strcopy == '#' || *strcopy == '$' || *strcopy == '&')
		{
			s16 *characterExtraDimensionsArray = 0;

			if (*strcopy < 3)
			{
				// Japanese dakuten and handakuten
				// unused in NTSC-U and PAL
				charWidth = 0;
				iconID = data.font_indentIconID[fontType * 2 + *strcopy - 1];
				characterExtraDimensionsArray = data.font_indentPixDimensions;
			}
			if (*strcopy == '#')
			{
				// Inverted exclamation mark, used in the Spanish language
				upsideDownCharacter = true;
				iconID = 0x25;
				characterExtraDimensionsArray = data.font_EurInvertedExclamationMarkData;
			}
			if (*strcopy == '$')
			{
				// Inverted question mark, used in the Spanish language
				upsideDownCharacter = true;
				iconID = 0x2e;
				characterExtraDimensionsArray = data.font_EurInvertedExclamationMarkData;
			}
			if (*strcopy == '&')
			{
				// Upwards period
				// Supposed to be the ordinal indicator character in Spanish and French
				iconID = 0x2c;
				characterExtraDimensionsArray = data.font_EurOrdinalIndicatorData;
			}

			pixWidthExtra = characterExtraDimensionsArray[fontType * 2];
			pixHeightExtra = characterExtraDimensionsArray[(fontType * 2) + 1];
		}

#endif

		// if iconID is valid
		if (iconID != 0xff)
		{
			s16 iconGroupID = data.font_IconGroupID[fontType];

#if BUILD <= UsaRetail

			// incomplete implementation of japanese font, unused
			// see below for more details
			if (iconID > 0x7f)
			{
				iconID -= 0x80;
				s16 kanaIconGroupID = 15;
				if (iconGroupID == 4)
				{
					kanaIconGroupID = 14;
				}
				iconGroupID = kanaIconGroupID;
			}

#elif BUILD == JpnTrial || BUILD == JpnRetail

			// defined here as a fallback for if the character in question isn't kana
			u16 kanaID = iconID;

			struct Icon *iconStruct = 0;

			// if icon ID goes over 0x7f, then this is a japanese character (i.e. kana)
			if (iconID > 0x7f)
			{
				// kana icon IDs are in a separate icon group from other font characters
				u16 kanaID = iconID - 0x80;

				// the "big" and "small" font icon groups for kana are 14 and 15 respectively
				s16 kanaIconGroupID = 15;

				// if icon group is non-japanese big font, set to japanese
				if (iconGroupID == 4)
					kanaIconGroupID = 14;

				iconGroupID = kanaIconGroupID;

				iconStruct = &sdata->font_icon;

				if (kanaIconGroupID == 14)
				{
					sdata->font_icon.texLayout.clut = sdata->font_jfontBigIconData.clut;

					if (kanaID & 1)
						sdata->font_icon.texLayout.clut += 0x40;

					sdata->font_icon.texLayout.u0 = kanaID * 8 & 0xf0;
					sdata->font_icon.texLayout.u1 = sdata->font_icon.texLayout.u0 + 15;

					s16 whateverThisIs_big = kanaID / 2 & 0x10;

					sdata->font_icon.texLayout.v0 = whateverThisIs_big + 8;
					sdata->font_icon.texLayout.v2 = whateverThisIs_big + 23;

					sdata->font_icon.texLayout.tpage = sdata->font_jfontBigIconData.tpage + ((kanaID < 0x40) ^ 1);

					sdata->font_icon.texLayout.v1 = sdata->font_icon.texLayout.v0;
					sdata->font_icon.texLayout.u2 = sdata->font_icon.texLayout.u0;
					sdata->font_icon.texLayout.u3 = sdata->font_icon.texLayout.u1;
					sdata->font_icon.texLayout.v3 = sdata->font_icon.texLayout.v2;
				}
				else // i.e. small font
				{
					if (kanaID < 24)
					{
						sdata->font_icon.texLayout.clut = sdata->font_jfontSmallIconData.clut;

						if (kanaID & 1)
							sdata->font_icon.texLayout.clut += 0x40;

						sdata->font_icon.texLayout.tpage = sdata->font_jfontSmallIconData.tpage;

						s16 whateverThisIs_small = ((kanaID & 0xfe) + kanaID / 2) * 4; // this is the one

						if (kanaID < 12)
							sdata->font_icon.texLayout.u0 = whateverThisIs_small + 176;
						else
							sdata->font_icon.texLayout.u0 = whateverThisIs_small + 104;

						sdata->font_icon.texLayout.u1 = sdata->font_icon.texLayout.u0 + 11;

						if (kanaID < 12)
							sdata->font_icon.texLayout.v0 = 24;
						else
							sdata->font_icon.texLayout.v0 = 32;

						sdata->font_icon.texLayout.v2 = sdata->font_icon.texLayout.v0 + 7;

						sdata->font_icon.texLayout.v1 = sdata->font_icon.texLayout.v0;
						sdata->font_icon.texLayout.u2 = sdata->font_icon.texLayout.u0;
						sdata->font_icon.texLayout.u3 = sdata->font_icon.texLayout.u1;
						sdata->font_icon.texLayout.v3 = sdata->font_icon.texLayout.v2;
					}
					else
					{
						kanaID = (u8)iconID - 0x98; // if you remove the u8 cast the dakuten and handakuten diacritics break. why??? --Super
						sdata->font_icon.texLayout.clut = sdata->font_jfontSmall0x18IconData.clut;

						if (kanaID & 1)
							sdata->font_icon.texLayout.clut += 0x40;

						sdata->font_icon.texLayout.tpage = sdata->font_jfontSmall0x18IconData.tpage;

						sdata->font_icon.texLayout.u0 = data.font_X1Y1data[kanaID * 2 + 1] / 2 * 12;
						sdata->font_icon.texLayout.u1 = sdata->font_icon.texLayout.u0 + 11;

						sdata->font_icon.texLayout.v0 = data.font_X1Y1data[kanaID * 2] * 8 + 8;
						sdata->font_icon.texLayout.v2 = data.font_X1Y1data[kanaID * 2] * 8 + 15;

						sdata->font_icon.texLayout.v1 = sdata->font_icon.texLayout.v0;
						sdata->font_icon.texLayout.u2 = sdata->font_icon.texLayout.u0;
						sdata->font_icon.texLayout.u3 = sdata->font_icon.texLayout.u1;
						sdata->font_icon.texLayout.v3 = sdata->font_icon.texLayout.v2;
					}
				}
			}

#endif

#if BUILD <= UsaRetail

// NOTE(aalhendi): Native can boot before every retail icon group is loaded.
#ifdef CTR_NATIVE
			if (P32_GET(struct IconGroup *, gGT->iconGroup[iconGroupID]) != 0)
			{
#endif

				if (iconID < P32_GET(struct IconGroup *, gGT->iconGroup[iconGroupID])->numIcons)
				{
					P32(struct Icon *) *iconPtrArray = ICONGROUP_GETICONS(P32_GET(struct IconGroup *, gGT->iconGroup[iconGroupID]));

					DecalFont_DrawGlyph(*strcopy, iconPtrArray[iconID],

					                     DECAL_FONT_DRAW_X(pixWidthExtra), posY + pixHeightExtra,

					                     &P32_GET(struct DB *, gGT->backBuffer)->primMem, P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT),

					                     ptrColor[0], ptrColor[1], ptrColor[2], ptrColor[3],

					                     0, iconScale);
				}
			}

#elif BUILD == JpnTrial || BUILD == JpnRetail

			if (iconStruct == 0)
			{
				P32(struct Icon *) *iconPtrArray = ICONGROUP_GETICONS(gGT->iconGroup[iconGroupID]);
				if (kanaID < gGT->iconGroup[iconGroupID]->numIcons)
					iconStruct = P32_GET(struct Icon *, iconPtrArray[kanaID]);
			}
			if (iconStruct != 0)
			{
				DecalFont_DrawGlyph(*strcopy, iconStruct,

				                     DECAL_FONT_DRAW_X(pixWidthExtra), posY + pixHeightExtra,

				                     &gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT,

				                     ptrColor[0], ptrColor[1], ptrColor[2], ptrColor[3],

				                     0, iconScale);
			}

#else // i.e. european build

			P32(struct Icon *) *iconPtrArray = ICONGROUP_GETICONS(gGT->iconGroup[iconGroupID]);

			for (; numCharacters > 0; numCharacters--, pixWidthExtra += data.font_EurPixWidthExtra[fontType])
			{
				if (upsideDownCharacter)
				{
					DecalHUD_Arrow2D(P32_GET(struct Icon *, iconPtrArray[iconID]),

					                 DECAL_FONT_DRAW_X(pixWidthExtra), posY + pixHeightExtra,

					                 &gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT,

					                 ptrColor[2], ptrColor[3], ptrColor[0], ptrColor[1],

					                 0, iconScale, 0x800);
				}
				else
				{
					DecalFont_DrawGlyph(*strcopy, iconPtrArray[iconID],

					                     DECAL_FONT_DRAW_X(pixWidthExtra), posY + pixHeightExtra,

					                     &gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT,

					                     ptrColor[0], ptrColor[1], ptrColor[2], ptrColor[3],

					                     0, iconScale);
				}
			}

#endif
		}
#if CTR_NATIVE_WIDESCREEN
		widescreenPenX += charWidth;
		posX = widescreenLineStartX + CTR_WIDESCREEN_SCALE_X(widescreenPenX);
#else
		posX += charWidth;
#endif
	}

#undef DECAL_FONT_DRAW_X
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80022878-0x800228c4.
void DecalFont_DrawLine(char *str, int posX, int posY, s16 fontType, int flags)
{
	DecalFont_DrawLineStrlen(str, -1, (s16)posX, (s16)posY, fontType, (s16)flags);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800228c4-0x80022930.
void DecalFont_DrawLineOT(char *str, int posX, int posY, s16 fontType, int flags, uint32_t *ot)
{
	struct GameTracker *gGT;
	uint32_t *backupOT;

	gGT = P32_GET(struct GameTracker *, sdata->gGT);

	// backup
	backupOT = P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT);

	// alter
	P32_SET(gGT->pushBuffer_UI.ptrOT, ot);

	// draw
	DecalFont_DrawLine(str, (s16)posX, (s16)posY, fontType, (s16)flags);

	// reset
	P32_SET(gGT->pushBuffer_UI.ptrOT, backupOT);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80022930-0x80022b34.
int DecalFont_DrawMultiLineStrlen(char *str, s16 len, s16 posX, s16 posY, s16 maxPixLen, s16 fontType, s16 flags)
{
	char strCharacter;
	s16 lineLen;
	char *currPointer;
	s16 lettersRemaining;
	char *strPointer;
	int totalPassageHeight;

	totalPassageHeight = 0;

	do
	{
		// pointer to string
		strPointer = str;

		// rather than using \n for new lines, CTR uses \r, which is similar if you try it with printf

		// while you've not reached the end of the line
		if (*str != '\r')
		{
			// get the first character
			strCharacter = *str;

			while (1)
			{
				// pointer to current letter
				currPointer = strPointer;

				// number of letters remaining
				lettersRemaining = len;

				// if you reached a space, and you're
				// not out of letters yet
				if ((strCharacter == ' ') && (len != 0))
				{
					// increment pointer to next letter
					currPointer = strPointer + 1;

					// one letter less
					lettersRemaining = len - 1;
				}

				// get next character
				strCharacter = *currPointer;

				// if nullptr, or out of letters, quit the loop
				if ((strCharacter == '\0') || (lettersRemaining == 0))
				{
					break;
				}

				// if this is a letter, number, or symbol
				if ((strCharacter != ' ') && (strCharacter != '\r'))
				{
					// get the length of the next word
					while (lettersRemaining != 0)
					{
						// increment pointer to next letter
						currPointer = currPointer + 1;

						// get value of next character
						strCharacter = *currPointer;

						// reduce number of remaining characters
						lettersRemaining = lettersRemaining + -1;

						// stop counting at a nullptr,
						// or a space (end of word),
						// or the end of the line '\r'
						if (((strCharacter == '\0') || (strCharacter == ' ')) || (strCharacter == '\r'))
						{
							break;
						}
					}
				}

				lineLen = DecalFont_GetLineWidthStrlen(str, (u32)(currPointer - str), (int)fontType);

				if (
				    // if parameter line length is longer than string line length
				    (maxPixLen <= lineLen) || (
				                                  // get character
				                                  strCharacter = *currPointer,

				                                  // update pointer
				                                  strPointer = currPointer,

				                                  // update number of remaining characters
				                                  len = lettersRemaining,

				                                  // check if this is new line
				                                  strCharacter == '\r'))
				{
					break;
				}
			}
		}

#if BUILD > UsaRetail
		if (!(flags & 0x800))
#endif
			DecalFont_DrawLineStrlen(str, (u32)(strPointer - str), (int)posX, posY + totalPassageHeight, (int)fontType, (int)flags);

#if BUILD > SepReview

		totalPassageHeight += data.font_charPixHeight[fontType];

#else

		// did font_charPixHeight not exist in 903?

		lettersRemaining = (s16)totalPassageHeight;
		totalPassageHeight = (int)lettersRemaining + 8;
		if ((int)fontType == 1)
		{
			totalPassageHeight = (int)lettersRemaining + 0x11;
		}
		strCharacter = *strPointer;
		str = strPointer;
		if (strCharacter == '\r')
		{
			if (len != 0)
			{
				strPointer++;
				len--;
			}
			strCharacter = *strPointer;
			str = strPointer;
		}

#endif

		if (*strPointer == '\0')
		{
		EndFunction:
			return totalPassageHeight;
		}

		if (len != 0)
		{
			strPointer = strPointer + 1;
			len = len + -1;
		}
		if ((*strPointer == '\0') || (str = strPointer, len == 0))
		{
			goto EndFunction;
		}
	} while (1);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80022b34-0x80022b94.
int DecalFont_DrawMultiLine(char *str, int posX, int posY, int maxPixLen, s16 fontType, int flags)
{
	return (s16)DecalFont_DrawMultiLineStrlen(str, -1, (s16)posX, (s16)posY, (s16)maxPixLen, fontType, (s16)flags);
}
