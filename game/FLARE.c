#include <common.h>

#if defined(CTR_NATIVE)
#include "platform/native_pgxp.h"
#endif

static inline u32 FLARE_PackXY(s16 x, s16 y)
{
	return (u16)x | ((u32)(u16)y << 16);
}

static inline void FLARE_LoadGridRow(s16 y)
{
	MTC2(FLARE_PackXY(-409, y), 0);
	MTC2(0, 1);
	MTC2(FLARE_PackXY(0, y), 2);
	MTC2(0, 3);
	MTC2(FLARE_PackXY(409, y), 4);
	MTC2(0, 5);
}

static inline void FLARE_WriteTexture(POLY_GT4 *poly, struct Icon *icon, u32 texWord1)
{
	CtrGpu_WritePackedUVWord(&poly->u0, CTR_ReadU32LE(&icon->texLayout.u0));
	CtrGpu_WritePackedUVWord(&poly->u1, texWord1);
	CtrGpu_WritePackedUV(&poly->u2, CTR_ReadU16LE(&icon->texLayout.u2));
	CtrGpu_WritePackedUV(&poly->u3, CTR_ReadU16LE(&icon->texLayout.u3));
}

static inline void FLARE_WriteColors(POLY_GT4 *poly)
{
	CtrGpu_WriteColorCode(&poly->r0, 0x3e000000);
	CtrGpu_WriteColorCode(&poly->r1, 0);
	CtrGpu_WriteColorCode(&poly->r2, 0);
	CtrGpu_WriteColorCode(&poly->r3, 0x007f7f7f);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80024c4c-0x80025138.
void FLARE_ThTick(struct Thread *th)
{
	struct GameTracker *gGT = sdata->gGT;
	struct PushBuffer *pb = &gGT->pushBuffer[0];
	s32 *flare = th->object;
	s32 timer = flare[0];

	flare[0] = timer + 1;

	// timer counts rendered frames; the 30 FPS breakpoints below are scaled to match
	if (timer >= FPS_DOUBLE(20))
	{
		th->flags |= THREAD_FLAG_DEAD;
		return;
	}

	POLY_GT4 *prim = gGT->backBuffer->primMem.cursor;
	if ((char *)(prim + 4) >= (char *)gGT->backBuffer->primMem.guardEnd)
	{
		return;
	}

	PushBuffer_SetPsyqGeom(pb);
	gte_SetLightMatrix(&pb->matrix_ViewProj);

	s32 relX = ((s32) * (s16 *)&flare[1] - pb->matrix_Camera.t[0]) << 2;
	s32 relY = ((s32) * (s16 *)((u8 *)flare + 6) - pb->matrix_Camera.t[1]) << 2;
	s32 relZ = ((s32) * (s16 *)&flare[2] - pb->matrix_Camera.t[2]) << 2;

	MTC2(((u32)(u16)relX) | ((u32)relY << 16), 0);
	MTC2(relZ, 1);
	gte_llv0();

	CTC2(MFC2(25), 5);
	CTC2(MFC2(26), 6);
	CTC2(MFC2(27), 7);

	s32 oldMin;
	s32 oldMax;
	s32 newMin;
	s32 newMax;
	if (timer < FPS_DOUBLE(2))
	{
		oldMin = 0;
		oldMax = FPS_DOUBLE(2);
		newMin = 0x400;
		newMax = 0x2000;
	}
	else if (timer < FPS_DOUBLE(4))
	{
		oldMin = FPS_DOUBLE(2);
		oldMax = FPS_DOUBLE(4);
		newMin = 0x2000;
		newMax = 0xc00;
	}
	else if (timer < FPS_DOUBLE(8))
	{
		oldMin = FPS_DOUBLE(4);
		oldMax = FPS_DOUBLE(8);
		newMin = 0xc00;
		newMax = 0x266;
	}
	else
	{
		oldMin = FPS_DOUBLE(8);
		oldMax = FPS_DOUBLE(20);
		newMin = 0x266;
		newMax = 0;
	}

	s32 scale = VehCalc_MapToRange(timer, oldMin, oldMax, newMin, newMax);
	u32 angle = ((u32)flare[0] << 12) / FPS_DOUBLE(20);
	s32 sin = (MATH_Sin(angle) * scale) >> 12;
	s32 cos = (MATH_Cos(angle) * scale) >> 12;
	s32 scaledCos = (cos << 9) / 0xf0;
	s32 scaledSin = (sin << 9) / 0xf0;

	CTC2(((u16)scaledCos) | ((u32)(u16)-scaledSin << 16), 0);
	CTC2((u32)sin << 16, 1);
	CTC2((u16)cos, 2);
	CTC2(0, 3);
	CTC2(scale, 4);

	struct Icon *icon = gGT->ptrIcons[0x87];
	if (icon == NULL)
	{
		return;
	}

	u32 texWord1 = (CTR_ReadU32LE(&icon->texLayout.u1) & 0xff9fffff) | 0x00200000;
	POLY_GT4 *p0 = prim;
	POLY_GT4 *p1 = prim + 1;
	POLY_GT4 *p2 = prim + 2;
	POLY_GT4 *p3 = prim + 3;

	FLARE_LoadGridRow(-409);
	gte_rtpt();
	FLARE_WriteTexture(p0, icon, texWord1);
	FLARE_WriteTexture(p1, icon, texWord1);
	CtrGpu_WritePackedXY(&p0->x0, MFC2(12));
	CtrGpu_WritePackedXY(&p0->x1, MFC2(13));
	CtrGpu_WritePackedXY(&p1->x1, MFC2(13));
	CtrGpu_WritePackedXY(&p1->x0, MFC2(14));

	FLARE_LoadGridRow(0);
	gte_rtpt();
	FLARE_WriteColors(p0);
	FLARE_WriteColors(p1);
	FLARE_WriteTexture(p2, icon, texWord1);
	CtrGpu_WritePackedXY(&p0->x2, MFC2(12));
	CtrGpu_WritePackedXY(&p0->x3, MFC2(13));
	CtrGpu_WritePackedXY(&p1->x3, MFC2(13));
	CtrGpu_WritePackedXY(&p1->x2, MFC2(14));
	CtrGpu_WritePackedXY(&p2->x2, MFC2(12));
	CtrGpu_WritePackedXY(&p2->x3, MFC2(13));
	CtrGpu_WritePackedXY(&p3->x3, MFC2(13));
	CtrGpu_WritePackedXY(&p3->x2, MFC2(14));
	s32 depth = MFC2(18);

	FLARE_LoadGridRow(409);
	gte_rtpt();
	FLARE_WriteColors(p2);
	FLARE_WriteColors(p3);
	FLARE_WriteTexture(p3, icon, texWord1);
	CtrGpu_WritePackedXY(&p2->x0, MFC2(12));
	CtrGpu_WritePackedXY(&p2->x1, MFC2(13));
	CtrGpu_WritePackedXY(&p3->x1, MFC2(13));
	CtrGpu_WritePackedXY(&p3->x0, MFC2(14));

	depth = (depth >> 8) - 2;
	if (depth < 0)
	{
		depth = 0;
	}
	if (depth > 0x3ff)
	{
		depth = 0x3ff;
	}

	uint32_t *ot = &pb->ptrOT[depth];
#if defined(CTR_NATIVE) && NATIVE_DRAW3D_SUPPORTED
	if (NATIVE_DRAW3D_ACTIVE())
	{
		NativeDraw3DView view = {0};
		double rotation[9], translation[3];
		NativePgxp_GetTransform(&pb->matrix_ViewProj, &pb->matrix_ViewProj.m[0][0], pb->matrix_ViewProj.t, rotation, translation);
		const double position[3] = {relX, relY, relZ};
		for (int i = 0; i < 3; i++)
			view.translation[i] = (rotation[i*3] * position[0] + rotation[i*3+1] * position[1] + rotation[i*3+2] * position[2]) / 16384.0;
		const s16 billboard[9] = {scaledCos, -scaledSin, 0, sin, cos, 0, 0, 0, scale};
		for (int i = 0; i < 9; i++) view.rotation[i] = billboard[i] / 16384.0;
		view.projection = (float)pb->distanceToScreen_PREV;
		view.centerX = (float)pb->rect.w * 0.5f;
		view.centerY = (float)pb->rect.h * 0.5f;
		view.width = (float)pb->rect.w;
		view.height = (float)pb->rect.h;
		// This tick precedes MainFrame_RenderFrame's mirror-phase switch.
		view.mirror = gNativeMirrorModeEnabled && !gGT->boolDemoMode &&
			!(gGT->gameMode1 & (GAME_CUTSCENE | MAIN_MENU | LOADING)) &&
			gGT->levelID >= DINGO_CANYON && gGT->levelID < INTRO_RACE_TODAY &&
			LOAD_IsOpen_RacingOrBattle() && sdata->Loading.stage == LOAD_IDLE;
		int layer = NativeDraw3D_BeginLayer(&view);
		if (layer >= 0)
		{
			for (int quad = 0; quad < 4; quad++)
			{
				const POLY_GT4 *poly = prim + quad;
				const u8 uv[4][2] = {{poly->u0,poly->v0}, {poly->u1,poly->v1}, {poly->u2,poly->v2}, {poly->u3,poly->v3}};
				const u8 *colors[4] = {&poly->r0, &poly->r1, &poly->r2, &poly->r3};
				NativeDraw3DVertex vertices[4];
				for (int i = 0; i < 4; i++)
					vertices[i] = (NativeDraw3DVertex){.x=(i & 1) ? 0 : ((quad & 1) ? 409 : -409),
						.y=(i & 2) ? 0 : ((quad & 2) ? 409 : -409), .z=0, .u=uv[i][0], .v=uv[i][1],
						.r=colors[i][0], .g=colors[i][1], .b=colors[i][2]};
				NativeDraw3DMaterial material = {0};
				material.tpage = poly->tpage;
				material.clut = poly->clut;
				material.depthBias = -2;
				material.flags = NATIVE_DRAW3D_TEXTURED | NATIVE_DRAW3D_SEMI_TRANS |
				                 NATIVE_DRAW3D_DOUBLE_SIDED | NATIVE_DRAW3D_ORDERED_BLEND;
				NativeDraw3D_AddQuad(layer, &vertices[0], &vertices[1], &vertices[2], &vertices[3], &material);
			}
			NativeDraw3D_EndLayer(layer);
			DR_PSYX_DRAW3D *marker = (DR_PSYX_DRAW3D *)prim;
			NativeDraw3D_SetMarker(marker, layer);
			AddPrim(ot, marker);
			gGT->backBuffer->primMem.cursor = marker + 1;
			return;
		}
	}
#endif
	p0->tag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(p1), 0x0c000000);
	p1->tag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(p2), 0x0c000000);
	p2->tag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(p3), 0x0c000000);
	p3->tag = CtrGpu_PackOTTag(*ot, 0x0c000000);
	*ot = CtrGpu_PrimToOTLink24(p0);

	gGT->backBuffer->primMem.cursor = prim + 4;
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80025138-0x800251ac.
void FLARE_Init(s16 *pos)
{
	// 0xc = size
	// 0 = no relation to param4
	// 0x300 = SmallStackPool
	// 0xd = "other" thread bucket
	struct Thread *th = PROC_BirthWithObject(0xc030d, FLARE_ThTick, rdata.s_lensflare, NULL);
	if (th != NULL)
	{
		// Get the pointer to flare, attached to the thread
		int *flare = th->object;
		*flare = 0; // frameCount = 0
		memcpy(&flare[1], pos, 2 * sizeof(int));
	}
}
