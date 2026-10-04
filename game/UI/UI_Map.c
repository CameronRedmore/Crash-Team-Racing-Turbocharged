#include <common.h>

#if defined(CTR_NATIVE)
#include "platform/native_adhoc.h"
#include "platform/native_pgxp.h"
#include "platform/native_minimap.h"
#include <math.h>
#include <string.h>
#endif

enum UIMapConstants
{
	UI_MAP_NEUTRAL_COLOR = 0x808080,
	UI_MAP_COLOR_MODE_BLACK = 2,
	UI_MAP_COLOR_MODE_BLUE = 3,
	UI_MAP_BLUE_OUTLINE_COLOR = 0x402000,
	UI_MAP_TPAGE_BLEND_MASK = 0xff9f,
	UI_MAP_TPAGE_BLEND_SHIFT = 5,
	UI_MAP_SEMI_TRANS_CODE_BIT = 2,
	UI_MAP_MODE_0_DEGREES = 0,
	UI_MAP_MODE_90_DEGREES = 1,
	UI_MAP_MODE_180_DEGREES = 2,
	UI_MAP_ICON_Y_OFFSET = 0x10,
	UI_MAP_3P_OFFSET_X = 60,
	UI_MAP_3P_OFFSET_Y = 10,
	UI_MAP_PLAYER_ICON_AI = 0x31,
	UI_MAP_PLAYER_ICON_HUMAN = 0x32,
	UI_MAP_WARPBALL_ICON = 0x20,
	UI_MAP_WARPBALL_TARGET_ICON = 0x21,
	UI_MAP_ICON_GROUP = 5,
	UI_MAP_ARROW_ROT_FLIP = 0x800,
	UI_MAP_ARROW_ROT_FLAG = 0x1000,
	UI_MAP_ICON_SCALE = 0x1000,
	UI_MAP_ADV_ARROW_SCALE = 0x800,
	UI_MAP_ADV_ARROW_PIVOT_X = 6,
	UI_MAP_ADV_ARROW_PIVOT_Y = 4,
};

CTR_STATIC_ASSERT(UI_MAP_NEUTRAL_COLOR == 0x808080);
CTR_STATIC_ASSERT(UI_MAP_COLOR_MODE_BLACK == 2);
CTR_STATIC_ASSERT(UI_MAP_COLOR_MODE_BLUE == 3);
CTR_STATIC_ASSERT(UI_MAP_BLUE_OUTLINE_COLOR == 0x402000);
CTR_STATIC_ASSERT(UI_MAP_TPAGE_BLEND_MASK == 0xff9f);
CTR_STATIC_ASSERT(UI_MAP_TPAGE_BLEND_SHIFT == 5);
CTR_STATIC_ASSERT(UI_MAP_SEMI_TRANS_CODE_BIT == 2);
CTR_STATIC_ASSERT(UI_MAP_MODE_0_DEGREES == 0);
CTR_STATIC_ASSERT(UI_MAP_MODE_90_DEGREES == 1);
CTR_STATIC_ASSERT(UI_MAP_MODE_180_DEGREES == 2);
CTR_STATIC_ASSERT(UI_MAP_ICON_Y_OFFSET == 0x10);
CTR_STATIC_ASSERT(UI_MAP_3P_OFFSET_X == 60);
CTR_STATIC_ASSERT(UI_MAP_3P_OFFSET_Y == 10);
CTR_STATIC_ASSERT(UI_MAP_PLAYER_ICON_AI == 0x31);
CTR_STATIC_ASSERT(UI_MAP_PLAYER_ICON_HUMAN == 0x32);
CTR_STATIC_ASSERT(UI_MAP_WARPBALL_ICON == 0x20);
CTR_STATIC_ASSERT(UI_MAP_WARPBALL_TARGET_ICON == 0x21);
CTR_STATIC_ASSERT(UI_MAP_ICON_GROUP == 5);
CTR_STATIC_ASSERT(UI_MAP_ARROW_ROT_FLIP == 0x800);
CTR_STATIC_ASSERT(UI_MAP_ARROW_ROT_FLAG == 0x1000);
CTR_STATIC_ASSERT(UI_MAP_ICON_SCALE == 0x1000);
CTR_STATIC_ASSERT(UI_MAP_ADV_ARROW_SCALE == 0x800);

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004d614-0x8004d8b4.
void UI_Map_DrawMap(struct Icon *mapTop, struct Icon *mapBottom, s16 posX, s16 posY, struct PrimMem *primMem, uint32_t *otMem, u32 colorID)
{
	s16 mapBottomHeight;
	s16 mapTopHeight;
	struct UIMapSpawnMetadata *mapMetadata;
	POLY_FT4 *p;
	b32 drawTopHalf;
	u32 color;
	u32 transparency;
	struct GameTracker *gGT;

#if defined(CTR_NATIVE)
	if (NativeMinimap_DrawLive(primMem, otMem, colorID)) return;
#endif
	gGT = sdata->gGT;

	mapMetadata = NULL;

	// draw minimap with neutral/none vertex color, minimap's regular color is white
	color = UI_MAP_NEUTRAL_COLOR;
	transparency = colorID;

	// draw map black
	// used for the minimap shadow in the track select screen
	if (colorID == UI_MAP_COLOR_MODE_BLACK)
	{
		color = 0;
		transparency = 0;
	}

	// draw minimap blue
	// used for the minimap outline in the track select screen
	if (colorID == UI_MAP_COLOR_MODE_BLUE)
	{
		color = UI_MAP_BLUE_OUTLINE_COLOR;
		transparency = 0;
	}

	if (gGT->level1->ptrSpawnType1 != 0)
	{
		void **pointers = ST1_GETPOINTERS(gGT->level1->ptrSpawnType1);
		mapMetadata = pointers[ST1_MAP];
	}

	// position of the bottom margin of the primitive for the bottom half of the minimap
	mapBottomHeight = mapBottom->texLayout.v2 - mapBottom->texLayout.v0;

	p = (POLY_FT4 *)primMem->cursor;

	// if these conditions are met, then draw the top half of the minimap; otherwise, only draw the bottom half
	// not sure when the game ever draws only the bottom half
	drawTopHalf = ((mapMetadata != NULL) && (mapMetadata->topHalfMode == 0)) ||
	              ((gGT->gameMode1 & MAIN_MENU) != 0);
	if (drawTopHalf)
	{
		// r0, g0, b0 (vertex color)
		CtrGpu_WriteColorCode(&p->r0, color);

		// position of the top margin of the primitive for the top half of the minimap
		mapTopHeight = posY - (((u16)mapTop->texLayout.v2 - (u16)mapTop->texLayout.v0) + mapBottomHeight);

		p->y0 = mapTopHeight;
		p->y1 = mapTopHeight;
		p->y2 = posY - mapBottomHeight;
		p->y3 = posY - mapBottomHeight;

		UI_Map_DrawMap_ExtraFunc(mapTop, p, posX, 0, primMem, otMem, transparency);

		p = p + 1;
	}

	// r0, g0, b0 (vertex color)
	CtrGpu_WriteColorCode(&p->r0, color);

	p->y0 = posY - mapBottomHeight;
	p->y1 = posY - mapBottomHeight;
	p->y2 = posY;
	p->y3 = posY;

	if (drawTopHalf)
	{
		p->y0--;
		p->y1--;
	}
	UI_Map_DrawMap_ExtraFunc(mapBottom, p, posX, 0, primMem, otMem, transparency);

	primMem->cursor = p + 1;
}

void UI_Map_DrawMap_ExtraFunc(struct Icon *icon, POLY_FT4 *p, s16 posX, s16 empty, struct PrimMem *primMem, uint32_t *otMem, u32 transparency)
{
	(void)empty;
	(void)primMem;
	s16 leftX;
	s16 sizeX;

	sizeX = icon->texLayout.u1 - icon->texLayout.u0;

	// posX is the right side,
	// letftX is the left side
	leftX = posX - sizeX;

#if CTR_NATIVE_WIDESCREEN
	int centerX = (leftX + posX) / 2;
	float mapOffsetX = 0.0f;

	// In-race map coordinates are projected around iconStartX.  Scale the
	// map around that same origin so its route icons remain registered.
	if (((sdata->gGT->gameMode1 & MAIN_MENU) == 0) && (sdata->gGT->level1 != NULL) && (sdata->gGT->level1->ptrSpawnType1 != NULL))
	{
		void **pointers = ST1_GETPOINTERS(sdata->gGT->level1->ptrSpawnType1);
		struct UIMap *map = pointers[ST1_MAP];
		if (map != NULL)
		{
			mapOffsetX = NativeMinimap_GetAnchorOffsetX(map);
			int centerY = 0;
			centerX = 0;
			UI_Map_GetIconPos(map, &centerX, &centerY);
			centerX -= (int)mapOffsetX;
		}
	}

	leftX = (s16)(centerX + CTR_WIDESCREEN_SCALE_X(leftX - centerX) + mapOffsetX);
	posX = (s16)(centerX + CTR_WIDESCREEN_SCALE_X(posX - centerX) + mapOffsetX);
#endif

	p->x0 = leftX;
	p->x1 = posX;
	p->x2 = leftX;
	p->x3 = posX;

	// set header
	setPolyFT4(p);

	// UVs
	CtrGpu_WritePackedUVWord(&p->u0, CTR_ReadU32LE(&icon->texLayout.u0));
	CtrGpu_WritePackedUVWord(&p->u1, CTR_ReadU32LE(&icon->texLayout.u1));
	CtrGpu_WritePackedUVWord(&p->u2, CTR_ReadU32LE(&icon->texLayout.u2));
	CtrGpu_WritePackedUV(&p->u3, CTR_ReadU16LE(&icon->texLayout.u3));

	if (transparency != 0)
	{
		p->tpage = (p->tpage & UI_MAP_TPAGE_BLEND_MASK) | ((u16)transparency << UI_MAP_TPAGE_BLEND_SHIFT);
	}

	p->code |= UI_MAP_SEMI_TRANS_CODE_BIT;

	AddPrim(otMem, p);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004d8b4-0x8004dbac.
void UI_Map_GetIconPos(struct UIMap *map, int *posX, int *posY)
{
	s16 mode;
	int addX;
	int addY;
	int worldRangeX;
	int worldRangeY;

#if 0
  // trap() functions were removed from original,
  // we assume dividing by zero will never happen
#endif

	// rendering mode (forward, sideways, etc)
	mode = map->mode;

	worldRangeX = map->worldEndX - map->worldStartX;
	worldRangeY = map->worldEndY - map->worldStartY;

	if (mode == UI_MAP_MODE_0_DEGREES)
	{
		// 0 degrees
		addX = (*posX * map->iconSizeX) / worldRangeX;
		addY = (*posY * map->iconSizeY * 2) / worldRangeY;
	}

	else if (mode == UI_MAP_MODE_90_DEGREES)
	{
		// 90 degrees
		addX = -(*posY * map->iconSizeX) / worldRangeY;
		addY = (*posX * map->iconSizeY * 2) / worldRangeX;
	}

	else if (mode == UI_MAP_MODE_180_DEGREES)
	{
		// 180 degrees
		addX = -(*posX * map->iconSizeX) / worldRangeX;
		addY = -(*posY * map->iconSizeY * 2) / worldRangeY;
	}

	else
	{
		// 270 degrees
		addX = (*posY * map->iconSizeX) / worldRangeY;
		addY = -(*posX * map->iconSizeY * 2) / worldRangeX;
	}

#if CTR_NATIVE_WIDESCREEN
	addX = CTR_WIDESCREEN_SCALE_X(addX);
#endif

	if (sdata->gGT->numPlyrCurrGame == 3)
	{
		addX -= UI_MAP_3P_OFFSET_X;
		addY += UI_MAP_3P_OFFSET_Y;
	}

	*posX = map->iconStartX + addX;
#if defined(CTR_NATIVE)
	*posX += (int)NativeMinimap_GetAnchorOffsetX(map);
#endif
	*posY = map->iconStartY + addY - UI_MAP_ICON_Y_OFFSET;
	return;
}

#if defined(CTR_NATIVE)
// Keep the division and widescreen conversion fractional until rasterization.
void UI_Map_GetIconPosPrecise(const struct UIMap *map, const s32 worldPos[3], float *posX, float *posY)
{
	double addX, addY;
	NativeMinimap_Project(map, worldPos[0], worldPos[2], &addX, &addY);
#if CTR_NATIVE_WIDESCREEN
	addX *= NativeAspect_GetScaleX();
#endif
	if (sdata->gGT->numPlyrCurrGame == 3)
	{
		addX -= UI_MAP_3P_OFFSET_X;
		addY += UI_MAP_3P_OFFSET_Y;
	}
	*posX = (float)(map->iconStartX + addX) + NativeMinimap_GetAnchorOffsetX(map);
	*posY = (float)(map->iconStartY + addY - UI_MAP_ICON_Y_OFFSET);
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004dbac-0x8004dc44.
// Draw dot for Player on 2D Adv Map
void UI_Map_DrawAdvPlayer(struct UIMap *map, const s32 worldPos[3], int unused1, int unused2, s16 rot, s16 scale)
{
	(void)unused1;
	(void)unused2;
	int *arrowColor;
	int posX;
	int posY;

	posX = worldPos[0];
	posY = worldPos[2];

	UI_Map_GetIconPos(map, &posX, &posY);

	arrowColor = &data.playerIconAdvMap.vertCol1[0];
	if ((FPS_HALF(sdata->gGT->timer) & 2) != 0)
	{
		arrowColor = &data.playerIconAdvMap.vertCol2[0];
	}

#if defined(CTR_NATIVE)
	if (NativeAspect_IsActive() || (gNativePreciseMinimapEnabled && NATIVE_PGXP_SUPPORTED) || gNativeModernMapEnabled)
	{
		float preciseX, preciseY;
		UI_Map_GetIconPosPrecise(map, worldPos, &preciseX, &preciseY);
		if (gNativeModernMapEnabled)
		{
			// The arrow pivots 6,4 past the position it is given.
			preciseX -= UI_MAP_ADV_ARROW_PIVOT_X;
			preciseY -= UI_MAP_ADV_ARROW_PIVOT_Y;
		}
		AH_Map_HubArrowPrecise(preciseX, preciseY, &data.playerIconAdvMap.pos[0], (char *)arrowColor, scale, rot, 0.6f);
		return;
	}
#endif
	AH_Map_HubArrow(posX, posY, &data.playerIconAdvMap.pos[0], (char *)arrowColor, (int)scale, (int)rot);

	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004dc44-0x8004dd5c.
// Draw icon on map
void UI_Map_DrawRawIcon(struct UIMap *map, const s32 worldPos[3], int iconID, int colorID, int unused, s16 scale)
{
	int posX;
	int posY;
	u32 *ptrColor;
	struct GameTracker *gGT = sdata->gGT;

	(void)unused;

	posX = worldPos[0];
	posY = worldPos[2];

	UI_Map_GetIconPos(map, &posX, &posY);

	ptrColor = data.ptrColor[colorID];

	struct Icon **iconPtrArray = ICONGROUP_GETICONS(sdata->gGT->iconGroup[UI_MAP_ICON_GROUP]);

#if defined(CTR_NATIVE)
	POLY_GT4 *p = gGT->backBuffer->primMem.cursor;
#endif
	DecalHUD_DrawPolyGT4(iconPtrArray[iconID], posX, posY, &gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT, ptrColor[0], ptrColor[1], ptrColor[2],
	                     ptrColor[3], 0, (int)scale);
#if defined(CTR_NATIVE)
	if ((NativeAspect_IsActive() || (gNativePreciseMinimapEnabled && NATIVE_PGXP_SUPPORTED)) && gGT->backBuffer->primMem.cursor == p + 1)
	{
		float preciseX, preciseY;
		UI_Map_GetIconPosPrecise(map, worldPos, &preciseX, &preciseY);
		// Preserve the icon's existing size and widescreen shape; translate all
		// corners by the precision lost in the legacy map projection.
		const float dx = preciseX - posX;
		const float dy = preciseY - posY;
		NativePgxp_SetScreenXY(&p->x0, p->x0 + dx, p->y0 + dy);
		NativePgxp_SetScreenXY(&p->x1, p->x1 + dx, p->y1 + dy);
		NativePgxp_SetScreenXY(&p->x2, p->x2 + dx, p->y2 + dy);
		NativePgxp_SetScreenXY(&p->x3, p->x3 + dx, p->y3 + dy);
	}
#endif


	return;
}

#if defined(CTR_NATIVE)
// Radii in HUD pixel rows, before the one-pixel outline.
#define UI_MAP_MARKER_RACER_RADIUS    1.8f
#define UI_MAP_MARKER_PLAYER_RADIUS   2.8f
#define UI_MAP_MARKER_WARPBALL_RADIUS 2.2f
#define UI_MAP_MARKER_TARGET_RADIUS   4.6f

// Modern Map: race markers become vector circles centred on the
// world position, instead of sprites hung from their top-left corner.
static void UI_Map_DrawMarker(struct UIMap *map, const s32 worldPos[3], float radius, const u32 colors[4])
{
	float posX, posY;
	UI_Map_GetIconPosPrecise(map, worldPos, &posX, &posY);
	AH_Map_MarkerShape(posX, posY, radius, false, colors);
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004dd5c-0x8004dee8.
void UI_Map_DrawDrivers(struct UIMap *map, struct Thread *bucket, s16 *driverIconCounter)
{
	int kartColor;
	int iconID;
	struct Driver *d;
	struct GameTracker *gGT = sdata->gGT;

	for (/* bucket */; bucket != 0; bucket = bucket->siblingThread, *driverIconCounter = *driverIconCounter + 1)
	{
		// Retail hides race-map driver markers in 2P/4P. Adhoc renders a
		// single fullscreen 1P presentation while retaining 2P simulation.
		if ((gGT->numPlyrCurrGame & 1) == 0)
		{
#if defined(__vita__)
			if (!NativeAdhoc_IsSingleViewRenderActive())
#endif
			{
				continue;
			}
		}

		// Player structure
		d = bucket->object;

		// characterID + 5
		// corresponds with ptrColors
		kartColor = data.characterIDs[d->driverID] + 5;

		// default (AI)
		iconID = UI_MAP_PLAYER_ICON_AI;

		// TO-DO: Should we just spawn player threads
		// and enable the AI flag anyway? What would it do?
		if ((d->actionsFlagSet & ACTION_BOT) == 0)

		{
			// If this is an even numbered frame
			// ptrColors white value
			if ((FPS_HALF(gGT->timer) & 2) == 0)
			{
				kartColor = WHITE;
			}

			// If you're in Adventure Arena
			if ((gGT->gameMode1 & ADVENTURE_ARENA) != 0)
			{
				// Draw dot for Player on 2D Adv Map
				UI_Map_DrawAdvPlayer(map, &bucket->inst->matrix.t[0], UI_MAP_PLAYER_ICON_HUMAN, kartColor,
				                     (d->rotCurr.y + UI_MAP_ARROW_ROT_FLIP) | UI_MAP_ARROW_ROT_FLAG, UI_MAP_ADV_ARROW_SCALE);

				continue;
			}

			// Player
			iconID = UI_MAP_PLAYER_ICON_HUMAN;
		}

#if defined(CTR_NATIVE)
		if (gNativeModernMapEnabled)
		{
			u32 colors[4];
			if (iconID == UI_MAP_PLAYER_ICON_HUMAN)
			{
				// Retail flicks the player's dot to white every other two frames.
				AH_Map_MarkerFlash(colors, WHITE, data.characterIDs[d->driverID] + 5, 4.0f);
			}
			else
			{
				memcpy(colors, data.ptrColor[kartColor], sizeof(colors));
			}
			UI_Map_DrawMarker(map, &bucket->inst->matrix.t[0], (iconID == UI_MAP_PLAYER_ICON_HUMAN) ? UI_MAP_MARKER_PLAYER_RADIUS : UI_MAP_MARKER_RACER_RADIUS,
			                  colors);
			continue;
		}
#endif
		UI_Map_DrawRawIcon(map, &bucket->inst->matrix.t[0], iconID, (s16)kartColor, 0, UI_MAP_ICON_SCALE);
	}
	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004dee8-0x8004dffc.
void UI_Map_DrawGhosts(struct UIMap *map, struct Thread *bucket)
{
	int color;
	struct Driver *d;
	struct GameTracker *gGT = sdata->gGT;

	for (/* bucket */; bucket != 0; bucket = bucket->siblingThread)
	{
		d = bucket->object;

		// if ghost not initialized
		if (d->ghostBoolInit == 0)
		{
			continue;
		}

		// ghost made by player
		if (d->ghostID == 0)
		{
			// flash red and blue

			color = CORTEX_RED;
			if ((FPS_HALF(gGT->timer) & 1) != 0)
			{
				color = CRASH_BLUE;
			}
		}

		// ghost is N Tropy or Oxide
		else
		{
			// N Tropy doesn't flicker
			color = TROPY_LIGHT_BLUE;

			// if timeTrialFlags for this track show [ n tropy beaten, oxide open ]
			if ((sdata->gameProgress.highScoreTracks[gGT->levelID].timeTrialFlags & 2) != 0)
			{
				// oxide flickers

				color = RED;
				if ((FPS_HALF(gGT->timer) & 1) != 0)
				{
					color = WHITE;
				}
			}
		}

#if defined(CTR_NATIVE)
		if (gNativeModernMapEnabled)
		{
			u32 colors[4];
			if (d->ghostID == 0)
			{
				AH_Map_MarkerFlash(colors, CORTEX_RED, CRASH_BLUE, 2.0f);
			}
			else if (color == TROPY_LIGHT_BLUE)
			{
				memcpy(colors, data.ptrColor[color], sizeof(colors));
			}
			else
			{
				AH_Map_MarkerFlash(colors, RED, WHITE, 2.0f);
			}
			UI_Map_DrawMarker(map, &bucket->inst->matrix.t[0], UI_MAP_MARKER_RACER_RADIUS, colors);
			continue;
		}
#endif
		UI_Map_DrawRawIcon(map, &bucket->inst->matrix.t[0], UI_MAP_PLAYER_ICON_AI, color, 0, UI_MAP_ICON_SCALE);
	}
	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004dffc-0x8004e0e0.
void UI_Map_DrawTracking(struct UIMap *map, struct Thread *bucket)
{
	int targetColor;
	struct Instance *inst;
	struct TrackerWeapon *tw;
	struct Driver *d;

	for (/* bucket */; bucket != 0; bucket = bucket->siblingThread)
	{
		// thread -> instance
		inst = bucket->inst;

		// instance -> model -> modelID != warpball
		if (inst->model->id != DYNAMIC_WARPBALL)
		{
			continue;
		}

		// == only draw warpball ==

		// draw warpball
#if defined(CTR_NATIVE)
		if (gNativeModernMapEnabled)
		{
			UI_Map_DrawMarker(map, &inst->matrix.t[0], UI_MAP_MARKER_WARPBALL_RADIUS, data.ptrColor[0]);
		}
		else
#endif
		{
			UI_Map_DrawRawIcon(map, &inst->matrix.t[0], UI_MAP_WARPBALL_ICON, 0, 0, UI_MAP_ICON_SCALE);
		}

		// driver target
		tw = (struct TrackerWeapon *)inst->thread->object;
		d = tw->driverTarget;

		// check if target exists
		if (d == 0)
		{
			continue;
		}

		// == only draw target if target exists ==

		// flicker
		targetColor = CRASH_BLUE;
		if ((FPS_HALF(sdata->gGT->timer) & 1) != 0)
		{
			targetColor = CORTEX_RED;
		}

#if defined(CTR_NATIVE)
		if (gNativeModernMapEnabled)
		{
			// A ring around the target's dot, with a black edge behind it.
			u32 colors[4];
			AH_Map_MarkerFlash(colors, CRASH_BLUE, CORTEX_RED, 2.0f);
			float posX, posY;
			UI_Map_GetIconPosPrecise(map, &d->instSelf->matrix.t[0], &posX, &posY);
			AH_Map_MarkerOutline(posX, posY, UI_MAP_MARKER_TARGET_RADIUS, 0.5f, false, colors[0]);
			AH_Map_MarkerOutline(posX, posY, UI_MAP_MARKER_TARGET_RADIUS, 1.0f, false, 0);
			continue;
		}
#endif
		UI_Map_DrawRawIcon(map, &d->instSelf->matrix.t[0], UI_MAP_WARPBALL_TARGET_ICON, targetColor, 0, UI_MAP_ICON_SCALE);
	}
	return;
}
