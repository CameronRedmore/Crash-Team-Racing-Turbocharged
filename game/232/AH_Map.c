#include <common.h>

#if defined(CTR_NATIVE)
#include "platform/native_pgxp.h"
#include "platform/native_minimap.h"
#include <math.h>
#include <string.h>
#endif

enum AHMapIconID
{
	AH_MAP_ICON_WARPPAD = 0x31,
	AH_MAP_ICON_BOSS_STAR = 0x37,
};

enum AHMapArrowType
{
	AH_MAP_ARROW_WARPPAD_TROPHY = 0,
	AH_MAP_ARROW_HUB_ROUTE,
	AH_MAP_ARROW_BOSS,
};

enum AHMapColor
{
	AH_MAP_COLOR_COMPLETE = RED,
	AH_MAP_COLOR_FLASH_PRIMARY = CRASH_BLUE,
	AH_MAP_COLOR_FLASH_SECONDARY = WHITE,
	AH_MAP_COLOR_RELIC_TOKEN = PAPU_YELLOW,
	AH_MAP_COLOR_INVALID = BLACK,
	AH_MAP_COLOR_LOCKED = GRAY,
};

CTR_STATIC_ASSERT(AH_MAP_ICON_WARPPAD == 0x31);
CTR_STATIC_ASSERT(AH_MAP_ICON_BOSS_STAR == 0x37);
CTR_STATIC_ASSERT(AH_MAP_ARROW_WARPPAD_TROPHY == 0);
CTR_STATIC_ASSERT(AH_MAP_ARROW_HUB_ROUTE == 1);
CTR_STATIC_ASSERT(AH_MAP_ARROW_BOSS == 2);
CTR_STATIC_ASSERT(AH_MAP_COLOR_COMPLETE == 3);
CTR_STATIC_ASSERT(AH_MAP_COLOR_FLASH_PRIMARY == 5);
CTR_STATIC_ASSERT(AH_MAP_COLOR_FLASH_SECONDARY == 4);
CTR_STATIC_ASSERT(AH_MAP_COLOR_RELIC_TOKEN == 0xe);
CTR_STATIC_ASSERT(AH_MAP_COLOR_INVALID == 0x15);
CTR_STATIC_ASSERT(AH_MAP_COLOR_LOCKED == 0x17);

enum AHMapBossItemState
{
	AH_MAP_BOSS_ITEM_NONE = -1,
	AH_MAP_BOSS_ITEM_LOCKED = 0,
	AH_MAP_BOSS_ITEM_OPEN = 1,
	AH_MAP_BOSS_ITEM_COMPLETE = 2,
};

CTR_STATIC_ASSERT(AH_MAP_BOSS_ITEM_NONE == -1);
CTR_STATIC_ASSERT(AH_MAP_BOSS_ITEM_LOCKED == 0);
CTR_STATIC_ASSERT(AH_MAP_BOSS_ITEM_OPEN == 1);
CTR_STATIC_ASSERT(AH_MAP_BOSS_ITEM_COMPLETE == 2);

enum AHMapMainConstants
{
	AH_MAP_HUD_AND_DEBUG_SPEEDOMETER = 0x8,
	AH_MAP_ICON_TOP_HALF = 3,
	AH_MAP_ICON_BOTTOM_HALF = 4,
	AH_MAP_SCREEN_POS_X = 500,
	AH_MAP_SCREEN_POS_Y = 195,
	AH_MAP_HUD_SLOT_SLIDE_METER = 8,
	AH_MAP_HUD_SLOT_RELIC_COUNT = 0xe,
	AH_MAP_HUD_SLOT_KEY_COUNT = 0xf,
	AH_MAP_HUD_SLOT_TROPHY_COUNT = 0x10,
	AH_MAP_HUD_COUNTER_OFFSET_X = 0x10,
	AH_MAP_HUD_COUNTER_OFFSET_Y = -10,
};

enum AHMapArrowOutlineConstants
{
	AH_MAP_ARROW_OUTLINE_COUNT = 3,
	AH_MAP_ARROW_OUTLINE_PHASE_STEP = 0xc,
	AH_MAP_ARROW_OUTLINE_PHASE_MASK = 0x3f,
	AH_MAP_ARROW_OUTLINE_PHASE_DELAY = 6,
	AH_MAP_ARROW_OUTLINE_VISIBLE_PHASES = 0xc,
	AH_MAP_ARROW_OUTLINE_RADIUS_MUL = 0x2aa,
	AH_MAP_ARROW_OUTLINE_RADIUS_BIAS = FP_ONE,
	AH_MAP_ARROW_OUTLINE_RADIUS_SHIFT = 0x1a,
	AH_MAP_ARROW_OUTLINE_TROPHY_STEP = 0x200,
	AH_MAP_ARROW_OUTLINE_ROUTE_STEP = 0x555,
	AH_MAP_ARROW_OUTLINE_BOSS_STEP = 0x199,
};

CTR_STATIC_ASSERT(AH_MAP_HUD_AND_DEBUG_SPEEDOMETER == 0x8);
CTR_STATIC_ASSERT(AH_MAP_ICON_TOP_HALF == 3);
CTR_STATIC_ASSERT(AH_MAP_ICON_BOTTOM_HALF == 4);
CTR_STATIC_ASSERT(AH_MAP_SCREEN_POS_X == 500);
CTR_STATIC_ASSERT(AH_MAP_SCREEN_POS_Y == 195);
CTR_STATIC_ASSERT(AH_MAP_HUD_SLOT_SLIDE_METER == 8);
CTR_STATIC_ASSERT(AH_MAP_HUD_SLOT_RELIC_COUNT == 0xe);
CTR_STATIC_ASSERT(AH_MAP_HUD_SLOT_KEY_COUNT == 0xf);
CTR_STATIC_ASSERT(AH_MAP_HUD_SLOT_TROPHY_COUNT == 0x10);
CTR_STATIC_ASSERT(AH_MAP_HUD_COUNTER_OFFSET_X == 0x10);
CTR_STATIC_ASSERT(AH_MAP_HUD_COUNTER_OFFSET_Y == -10);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_COUNT == 3);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_PHASE_STEP == 0xc);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_PHASE_MASK == 0x3f);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_PHASE_DELAY == 6);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_VISIBLE_PHASES == 0xc);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_RADIUS_MUL == 0x2aa);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_RADIUS_BIAS == 0x1000);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_RADIUS_SHIFT == 0x1a);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_TROPHY_STEP == 0x200);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_ROUTE_STEP == 0x555);
CTR_STATIC_ASSERT(AH_MAP_ARROW_OUTLINE_BOSS_STEP == 0x199);

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b0b98-0x800b0ce0.
void AH_Map_LoadSave_Prim(const SVec2 *vertPos, char *vertCol, void *ot, struct PrimMem *primMem)
{
	POLY_G4 *p = P32_GET(void *, primMem->cursor);

	if (P32_GET(void *, primMem->end) < (void *)p)
	{
		return;
	}

	P32_SET(primMem->cursor, p + 1);

	setPolyG4(p);

	p->r0 = vertCol[0];
	p->g0 = vertCol[1];
	p->b0 = vertCol[2];

	p->r1 = vertCol[4];
	p->g1 = vertCol[5];
	p->b1 = vertCol[6];

	p->r2 = vertCol[8];
	p->g2 = vertCol[9];
	p->b2 = vertCol[10];

	p->r3 = vertCol[12];
	p->g3 = vertCol[13];
	p->b3 = vertCol[14];

	p->x0 = vertPos[0].x;
	p->y0 = vertPos[0].y;

	p->x1 = vertPos[1].x;
	p->y1 = vertPos[1].y;

	p->x2 = vertPos[2].x;
	p->y2 = vertPos[2].y;

	p->x3 = vertPos[3].x;
	p->y3 = vertPos[3].y;

	AddPrim(ot, p);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b0ce0-0x800b0f18.
void AH_Map_LoadSave_Full(int posX, int posY, const SVec2 *vertPos, char *vertCol, int scale, int angle)
{
	SVec2 basePos[4];
	SVec2 drawPos[4];

	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	int sin = MATH_Sin(angle);
	int cos = MATH_Cos(angle);

	for (int i = 0; i < 4; i++)
	{
		basePos[i].x = posX + 6 +
		               (s16)(((((vertPos[i].x * cos) >> 0xc) + ((vertPos[i].y * sin) >> 0xc)) * ((scale * 8) / 5)

		                          ) >>
		                     0xc);

		basePos[i].y = posY + 4 +
		               (s16)(((((vertPos[i].y * cos) >> 0xc) - ((vertPos[i].x * sin) >> 0xc)) * scale

		                      ) >>
		                     0xc);
	}

	const SVec2 *offset = &D232.loadSavePrimOffset[0];

	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			drawPos[j].x = basePos[j].x + offset[i].x;

			drawPos[j].y = basePos[j].y + offset[i].y;
		}

		AH_Map_LoadSave_Prim(&drawPos[0], vertCol, P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), &P32_GET(struct DB *, gGT->backBuffer)->primMem);

		vertCol = (char *)&D232.colorQuad[0];
	}
}

#if defined(CTR_NATIVE)
enum AHMapPreciseConstants
{
	AH_MAP_PRECISE_MAX_VERTS = 4,
};

static void AH_Map_PreciseSetXY(s16 *xy, float x, float y)
{
	xy[0] = (s16)x;
	xy[1] = (s16)y;
	NativePgxp_SetScreenXY(xy, x, y);
}

// Rotate and scale a hub map marker exactly as the retail integer version does,
// keeping the fractions.
static float AH_Map_ShapeAspectX(void)
{
	return NativeAspect_IsActive() ? (float)NativeAspect_GetHudPixelAspectX() : 8.0f / 5.0f;
}

static void AH_Map_PreciseShape(float posX, float posY, const SVec2 *vertPos, int count, int scale, float sizeScale, int angle, float *x, float *y)
{
	const float sin = MATH_Sin(angle) / 4096.0f;
	const float cos = MATH_Cos(angle) / 4096.0f;
	const float scaleY = scale * sizeScale / 4096.0f;
	const float scaleX = scaleY * AH_Map_ShapeAspectX();
	for (int i = 0; i < count; i++)
	{
		x[i] = posX + 6 + (vertPos[i].x * cos + vertPos[i].y * sin) * scaleX;
		y[i] = posY + 4 + (vertPos[i].y * cos - vertPos[i].x * sin) * scaleY;
	}
}

// Grow a convex outline (vertices in perimeter order) by one pixel with mitred
// corners, replacing the retail five-copy offset outline.
static void AH_Map_PreciseOutline(const float *x, const float *y, int count, float *outlineX, float *outlineY)
{
	const float pixelAspectX = NativeAspect_IsActive() ? (float)NativeAspect_GetHudPixelAspectX() : 1.0f;
	float signedArea = 0.0f;
	for (int i = 0; i < count; i++)
	{
		const int next = (i + 1) % count;
		signedArea += x[i] * y[next] - x[next] * y[i];
	}
	const float normalDirection = (signedArea < 0.0f) ? 1.0f : -1.0f;
	const float outlineThickness = 1.0f;
	const float maxMiterLength = outlineThickness * 3.0f;
	for (int i = 0; i < count; i++)
	{
		const int previous = (i + count - 1) % count;
		const int next = (i + 1) % count;
		const float prevDX = (x[i] - x[previous]) / pixelAspectX;
		const float prevDY = y[i] - y[previous];
		const float nextDX = (x[next] - x[i]) / pixelAspectX;
		const float nextDY = y[next] - y[i];
		const float prevLength = sqrtf(prevDX * prevDX + prevDY * prevDY);
		const float nextLength = sqrtf(nextDX * nextDX + nextDY * nextDY);
		if (prevLength <= 0.0001f || nextLength <= 0.0001f || fabsf(signedArea) <= 0.0001f)
		{
			outlineX[i] = x[i];
			outlineY[i] = y[i];
			continue;
		}

		const float prevNormalX = normalDirection * -prevDY / prevLength;
		const float prevNormalY = normalDirection * prevDX / prevLength;
		const float nextNormalX = normalDirection * -nextDY / nextLength;
		const float nextNormalY = normalDirection * nextDX / nextLength;
		const float normalDot = prevNormalX * nextNormalX + prevNormalY * nextNormalY;
		const float denominator = 1.0f + normalDot;
		if (denominator <= 0.0001f)
		{
			outlineX[i] = x[i];
			outlineY[i] = y[i];
			continue;
		}

		float miterX = (prevNormalX + nextNormalX) * outlineThickness / denominator;
		float miterY = (prevNormalY + nextNormalY) * outlineThickness / denominator;
		const float miterLength = sqrtf(miterX * miterX + miterY * miterY);
		if (miterLength > maxMiterLength)
		{
			const float miterScale = maxMiterLength / miterLength;
			miterX *= miterScale;
			miterY *= miterScale;
		}
		outlineX[i] = x[i] + miterX * pixelAspectX;
		outlineY[i] = y[i] + miterY;
	}
}

static void AH_Map_HubArrowPreciseTriangle(struct GameTracker *gGT, const float x[3], const float y[3], char *vertCol)
{
	SVec2 drawPos[3];
	for (int i = 0; i < 3; i++)
	{
		drawPos[i].x = (s16)x[i];
		drawPos[i].y = (s16)y[i];
	}

	POLY_G4 *p = P32_GET(void *, P32_GET(struct DB *, gGT->backBuffer)->primMem.cursor);
	RECTMENU_DrawRwdTriangle(drawPos[0].v, vertCol, P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), &P32_GET(struct DB *, gGT->backBuffer)->primMem);
	if (P32_GET(void *, P32_GET(struct DB *, gGT->backBuffer)->primMem.cursor) == p + 1)
	{
		// Use the same first point twice so the precise triangle has no pixel-sized split.
		NativePgxp_SetScreenXY(&p->x0, x[0], y[0]);
		NativePgxp_SetScreenXY(&p->x1, x[1], y[1]);
		NativePgxp_SetScreenXY(&p->x2, x[0], y[0]);
		NativePgxp_SetScreenXY(&p->x3, x[2], y[2]);
	}
}

// sizeScale shrinks the shape about its pivot; 1.0 keeps the retail size.
void AH_Map_HubArrowPrecise(float posX, float posY, const SVec2 *vertPos, char *vertCol, int scale, int angle, float sizeScale)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	float x[3], y[3];
	AH_Map_PreciseShape(posX, posY, vertPos, 3, scale, sizeScale, angle, x, y);

	float outlineX[3], outlineY[3];
	AH_Map_PreciseOutline(x, y, 3, outlineX, outlineY);

	// OT entries render newest first, so submit the outline after the fill to keep it behind.
	AH_Map_HubArrowPreciseTriangle(gGT, x, y, vertCol);
	AH_Map_HubArrowPreciseTriangle(gGT, outlineX, outlineY, (char *)&D232.colorTri[0]);
}

static void AH_Map_LoadSavePreciseQuad(struct GameTracker *gGT, const float x[4], const float y[4], char *vertCol)
{
	// Perimeter order to the packet's Z order.
	static const int s_cornerOrder[4] = {0, 1, 3, 2};
	SVec2 drawPos[4];
	for (int i = 0; i < 4; i++)
	{
		drawPos[s_cornerOrder[i]].x = (s16)x[i];
		drawPos[s_cornerOrder[i]].y = (s16)y[i];
	}

	POLY_G4 *p = P32_GET(void *, P32_GET(struct DB *, gGT->backBuffer)->primMem.cursor);
	AH_Map_LoadSave_Prim(&drawPos[0], vertCol, P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), &P32_GET(struct DB *, gGT->backBuffer)->primMem);
	if (P32_GET(void *, P32_GET(struct DB *, gGT->backBuffer)->primMem.cursor) == p + 1)
	{
		s16 *corners[4] = {&p->x0, &p->x1, &p->x2, &p->x3};
		for (int i = 0; i < 4; i++)
		{
			NativePgxp_SetScreenXY(corners[s_cornerOrder[i]], x[i], y[i]);
		}
	}
}

void AH_Map_LoadSavePrecise(float posX, float posY, const SVec2 *vertPos, char *vertCol, int scale, int angle)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	const SVec2 perimeter[4] = {vertPos[0], vertPos[1], vertPos[3], vertPos[2]};
	float x[4], y[4];
	AH_Map_PreciseShape(posX, posY, perimeter, 4, scale, gNativeModernMapEnabled ? 1.1f : 1.0f, angle, x, y);

	float outlineX[4], outlineY[4];
	AH_Map_PreciseOutline(x, y, 4, outlineX, outlineY);

	AH_Map_LoadSavePreciseQuad(gGT, x, y, vertCol);
	AH_Map_LoadSavePreciseQuad(gGT, outlineX, outlineY, (char *)&D232.colorQuad[0]);
}

// One-pixel line with square caps, as a quad so both ends keep their fractions.
static void AH_Map_PreciseLine(struct GameTracker *gGT, float x0, float y0, float x1, float y1, Color color)
{
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float length = sqrtf(dx * dx + dy * dy);
	if (length <= 0.0001f)
	{
		return;
	}

	struct PrimMem *primMem = &P32_GET(struct DB *, gGT->backBuffer)->primMem;
	POLY_F4 *p = P32_GET(void *, primMem->cursor);
	if ((void *)p > P32_GET(void *, primMem->guardEnd))
	{
		return;
	}
	P32_SET(primMem->cursor, p + 1);

	// Retail lines fill the pixel to the right of and below each point.
	x0 += 0.5f;
	y0 += 0.5f;
	x1 += 0.5f;
	y1 += 0.5f;
	const float alongX = dx * 0.5f / length;
	const float alongY = dy * 0.5f / length;

	setPolyF4(p);
	setRGB0(p, color.r, color.g, color.b);
	AH_Map_PreciseSetXY(&p->x0, x0 - alongX - alongY, y0 - alongY + alongX);
	AH_Map_PreciseSetXY(&p->x1, x0 - alongX + alongY, y0 - alongY - alongX);
	AH_Map_PreciseSetXY(&p->x2, x1 + alongX - alongY, y1 + alongY + alongX);
	AH_Map_PreciseSetXY(&p->x3, x1 + alongX + alongY, y1 + alongY - alongX);
	AddPrim(P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), p);
}

// Fractional map position for the vector markers, drawn with Precise Minimap
// or Modern Map.
static b32 AH_Map_PrecisePos(struct UIMap *map, int worldX, int worldZ, float *posX, float *posY)
{
	if (!NativeAspect_IsActive() && !(gNativePreciseMinimapEnabled && NATIVE_PGXP_SUPPORTED) && !gNativeModernMapEnabled)
	{
		return false;
	}

	const s32 worldPos[3] = {worldX, 0, worldZ};
	UI_Map_GetIconPosPrecise(map, worldPos, posX, posY);
	return true;
}

// Route items carry their lock rule, while the level trigger payload selects
// the actual connected hub. Keep that relationship explicit per hub.
static int AH_Map_HubRouteTriggerID(int levelID, AdventureHubItemType iconType)
{
	switch (levelID)
	{
	case GEM_STONE_VALLEY:
		if (iconType == AH_HUB_ITEM_ROUTE_OPEN_A) return 1; // N. Sanity Beach
		if (iconType == AH_HUB_ITEM_ROUTE_OPEN_B) return 2; // The Lost Ruins
		break;
	case N_SANITY_BEACH:
		if (iconType == AH_HUB_ITEM_ROUTE_KEY1_IF_BEACH) return 1; // Gem Stone Valley
		if (iconType == AH_HUB_ITEM_ROUTE_KEY2) return 2; // Glacier Park
		break;
	case THE_LOST_RUINS:
		if (iconType == AH_HUB_ITEM_ROUTE_KEY1_IF_BEACH) return 1; // Gem Stone Valley
		if (iconType == AH_HUB_ITEM_ROUTE_KEY2) return 2; // Glacier Park
		break;
	case GLACIER_PARK:
		if (iconType == AH_HUB_ITEM_ROUTE_OPEN_A) return 1; // N. Sanity Beach
		if (iconType == AH_HUB_ITEM_ROUTE_OPEN_B) return 2; // The Lost Ruins
		if (iconType == AH_HUB_ITEM_ROUTE_KEY3) return 3; // Citadel City
		break;
	case CITADEL_CITY:
		if (iconType == AH_HUB_ITEM_ROUTE_KEY2) return 1; // Glacier Park
		break;
	}
	return 0;
}

static void AH_Map_HubRoutePulsePivot(float centreX, float centreY, int inputAngle, float *pivotX, float *pivotY)
{
	const int directionIndex = ((inputAngle >> 0x8) & 0xc) >> 2;
	*pivotX = centreX - D232.hubArrowInnerOffset[AH_MAP_ARROW_HUB_ROUTE].x - D232.hubArrowOuterOffset[directionIndex].x;
	*pivotY = centreY - D232.hubArrowInnerOffset[AH_MAP_ARROW_HUB_ROUTE].y - D232.hubArrowOuterOffset[directionIndex].y;
}

static void AH_Map_HubRouteTrianglePivot(float centreX, float centreY, int angle, float sizeScale, float *pivotX, float *pivotY)
{
	const float sin = MATH_Sin(angle) / 4096.0f;
	const float cos = MATH_Cos(angle) / 4096.0f;
	// The triangle's local vertex centroid is (0, 8/3), scaled to (0, 4/3).
	const float centroid = (4.0f / 3.0f) * sizeScale;
	*pivotX = centreX - 6.0f - sin * centroid * AH_Map_ShapeAspectX();
	*pivotY = centreY - 4.0f - cos * centroid;
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b0f18-0x800b1150.
void AH_Map_HubArrow(int posX, int posY, const SVec2 *vertPos, char *vertCol, int scale, int angle)
{
	SVec2 basePos[3];
	SVec2 drawPos[3];

	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	int sin = MATH_Sin(angle);
	int cos = MATH_Cos(angle);

	for (int i = 0; i < 3; i++)
	{
		basePos[i].x = posX + 6 +
		               (s16)(((((vertPos[i].x * cos) >> 0xc) + ((vertPos[i].y * sin) >> 0xc)) * ((scale * 8) / 5)

		                          ) >>
		                     0xc);

		basePos[i].y = posY + 4 +
		               (s16)(((((vertPos[i].y * cos) >> 0xc) - ((vertPos[i].x * sin) >> 0xc)) * scale

		                      ) >>
		                     0xc);
	}

	const SVec2 *offset = &D232.hubArrowPrimOffset[0];

	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			drawPos[j].x = basePos[j].x + offset[i].x;

			drawPos[j].y = basePos[j].y + offset[i].y;
		}

		RECTMENU_DrawRwdTriangle(drawPos[0].v, vertCol, P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), &P32_GET(struct DB *, gGT->backBuffer)->primMem);

		vertCol = (char *)&D232.colorTri[0];
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b1150-0x800b14f4.
void AH_Map_HubArrowOuter(struct UIMap *map, int arrowIndex, int posX, int posY, int inputAngle, int type)
{
	struct GameTracker *gGT;
	gGT = P32_GET(struct GameTracker *, sdata->gGT);

	(void)map;

	arrowIndex = (s16)arrowIndex;
	type = (s16)type;

	posX += D232.hubArrowInnerOffset[type].x;
	posY += D232.hubArrowInnerOffset[type].y;

	int timer = FPS_HALF(gGT->timer) >> 0;

	int outlineColorR;
	int outlineColorG;
	int outlineAngleStep;

	outlineColorG = 0x40;
	if ((timer & 1) != 0)
	{
		outlineColorG = 0xe0;
	}

	if (type == 0)
	{
		outlineColorR = outlineColorG;
		outlineAngleStep = AH_MAP_ARROW_OUTLINE_TROPHY_STEP;
	}

	else if (type == 1)
	{
		outlineColorR = 0xff;
		outlineAngleStep = AH_MAP_ARROW_OUTLINE_ROUTE_STEP;

		int directionIndex = ((inputAngle >> 0x8) & 0xc) >> 2;
		posX += D232.hubArrowOuterOffset[directionIndex].x;
		posY += D232.hubArrowOuterOffset[directionIndex].y;
	}

	else
	{
		outlineColorR = outlineColorG;
		outlineAngleStep = AH_MAP_ARROW_OUTLINE_BOSS_STEP;
		inputAngle ^= 0x800;
	}

	inputAngle = (s16)inputAngle;

	for (int outlineIndex = 0; outlineIndex < AH_MAP_ARROW_OUTLINE_COUNT; outlineIndex++)
	{
		u32 outlinePhase = (~(timer + (int)arrowIndex * AH_MAP_ARROW_OUTLINE_PHASE_STEP) & AH_MAP_ARROW_OUTLINE_PHASE_MASK) +
		                   ((AH_MAP_ARROW_OUTLINE_COUNT - 1) - (int)(s16)outlineIndex) * -AH_MAP_ARROW_OUTLINE_PHASE_DELAY;

		if (outlinePhase >= AH_MAP_ARROW_OUTLINE_VISIBLE_PHASES)
		{
			continue;
		}

		int outlineRadius =
		    ((outlinePhase * AH_MAP_ARROW_OUTLINE_RADIUS_MUL + AH_MAP_ARROW_OUTLINE_RADIUS_BIAS) * 0x10000) >> AH_MAP_ARROW_OUTLINE_RADIUS_SHIFT;

		b32 isFirstPoint = true;

		int shiftToggle = 1;

		int prevX = 0;
		int prevY = 0;

		for (int outlineAngle = 0; outlineAngle < outlineAngleStep + 0xfff; outlineAngle += outlineAngleStep)
		{
			if (type != 2)
			{
				shiftToggle = 0;
			}

			int angle = outlineAngle + inputAngle;

			int sin = MATH_Sin(angle);
			int cos = MATH_Cos(angle);

			int radiusShift = (shiftToggle & 1) + 0xc;

			sin = posX + ((((outlineRadius << 3) / 5) * sin) >> radiusShift);
			cos = posY - ((outlineRadius * cos) >> radiusShift);

			if (!isFirstPoint)
			{
				CTR_Box_DrawWirePrims((Point){{prevX, prevY}}, (Point){{sin, cos}}, MakeColor(outlineColorR, outlineColorG, 0xff),
				                      (void *)P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT));
			}

			isFirstPoint = false;
			prevX = sin;
			prevY = cos;
			shiftToggle++;
		}
	}
}

#if defined(CTR_NATIVE)
// AH_Map_HubArrowOuter with a fractional centre and lines that keep their fractions.
static void AH_Map_HubArrowOuterPrecise(int arrowIndex, float posX, float posY, int inputAngle, int type, float sizeScale)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	posX += D232.hubArrowInnerOffset[type].x;
	posY += D232.hubArrowInnerOffset[type].y;

	int timer = FPS_HALF(gGT->timer);
	int outlineColorG = ((timer & 1) != 0) ? 0xe0 : 0x40;
	int outlineColorR = outlineColorG;
	int outlineAngleStep = AH_MAP_ARROW_OUTLINE_TROPHY_STEP;

	if (type == AH_MAP_ARROW_HUB_ROUTE)
	{
		outlineColorR = 0xff;
		outlineAngleStep = AH_MAP_ARROW_OUTLINE_ROUTE_STEP;

		int directionIndex = ((inputAngle >> 0x8) & 0xc) >> 2;
		posX += D232.hubArrowOuterOffset[directionIndex].x;
		posY += D232.hubArrowOuterOffset[directionIndex].y;
	}
	else if (type == AH_MAP_ARROW_BOSS)
	{
		outlineAngleStep = AH_MAP_ARROW_OUTLINE_BOSS_STEP;
		inputAngle ^= 0x800;
	}

	inputAngle = (s16)inputAngle;
	const Color color = MakeColor(outlineColorR, outlineColorG, 0xff);

	for (int outlineIndex = 0; outlineIndex < AH_MAP_ARROW_OUTLINE_COUNT; outlineIndex++)
	{
		u32 outlinePhase = (~(timer + arrowIndex * AH_MAP_ARROW_OUTLINE_PHASE_STEP) & AH_MAP_ARROW_OUTLINE_PHASE_MASK) +
		                   ((AH_MAP_ARROW_OUTLINE_COUNT - 1) - outlineIndex) * -AH_MAP_ARROW_OUTLINE_PHASE_DELAY;

		if (outlinePhase >= AH_MAP_ARROW_OUTLINE_VISIBLE_PHASES)
		{
			continue;
		}

		int outlineRadius =
		    ((outlinePhase * AH_MAP_ARROW_OUTLINE_RADIUS_MUL + AH_MAP_ARROW_OUTLINE_RADIUS_BIAS) * 0x10000) >> AH_MAP_ARROW_OUTLINE_RADIUS_SHIFT;
		const float radiusX = NativeAspect_IsActive() ? outlineRadius * sizeScale * AH_Map_ShapeAspectX() : (float)((outlineRadius << 3) / 5) * sizeScale;
		const float radiusY = outlineRadius * sizeScale;

		float prevX = 0.0f;
		float prevY = 0.0f;
		int pointIndex = 0;

		for (int outlineAngle = 0; outlineAngle < outlineAngleStep + 0xfff; outlineAngle += outlineAngleStep, pointIndex++)
		{
			int angle = outlineAngle + inputAngle;

			// The boss star alternates outer and inner (half radius) points.
			const float radiusScale = ((type == AH_MAP_ARROW_BOSS) && ((pointIndex & 1) == 0)) ? (1.0f / 8192.0f) : (1.0f / 4096.0f);

			float x = posX + radiusX * MATH_Sin(angle) * radiusScale;
			float y = posY - radiusY * MATH_Cos(angle) * radiusScale;

			if (pointIndex != 0)
			{
				AH_Map_PreciseLine(gGT, prevX, prevY, x, y, color);
			}

			prevX = x;
			prevY = y;
		}
	}
}

enum AHMapMarkerConstants
{
	AH_MAP_MARKER_CIRCLE_SEGMENTS = 20,
	AH_MAP_MARKER_STAR_POINTS = 5,
	AH_MAP_MARKER_MAX_RIM = AH_MAP_MARKER_CIRCLE_SEGMENTS,
	AH_MAP_MARKER_PULSE_PERIOD = 64,
};

// Convert vertical marker radii to horizontal HUD units so they stay round.
static float AH_Map_MarkerAspectX(void)
{
#if CTR_NATIVE_WIDESCREEN
	return (float)NativeAspect_GetHudPixelAspectX();
#else
	return (512.0f / SCREEN_HEIGHT) / (4.0f / 3.0f);
#endif
}

// 30 FPS retail frames, fractional at higher frame rates for smooth animation.
static double AH_Map_MarkerFrames(void)
{
	const struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	return CTR_NATIVE_60FPS_ACTIVE ? (double)gGT->timer * FPS / CTR_FRAMES_PER_SECOND : (double)gGT->timer;
}

// Packed 0x00BBGGRR colours.
static u32 AH_Map_MixColor(u32 a, u32 b, float t)
{
	u32 result = 0;
	for (int shift = 0; shift < 24; shift += 8)
	{
		const float from = (float)((a >> shift) & 0xff);
		const float to = (float)((b >> shift) & 0xff);
		result |= (u32)(from + (to - from) * t + 0.5f) << shift;
	}
	return result;
}

static void AH_Map_PreciseTriangleG3(struct GameTracker *gGT, float x0, float y0, float x1, float y1, float x2, float y2, u32 c0, u32 c1, u32 c2)
{
	struct PrimMem *primMem = &P32_GET(struct DB *, gGT->backBuffer)->primMem;
	POLY_G3 *p = P32_GET(void *, primMem->cursor);
	if ((void *)p > P32_GET(void *, primMem->guardEnd))
	{
		return;
	}
	P32_SET(primMem->cursor, p + 1);

	setPolyG3(p);
	setRGB0(p, c0 & 0xff, (c0 >> 8) & 0xff, (c0 >> 16) & 0xff);
	setRGB1(p, c1 & 0xff, (c1 >> 8) & 0xff, (c1 >> 16) & 0xff);
	setRGB2(p, c2 & 0xff, (c2 >> 8) & 0xff, (c2 >> 16) & 0xff);
	AH_Map_PreciseSetXY(&p->x0, x0, y0);
	AH_Map_PreciseSetXY(&p->x1, x1, y1);
	AH_Map_PreciseSetXY(&p->x2, x2, y2);
	AddPrim(P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), p);
}

// Fill a star-shaped outline as a fan from its centre. The rim takes a
// top-to-bottom gradient and the centre is lighter, so markers look domed.
static void AH_Map_MarkerFan(struct GameTracker *gGT, float centreX, float centreY, const float *x, const float *y, int count, u32 centreColor, u32 topColor,
                             u32 bottomColor)
{
	float top = y[0];
	float bottom = y[0];
	for (int i = 1; i < count; i++)
	{
		if (y[i] < top) top = y[i];
		if (y[i] > bottom) bottom = y[i];
	}

	u32 rimColor[AH_MAP_MARKER_MAX_RIM * 2];
	for (int i = 0; i < count; i++)
	{
		rimColor[i] = AH_Map_MixColor(topColor, bottomColor, (bottom > top) ? (y[i] - top) / (bottom - top) : 0.0f);
	}
	for (int i = 0; i < count; i++)
	{
		const int next = (i + 1) % count;
		AH_Map_PreciseTriangleG3(gGT, centreX, centreY, x[i], y[i], x[next], y[next], centreColor, rimColor[i], rimColor[next]);
	}
}

// Rim of a circle, or of a star when innerRadius differs from radius. The
// first point is straight up. Radii are in HUD pixel rows.
static int AH_Map_MarkerRim(float centreX, float centreY, float radius, float innerRadius, b32 star, float *x, float *y)
{
	const int count = star ? AH_MAP_MARKER_STAR_POINTS * 2 : AH_MAP_MARKER_CIRCLE_SEGMENTS;
	const float aspectX = AH_Map_MarkerAspectX();
	for (int i = 0; i < count; i++)
	{
		const float angle = 6.2831853f * i / count;
		const float r = (star && (i & 1)) ? innerRadius : radius;
		x[i] = centreX + sinf(angle) * r * aspectX;
		y[i] = centreY - cosf(angle) * r;
	}
	return count;
}

// Modern Map: a filled circle or star with a one-pixel black
// outline, in place of a retail map sprite. Shared with the race map.
void AH_Map_MarkerShape(float centreX, float centreY, float radius, b32 star, const u32 colors[4])
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	const float innerRadius = star ? radius * 0.43f : radius;
	const u32 topColor = colors[0];
	const u32 bottomColor = colors[2];
	const u32 centreColor = AH_Map_MixColor(AH_Map_MixColor(topColor, bottomColor, 0.5f), 0xffffff, 0.4f);

	float x[AH_MAP_MARKER_MAX_RIM], y[AH_MAP_MARKER_MAX_RIM];
	const int count = AH_Map_MarkerRim(centreX, centreY, radius, innerRadius, star, x, y);
	AH_Map_MarkerFan(gGT, centreX, centreY, x, y, count, centreColor, topColor, bottomColor);

	// OT entries render newest first, so the outline goes in after the fill.
	// A star's points need a longer offset to keep a one-pixel edge.
	AH_Map_MarkerRim(centreX, centreY, radius + (star ? 1.7f : 1.0f), innerRadius + (star ? 0.9f : 1.0f), star, x, y);
	AH_Map_MarkerFan(gGT, centreX, centreY, x, y, count, 0, 0, 0);
}

// Circle or star outline of a given radius and half width, in one colour.
void AH_Map_MarkerOutline(float centreX, float centreY, float radius, float halfWidth, b32 star, u32 color)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	float innerX[AH_MAP_MARKER_MAX_RIM], innerY[AH_MAP_MARKER_MAX_RIM];
	float outerX[AH_MAP_MARKER_MAX_RIM], outerY[AH_MAP_MARKER_MAX_RIM];
	const float innerRatio = star ? 0.45f : 1.0f;
	const int count = AH_Map_MarkerRim(centreX, centreY, radius - halfWidth, (radius - halfWidth) * innerRatio, star, innerX, innerY);
	AH_Map_MarkerRim(centreX, centreY, radius + halfWidth, (radius + halfWidth) * innerRatio, star, outerX, outerY);
	for (int i = 0; i < count; i++)
	{
		const int next = (i + 1) % count;
		AH_Map_PreciseTriangleG3(gGT, innerX[i], innerY[i], outerX[i], outerY[i], innerX[next], innerY[next], color, color, color);
		AH_Map_PreciseTriangleG3(gGT, outerX[i], outerY[i], outerX[next], outerY[next], innerX[next], innerY[next], color, color, color);
	}
}

// Modern Map pulse: three outlines contract onto the marker, on the
// retail timing but animated per frame and fading in from the edge.
static void AH_Map_MarkerPulse(int arrowIndex, float centreX, float centreY, b32 star)
{
	const double frames = AH_Map_MarkerFrames();
	const float cycle = AH_MAP_MARKER_PULSE_PERIOD - 1 - (float)fmod(frames + arrowIndex * AH_MAP_ARROW_OUTLINE_PHASE_STEP, AH_MAP_MARKER_PULSE_PERIOD);

	for (int outlineIndex = 0; outlineIndex < AH_MAP_ARROW_OUTLINE_COUNT; outlineIndex++)
	{
		const float phase = cycle - ((AH_MAP_ARROW_OUTLINE_COUNT - 1) - outlineIndex) * AH_MAP_ARROW_OUTLINE_PHASE_DELAY;
		if ((phase < 0.0f) || (phase >= AH_MAP_ARROW_OUTLINE_VISIBLE_PHASES))
		{
			continue;
		}

		// Retail radius: 4 + phase * 2/3 HUD rows.
		const float t = phase / AH_MAP_ARROW_OUTLINE_VISIBLE_PHASES;
		const float radius = ((star ? 5.5f : 4.0f) + phase * (2.0f / 3.0f)) * 0.85f;
		AH_Map_MarkerOutline(centreX, centreY, radius, 0.6f - 0.3f * t, star, AH_Map_MixColor(0xffe0e0, 0xff4040, t));
	}
}

// The live instance with this behaviour nearest the hub table position, so
// boss and save markers follow the object in the world.
static struct Instance *AH_Map_FindStaticInstance(ThreadFunc tick, int nearX, int nearZ)
{
	struct Instance *nearest = NULL;
	s64 nearestDistance = 0;
	for (struct Thread *t = P32_GET(struct Thread *, P32_GET(struct GameTracker *, sdata->gGT)->threadBuckets[STATIC].thread); t != NULL;
	     t = P32_GET(struct Thread *, t->siblingThread))
	{
		if ((P32_GET(ThreadFunc, t->funcThTick) != tick) || (P32_GET(struct Instance *, t->inst) == NULL))
		{
			continue;
		}
		const s64 dx = P32_GET(struct Instance *, t->inst)->matrix.t[0] - nearX;
		const s64 dz = P32_GET(struct Instance *, t->inst)->matrix.t[2] - nearZ;
		const s64 distance = dx * dx + dz * dz;
		if ((nearest == NULL) || (distance < nearestDistance))
		{
			nearest = P32_GET(struct Instance *, t->inst);
			nearestDistance = distance;
		}
	}
	return nearest;
}

// Pulse ring around a warp pad or boss marker.
static void AH_Map_MarkerRing(struct UIMap *map, int arrowIndex, const s32 worldPos[3], int type)
{
	float posX, posY;
	if (AH_Map_PrecisePos(map, worldPos[0], worldPos[2], &posX, &posY))
	{
		if (gNativeModernMapEnabled)
		{
			AH_Map_MarkerPulse(arrowIndex, posX, posY, type == AH_MAP_ARROW_BOSS);
		}
		else
		{
			AH_Map_HubArrowOuterPrecise(arrowIndex, posX, posY, 0, type, 1.0f);
		}
		return;
	}

	int ringX = worldPos[0];
	int ringY = worldPos[2];
	UI_Map_GetIconPos(map, &ringX, &ringY);
	AH_Map_HubArrowOuter(map, arrowIndex, ringX, ringY, 0, type);
}

// Retail flickers markers between two colours; blend smoothly over the same
// cycle (in 30 FPS frames) instead, starting on colorA.
void AH_Map_MarkerFlash(u32 colors[4], int colorA, int colorB, float periodFrames)
{
	const float t = 0.5f - 0.5f * cosf((float)(AH_Map_MarkerFrames() * 6.2831853 / periodFrames));
	for (int i = 0; i < 4; i++)
	{
		colors[i] = AH_Map_MixColor(P32_GET(u32 *, data.ptrColor[colorA])[i], P32_GET(u32 *, data.ptrColor[colorB])[i], t);
	}
}

// Open warp pads and boss doors flick between blue and white every two frames.
static void AH_Map_MarkerFlashColors(u32 colors[4])
{
	AH_Map_MarkerFlash(colors, AH_MAP_COLOR_FLASH_PRIMARY, AH_MAP_COLOR_FLASH_SECONDARY, 4.0f);
}

// Slide Coliseum/Turbo Track pads step through eight colours, two frames each;
// blend between them.
static void AH_Map_MarkerCycleColors(u32 colors[4])
{
	const double step = AH_Map_MarkerFrames() / 2.0;
	const int index = (int)fmod(floor(step), 8.0);
	const float t = (float)(step - floor(step));
	for (int i = 0; i < 4; i++)
	{
		colors[i] = AH_Map_MixColor(P32_GET(u32 *, data.ptrColor[AH_MAP_COLOR_FLASH_PRIMARY + index])[i],
		                            P32_GET(u32 *, data.ptrColor[AH_MAP_COLOR_FLASH_PRIMARY + ((index + 1) & 7)])[i], t);
	}
}

// Warp pad dot or boss star.
static void AH_Map_MarkerIcon(struct UIMap *map, const s32 worldPos[3], int iconID, const u32 colors[4], int colorID)
{
	if (gNativeModernMapEnabled)
	{
		float posX, posY;
		UI_Map_GetIconPosPrecise(map, worldPos, &posX, &posY);
		const b32 star = iconID == AH_MAP_ICON_BOSS_STAR;
		AH_Map_MarkerShape(posX, posY, (star ? 3.7f : 2.1f) * 0.85f, star, colors);
		return;
	}
	UI_Map_DrawRawIcon(map, worldPos, iconID, colorID, 0, 0x1000);
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b14f4-0x800b1a18.
void AH_Map_HubItems(struct UIMap *map, s16 *arrowCounter)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	struct AdvProgress *adv = &sdata->advProgress;
	s16 levelID = gGT->levelID;
	struct HubItem *item = P32_GET(struct HubItem *, D232.hubItemsXY_ptrArray[levelID - GEM_STONE_VALLEY]);
	Vec3 pos3D;

	if (item->posX != AH_HUB_ITEM_LIST_END_POS_X)
	{
		do
		{
			AdventureHubItemType iconType = item->iconType;
			s16 routeLockState = -1;
			s16 bossState = AH_MAP_BOSS_ITEM_NONE;

			b32 open = true;

			// One-key route arrow, only locked in N. Sanity Beach.
			if (iconType == AH_HUB_ITEM_ROUTE_KEY1_IF_BEACH)
			{
				routeLockState = 0;

				if (levelID == N_SANITY_BEACH)
				{
					// locked if key < 1
					routeLockState = (gGT->currAdvProfile.numKeys < 1);
				}
			}
			else if (AH_HUB_ITEM_ROUTE_KEY1_IF_BEACH < iconType)
			{
				// gemstone valley
				if (iconType == AH_HUB_ITEM_OXIDE_WARPPAD)
				{
					// check all boss keys
					for (int i = 0; i < AH_BOSS_KEY_COUNT; i++)
					{
						u32 bit = i + ADV_REWARD_FIRST_BOSS_KEY;

						if (!CHECK_ADV_BIT(adv->rewards, bit))
						{
							open = false;
							break;
						}
					}

					if (open)
					{
						bossState = ((adv->storyFlags & ADV_REWARD_BEAT_OXIDE_FIRST_BOSS_MASK) != 0) ? AH_MAP_BOSS_ITEM_COMPLETE : AH_MAP_BOSS_ITEM_OPEN;
					}
					else
					{
						bossState = AH_MAP_BOSS_ITEM_LOCKED;
					}
				}
				else if (AH_HUB_ITEM_PINSTRIPE_GARAGE < iconType)
				{
					// save/load screen synthetic hub marker
					if (iconType == AH_HUB_ITEM_SAVE_LOAD_MARKER)
					{
						int saveLoadPosX = (int)item->posX - 0x200;
						int saveLoadPosY = (int)item->posY - 0x100;

#if defined(CTR_NATIVE)
						float preciseX, preciseY;
						struct Instance *saveInst = gNativeModernMapEnabled ? AH_Map_FindStaticInstance(AH_SaveObj_ThTick, item->posX, item->posY) : NULL;
						if (saveInst != NULL)
						{
							// Centred on the save station; the marker pivots 6,4 past its position.
							UI_Map_GetIconPosPrecise(map, &saveInst->matrix.t[0], &preciseX, &preciseY);
							AH_Map_LoadSavePrecise(preciseX - 6, preciseY - 4, &D232.loadSavePos[0], (char *)&D232.loadSave_col[0], 0x800, (int)item->angle);
						}
						else if (AH_Map_PrecisePos(map, saveLoadPosX, saveLoadPosY, &preciseX, &preciseY))
						{
							AH_Map_LoadSavePrecise(preciseX, preciseY, &D232.loadSavePos[0], (char *)&D232.loadSave_col[0], 0x800, (int)item->angle);
						}
						else
#endif
						{
							UI_Map_GetIconPos(map, &saveLoadPosX, &saveLoadPosY);

							AH_Map_LoadSave_Full(saveLoadPosX, saveLoadPosY, &D232.loadSavePos[0], (char *)&D232.loadSave_col[0], 0x800, (int)item->angle);
						}
					}
				}
				else
				{
					int base = levelID - N_SANITY_BEACH;
					s16 *trophies = &data.advHubTrackIDs[base * AH_HUB_TRACK_COUNT];

					for (int i = 0; i < AH_HUB_TRACK_COUNT; i++)
					{
						if (!CHECK_ADV_BIT(adv->rewards, trophies[i] + ADV_REWARD_FIRST_TROPHY))
						{
							open = false;
							break;
						}
					}

					if (open)
					{
						bossState = CHECK_ADV_BIT(adv->rewards, base + ADV_REWARD_FIRST_BOSS_KEY) ? AH_MAP_BOSS_ITEM_COMPLETE : AH_MAP_BOSS_ITEM_OPEN;
					}
					else
					{
						bossState = AH_MAP_BOSS_ITEM_LOCKED;
					}
				}
			}
			// Two-key route arrow.
			else if (iconType == AH_HUB_ITEM_ROUTE_KEY2)
			{
				// locked if keys < 2
				routeLockState = (gGT->currAdvProfile.numKeys < 2);
			}
			else if (iconType < AH_HUB_ITEM_ROUTE_OPEN_B)
			{
				// Three-key route arrow.
				if (iconType == AH_HUB_ITEM_ROUTE_KEY3)
				{
					// locked if keys < 3
					routeLockState = (gGT->currAdvProfile.numKeys < 3);
				}
			}
			// Open route arrows.
			else if ((iconType == AH_HUB_ITEM_ROUTE_OPEN_B) || (iconType == AH_HUB_ITEM_ROUTE_OPEN_A))
			{
				// never locked
				routeLockState = 0;
			}

			if (routeLockState >= 0)
			{
				int routePosX = (int)item->posX - 0x200;
				int routePosY = (int)item->posY - 0x100;

#if defined(CTR_NATIVE)
				float preciseX, preciseY;
				b32 precise = false;
				b32 modernRoute = false;
				if (gNativeModernMapEnabled)
				{
					const int triggerID = AH_Map_HubRouteTriggerID(levelID, iconType);
					s32 routeWorldPos[3];
					if ((triggerID != 0) && NativeMinimap_GetHubRoutePosition(levelID, triggerID, routeWorldPos))
					{
						UI_Map_GetIconPosPrecise(map, routeWorldPos, &preciseX, &preciseY);
						precise = true;
						modernRoute = true;
					}
				}
				if (!modernRoute)
					precise = AH_Map_PrecisePos(map, routePosX, routePosY, &preciseX, &preciseY);
#endif
				UI_Map_GetIconPos(map, &routePosX, &routePosY);
				if ((routeLockState == 0) && (D232.mapPriorityArrowDrawn == 0))
				{
#if defined(CTR_NATIVE)
					if (precise)
					{
						const int pulseAngle = (0x1000 - (u16)item->angle);
						const float pulseScale = gNativeModernMapEnabled ? 0.75f : 1.0f;
						if (modernRoute)
						{
							float pulseX, pulseY;
							AH_Map_HubRoutePulsePivot(preciseX, preciseY, pulseAngle, &pulseX, &pulseY);
							AH_Map_HubArrowOuterPrecise((int)*arrowCounter, pulseX, pulseY, pulseAngle, AH_MAP_ARROW_HUB_ROUTE, pulseScale);
						}
						else
							AH_Map_HubArrowOuterPrecise((int)*arrowCounter, preciseX, preciseY, pulseAngle, AH_MAP_ARROW_HUB_ROUTE, pulseScale);
					}
					else
#endif
					{
						AH_Map_HubArrowOuter(map, (int)*arrowCounter, routePosX, routePosY, (0x1000 - (u16)item->angle), AH_MAP_ARROW_HUB_ROUTE);
					}
					*arrowCounter = *arrowCounter + 1;
				}

				int colorOffset;

				// if even frame
				if ((FPS_HALF(gGT->timer) & 2) == 0)
				{
					colorOffset = (int)routeLockState * 6;
				}
				else
				{
					colorOffset = ((int)routeLockState * 2 + 1) * 3;
				}

#if defined(CTR_NATIVE)
				if (precise)
				{
					float triangleX = preciseX, triangleY = preciseY;
					const float routeSize = gNativeModernMapEnabled ? 0.75f : 1.0f;
					if (modernRoute)
						AH_Map_HubRouteTrianglePivot(preciseX, preciseY, (int)item->angle, routeSize, &triangleX, &triangleY);
					AH_Map_HubArrowPrecise(triangleX, triangleY, &D232.hubArrowPos[0], (char *)&D232.hubArrow_col1[colorOffset], 0x800, (int)item->angle, routeSize);
				}
				else
#endif
				{
					AH_Map_HubArrow(routePosX, routePosY, &D232.hubArrowPos[0], (char *)&D232.hubArrow_col1[colorOffset], 0x800, (int)item->angle);
				}
			}

			if (bossState >= AH_MAP_BOSS_ITEM_LOCKED)
			{
				pos3D.x = (int)item->posX;
				pos3D.y = 0;
				pos3D.z = (int)item->posY;

#if defined(CTR_NATIVE)
				// Modern Map use the garage door in the world.
				struct Instance *garageInst = gNativeModernMapEnabled ? AH_Map_FindStaticInstance(AH_Garage_ThTick, pos3D.x, pos3D.z) : NULL;
				if (garageInst != NULL)
				{
					pos3D.x = garageInst->matrix.t[0];
					pos3D.z = garageInst->matrix.t[2];
				}
#endif

				// if beat boss race
				int bossIconColor;

				if (bossState == AH_MAP_BOSS_ITEM_COMPLETE)
				{
					// red
					bossIconColor = AH_MAP_COLOR_COMPLETE;
				}
				else
				{
					// locked boss race
					// bossState == AH_MAP_BOSS_ITEM_LOCKED

					// grey
					bossIconColor = AH_MAP_COLOR_LOCKED;

					// open, not beaten
					if (bossState == AH_MAP_BOSS_ITEM_OPEN)
					{
						// blue and white
						// depending on frames
						bossIconColor = AH_MAP_COLOR_FLASH_PRIMARY;
						if ((FPS_HALF(gGT->timer) & 2) != 0)
						{
							bossIconColor = AH_MAP_COLOR_FLASH_SECONDARY;
						}
					}
				}

				// open, not beaten
				if (bossState == AH_MAP_BOSS_ITEM_OPEN)
				{
					D232.mapPriorityArrowDrawn = bossState;

#if defined(CTR_NATIVE)
					AH_Map_MarkerRing(map, (int)*arrowCounter, pos3D.v, AH_MAP_ARROW_BOSS);
#else
					int bossArrowPosX = pos3D.x;
					int bossArrowPosY = pos3D.z;

					UI_Map_GetIconPos(map, &bossArrowPosX, &bossArrowPosY);

					AH_Map_HubArrowOuter(map, (int)*arrowCounter, bossArrowPosX, bossArrowPosY, 0, AH_MAP_ARROW_BOSS);
#endif

					*arrowCounter = *arrowCounter + 1;
				}

				// draw star icon for boss
#if defined(CTR_NATIVE)
				u32 bossColors[4];
				memcpy(bossColors, P32_GET(u32 *, data.ptrColor[bossIconColor]), sizeof(bossColors));
				if (gNativeModernMapEnabled && (bossState == AH_MAP_BOSS_ITEM_OPEN))
				{
					AH_Map_MarkerFlashColors(bossColors);
				}
				AH_Map_MarkerIcon(map, pos3D.v, AH_MAP_ICON_BOSS_STAR, bossColors, bossIconColor);
#else
				UI_Map_DrawRawIcon(map, pos3D.v, AH_MAP_ICON_BOSS_STAR, bossIconColor, 0, 0x1000);
#endif
			}
			item++;
		} while (item->posX != AH_HUB_ITEM_LIST_END_POS_X);
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b1a18-0x800b1c90.
void AH_Map_Warppads(struct UIMap *map, struct Thread *warppadThread, s16 *arrowCounter)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	// find minDistance, set to max
	int minDistance = 0x7fffffff;
	struct Instance *closestWarppadInst = NULL;

	MATRIX *driverMatrix = &P32_GET(struct Instance *, P32_GET(struct Driver *, gGT->drivers[0])->instSelf)->matrix;

	for (
	    /**/; warppadThread != NULL; warppadThread = P32_GET(struct Thread *, warppadThread->siblingThread))
	{
		int visualState = warppadThread->modelIndex;
		b32 drawsTrophyArrow = false;
		b32 includeInSoundDistance = true;

		struct Instance *warppadInst = P32_GET(struct Instance *, warppadThread->inst);
		int color;

		switch ((u32)visualState)
		{
		case AH_WP_VISUAL_LOCKED:
			color = AH_MAP_COLOR_LOCKED;
			includeInSoundDistance = false;
			break;
		case AH_WP_VISUAL_TROPHY_OPEN:
			color = AH_MAP_COLOR_FLASH_PRIMARY;
			if ((FPS_HALF(gGT->timer) & 2) != 0)
			{
				color = AH_MAP_COLOR_FLASH_SECONDARY;
			}
			drawsTrophyArrow = true;
			break;
		case AH_WP_VISUAL_COMPLETE:
			color = AH_MAP_COLOR_COMPLETE;
			break;
		case AH_WP_VISUAL_RELIC_TOKEN_OPEN:
			color = AH_MAP_COLOR_RELIC_TOKEN;
			break;
		case AH_WP_VISUAL_COLOR_CYCLE_OPEN:
			// Each Slide Coliseum/Turbo Track color lasts two frames.
			color = ((FPS_HALF(gGT->timer) >> 1) & 7) + AH_MAP_COLOR_FLASH_PRIMARY;
			break;
		default:
			color = AH_MAP_COLOR_INVALID;
			includeInSoundDistance = false;
			break;
		}

		if (drawsTrophyArrow)
		{
			D232.mapPriorityArrowDrawn = 1;

#if defined(CTR_NATIVE)
			AH_Map_MarkerRing(map, (int)*arrowCounter, &warppadInst->matrix.t[0], AH_MAP_ARROW_WARPPAD_TROPHY);
#else
			// get posZ in 3D, turns into posY in 2D
			int arrowPosX = warppadInst->matrix.t[0];
			int arrowPosY = warppadInst->matrix.t[2];

			// Get Icon Dimensions
			UI_Map_GetIconPos(map, &arrowPosX, &arrowPosY);

			AH_Map_HubArrowOuter(map, (int)*arrowCounter, arrowPosX, arrowPosY, 0, AH_MAP_ARROW_WARPPAD_TROPHY);
#endif

			*arrowCounter = *arrowCounter + 1;
		}

#if defined(CTR_NATIVE)
		u32 markerColors[4];
		memcpy(markerColors, P32_GET(u32 *, data.ptrColor[color]), sizeof(markerColors));
		if (gNativeModernMapEnabled)
		{
			if (visualState == AH_WP_VISUAL_TROPHY_OPEN)
			{
				AH_Map_MarkerFlashColors(markerColors);
			}
			else if (visualState == AH_WP_VISUAL_COLOR_CYCLE_OPEN)
			{
				AH_Map_MarkerCycleColors(markerColors);
			}
		}
		AH_Map_MarkerIcon(map, &warppadInst->matrix.t[0], AH_MAP_ICON_WARPPAD, markerColors, color);
#else
		UI_Map_DrawRawIcon(map, &warppadInst->matrix.t[0], AH_MAP_ICON_WARPPAD, color, 0, 0x1000);
#endif

		if (!includeInSoundDistance)
		{
			// skip distance check
			continue;
		}

		int distX = warppadInst->matrix.t[0] - driverMatrix->t[0];
		int distY = warppadInst->matrix.t[1] - driverMatrix->t[1];
		int distZ = warppadInst->matrix.t[2] - driverMatrix->t[2];

		int currDistance = SquareRoot0_stub(distX * distX + distY * distY + distZ * distZ);

		if (minDistance > currDistance)
		{
			minDistance = currDistance;
			closestWarppadInst = warppadInst;
		}
	}

	// play sound from closest unlocked warppad
	if (closestWarppadInst != NULL)
	{
		PlayWarppadSound(minDistance << 1);
	}

	return;
}

#if defined(CTR_NATIVE)
force_inline void AH_MaskHint_DrawRepeatPrompt(void);
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b1c90-0x800b1ef8.
void AH_Map_Main(void)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	sdata->HudAndDebugFlags &= ~AH_MAP_HUD_AND_DEBUG_SPEEDOMETER;

	s16 driverIconCounter = 0;
	s16 arrowCounter = 0;
	struct Driver *advDriver = P32_GET(struct Driver *, gGT->drivers[0]);
	struct UiElement2D *hud = P32_GET(struct UiElement2D *, data.hudStructPtr[gGT->numPlyrCurrGame - 1]);
	struct UIMap *map = NULL;

	int raceFlagState = RaceFlag_GetCanDraw();
	if (raceFlagState == 0)
	{
		RaceFlag_SetCanDraw(1);
	}

	if (
	    // if Aku Hint is not unlocked
	    !CHECK_ADV_BIT(sdata->advProgress.rewards, ADV_REWARD_HINT_WELCOME_TO_ARENA) &&

	    RaceFlag_IsFullyOffScreen())
	{
		// Trigger Aku Hint:
		// Welcome to Adventure Arena
		MainFrame_RequestMaskHint(ADV_MASK_HINT_ID_WELCOME_TO_ARENA, 0);
	}


	// NOTE(aalhendi): Retail keeps this AI-only Adventure Hub speedometer fallback.
	if ((gGT->numPlyrCurrGame == 0) && ((advDriver->actionsFlagSet & ACTION_BOT) != 0))
	{
		sdata->HudAndDebugFlags = AH_MAP_HUD_AND_DEBUG_SPEEDOMETER;
	}

	if (P32_GET(struct SpawnType1 *, P32_GET(struct Level *, gGT->level1)->ptrSpawnType1)->count != 0)
	{
		P32(void *) *pointers = ST1_GETPOINTERS(P32_GET(struct SpawnType1 *, P32_GET(struct Level *, gGT->level1)->ptrSpawnType1));
		map = P32_GET(void *, pointers[ST1_MAP]);
	}

	// if game is not paused
	if ((gGT->gameMode1 & PAUSE_ALL) == 0)
	{
		// Jump meter and landing boost
		UI_JumpMeter_Update(advDriver);
	}

	if ((gGT->hudFlags & HUD_FLAG_HIDE_ADVENTURE_MAP) == 0)
	{
		arrowCounter = 0;

		D232.mapPriorityArrowDrawn = 0;

		UI_Map_DrawDrivers(map, P32_GET(struct Thread *, gGT->threadBuckets[PLAYER].thread), &driverIconCounter);

		AH_Map_Warppads(map, P32_GET(struct Thread *, gGT->threadBuckets[WARPPAD].thread), &arrowCounter);

		AH_Map_HubItems(map, &arrowCounter);

		UI_Map_DrawMap(P32_GET(struct Icon *, gGT->ptrIcons[AH_MAP_ICON_TOP_HALF]), P32_GET(struct Icon *, gGT->ptrIcons[AH_MAP_ICON_BOTTOM_HALF]),

		               AH_MAP_SCREEN_POS_X, AH_MAP_SCREEN_POS_Y,

		               &P32_GET(struct DB *, gGT->backBuffer)->primMem, P32_GET(uint32_t *, gGT->pushBuffer_UI.ptrOT), 1);

		UI_DrawSlideMeter(hud[AH_MAP_HUD_SLOT_SLIDE_METER].x, hud[AH_MAP_HUD_SLOT_SLIDE_METER].y, advDriver);
	}

	UI_DrawNumRelic(hud[AH_MAP_HUD_SLOT_RELIC_COUNT].x + AH_MAP_HUD_COUNTER_OFFSET_X, hud[AH_MAP_HUD_SLOT_RELIC_COUNT].y + AH_MAP_HUD_COUNTER_OFFSET_Y);
	UI_DrawNumKey(hud[AH_MAP_HUD_SLOT_KEY_COUNT].x + AH_MAP_HUD_COUNTER_OFFSET_X, hud[AH_MAP_HUD_SLOT_KEY_COUNT].y + AH_MAP_HUD_COUNTER_OFFSET_Y);
	UI_DrawNumTrophy(hud[AH_MAP_HUD_SLOT_TROPHY_COUNT].x + AH_MAP_HUD_COUNTER_OFFSET_X, hud[AH_MAP_HUD_SLOT_TROPHY_COUNT].y + AH_MAP_HUD_COUNTER_OFFSET_Y);

#if defined(CTR_NATIVE)
	// NOTE(aalhendi): Retail appends this prompt after DrawOTag starts; the PS1
	// GPU can still consume that late OT write. Native DrawOTag parses
	// synchronously, so emit only this static prompt during the hub UI pass and
	// leave AH_MaskHint_Update to run the real state/audio timing later.
	if (sdata->AkuAkuHintState == 5)
	{
		AH_MaskHint_DrawRepeatPrompt();
	}
#endif
}
