#include <common.h>

#if defined(CTR_NATIVE)
#include "platform/native_adhoc.h"
#include "platform/native_pgxp.h"
#endif

#define RED_BEAKER_CENTER_XY 0x02000080u
#define RED_BEAKER_XY_MASK   0xfffeffffu
#define RED_BEAKER_WRAP_MASK 0x01fe00ffu

struct RedBeakerRng
{
	u32 xy;
	s32 z;
};

CTR_STATIC_ASSERT(sizeof(LINE_G2) == 0x14);
CTR_STATIC_ASSERT(offsetof(LINE_G2, tag) == 0x00);
CTR_STATIC_ASSERT(offsetof(LINE_G2, r0) == 0x04);
CTR_STATIC_ASSERT(offsetof(LINE_G2, x0) == 0x08);
CTR_STATIC_ASSERT(offsetof(LINE_G2, r1) == 0x0C);
CTR_STATIC_ASSERT(offsetof(LINE_G2, x1) == 0x10);

static u32 RedBeaker_ReadWord(const void *base, int offset)
{
	return *(const u32 *)(const void *)((const char *)base + offset);
}

static s16 RedBeaker_ReadS16(const void *base, int offset)
{
	return *(const s16 *)(const void *)((const char *)base + offset);
}

static s8 RedBeaker_ReadS8(const void *base, int offset)
{
	return *(const s8 *)(const void *)((const char *)base + offset);
}

static u32 RedBeaker_NextRngByte(u32 *state0, u32 *state1)
{
	u32 state1Shifted = *state1 >> 8;
	u32 mixed = *state0 + state1Shifted;

	*state0 = (*state0 >> 8) | (*state1 << 24);
	mixed += *state0 >> 8;
	mixed <<= 24;
	*state1 = state1Shifted | mixed;

	return mixed;
}

static struct RedBeakerRng RedBeaker_NextRng(u32 *state0, u32 *state1)
{
	u32 xSeed = RedBeaker_NextRngByte(state0, state1);
	u32 ySeed = RedBeaker_NextRngByte(state0, state1);
	u32 zSeed = RedBeaker_NextRngByte(state0, state1);
	struct RedBeakerRng rng;

	rng.xy = ((u32)((s32)xSeed >> 24) & 0xffff) | (u32)((s32)ySeed >> 7);
	rng.z = (s32)zSeed >> 23;

	return rng;
}

static int RedBeaker_IsVisible(u32 gteFlag, u32 sxy0, u32 sxy1, u32 screenBounds)
{
	u32 overlap;
	u32 bounds;

	if ((s32)(gteFlag << 14) < 0)
	{
		return 0;
	}

	overlap = sxy0 & sxy1;
	bounds = ~((sxy0 - screenBounds) | (sxy1 - screenBounds)) | overlap;
	if ((s32)bounds < 0)
	{
		return 0;
	}

	return (s32)(bounds << 16) >= 0;
}

static void RedBeaker_EmitLine(u32 **primCursor, uint32_t *ot, u32 color)
{
	LINE_G2 *line = (LINE_G2 *)*primCursor;

	CtrGpu_WriteColorCode(&line->r0, 0x52000000);
	CtrGpu_WritePackedXY(&line->x0, MFC2(12));
	CtrGpu_WriteColorCode(&line->r1, color);
	CtrGpu_WritePackedXY(&line->x1, MFC2(13));
	CtrGpu_LinkPacket24(ot, &line->tag, line, 0x04000000);
	*primCursor = (u32 *)(line + 1);
}

static void RedBeaker_EmitDrawMode(u32 **primCursor, uint32_t *ot, u32 drawMode)
{
	struct CtrGpuDrawModePacket *packet = (struct CtrGpuDrawModePacket *)*primCursor;

	packet->drawMode = drawMode;
	packet->terminator = 0;
	CtrGpu_LinkPacket24(ot, &packet->tag, packet, 0x02000000);
	*primCursor = (u32 *)(packet + 1);
}

static void RedBeaker_RenderPass(u32 **primCursor, uint32_t *ot, u32 color, u32 drawMode, s32 frameCount, u32 scrollXY, u32 nextScrollXY, s32 scrollZ,
                                 s32 nextScrollZ, u32 spanXY, s32 spanZ, u32 screenBounds, struct PushBuffer *pb,
                                 struct Instance *cloudInst, struct PrimMem *primMem)
{
	u32 state0 = 0x30125400;
	u32 state1 = 0x493583fe;
	struct RedBeakerRng rng = RedBeaker_NextRng(&state0, &state1);
	s32 remaining = frameCount;
	int nativeLayer = -1;
#if defined(CTR_NATIVE) && NATIVE_DRAW3D_SUPPORTED
	if (NATIVE_DRAW3D_ACTIVE() && (u8 *)*primCursor + sizeof(DR_PSYX_DRAW3D) <= (u8 *)primMem->guardEnd)
	{
		NativeDraw3DView view = {0};
		double rotation[9], translation[3];
		float cameraPosition[3];
		NativePgxp_GetPosition(&pb->pos, pb->pos.v, cameraPosition);
		NativePgxp_GetTransform(&pb->matrix_ViewProj, &pb->matrix_ViewProj.m[0][0], pb->matrix_ViewProj.t, rotation, translation);
		// Rain runs before the cloud is queued this frame. Build the anchor
		// from its current world position instead of the previous MVP packet.
		NativePgxp_ModelViewTranslation(rotation, cloudInst->matrix.t, pb->pos.v, cameraPosition, view.translation);
		double scale = (view.translation[2] < 4096.0 ? 0.25 : 1.0) * ((cloudInst->flags & DRAW_HUGE) ? 4.0 : 1.0);
		for (int i = 0; i < 9; i++) view.rotation[i] = rotation[i] * scale / 4096.0;
		view.projection = (float)pb->distanceToScreen_PREV;
		view.centerX = (float)pb->rect.w * 0.5f;
		view.centerY = (float)pb->rect.h * 0.5f;
		view.width = (float)pb->rect.w;
		view.height = (float)pb->rect.h;
		view.mirror = gNativeMirrorModeRenderActive != 0;
		nativeLayer = NativeDraw3D_BeginLayer(&view);
	}
#endif

	while (remaining != 0)
	{
		u32 xy0;
		u32 xy1;
		u32 z0;
		u32 z1;

		remaining--;

		xy0 = ((rng.xy + scrollXY) & RED_BEAKER_WRAP_MASK) - RED_BEAKER_CENTER_XY;
		xy1 = ((rng.xy + nextScrollXY) & RED_BEAKER_WRAP_MASK) - RED_BEAKER_CENTER_XY;

		if ((u32)(xy1 - xy0) == spanXY)
		{
			xy0 &= RED_BEAKER_XY_MASK;
			xy1 &= RED_BEAKER_XY_MASK;

			MTC2(xy0, 0);
			MTC2(xy1, 2);
			MTC2(xy1, 4);

			z0 = (u32)(rng.z + scrollZ) & 0xff;
			z1 = (u32)(rng.z + nextScrollZ) & 0xff;

			if ((s32)(z1 - z0) == spanZ)
			{
				u32 sxy0;
				u32 sxy1;
				u32 gteFlag;

				z0 -= 0x80;
				z1 -= 0x80;

				MTC2(z0, 1);
				MTC2(z1, 3);
				MTC2(z1, 5);

				gte_rtpt_b();
				rng = RedBeaker_NextRng(&state0, &state1);

				sxy0 = MFC2(12);
				gteFlag = CFC2(31);
				sxy1 = MFC2(13);

#if defined(CTR_NATIVE) && NATIVE_DRAW3D_SUPPORTED
				if (nativeLayer >= 0)
				{
					NativeDraw3DVertex vertices[2] = {
						{.x=(s16)xy0, .y=(s16)(xy0 >> 16), .z=(s16)z0},
						{.x=(s16)xy1, .y=(s16)(xy1 >> 16), .z=(s16)z1,
						 .r=(u8)color, .g=(u8)(color >> 8), .b=(u8)(color >> 16)},
					};
					NativeDraw3DMaterial material = {0};
					material.tpage = (u16)drawMode;
					material.flags = NATIVE_DRAW3D_SEMI_TRANS | NATIVE_DRAW3D_ORDERED_BLEND;
					NativeDraw3D_AddLine(nativeLayer, &vertices[0], &vertices[1], &material, 1.0f);
				}
				else
#endif
				if (RedBeaker_IsVisible(gteFlag, sxy0, sxy1, screenBounds))
				{
					RedBeaker_EmitLine(primCursor, ot, color);
				}
			}
		}

		rng = RedBeaker_NextRng(&state0, &state1);
	}

#if defined(CTR_NATIVE) && NATIVE_DRAW3D_SUPPORTED
	if (nativeLayer >= 0)
	{
		NativeDraw3D_EndLayer(nativeLayer);
		DR_PSYX_DRAW3D *marker = (DR_PSYX_DRAW3D *)*primCursor;
		NativeDraw3D_SetMarker(marker, nativeLayer);
		AddPrim(ot, marker);
		*primCursor = (u32 *)(marker + 1);
		return;
	}
#else
	(void)nativeLayer;
#endif
	RedBeaker_EmitDrawMode(primCursor, ot, drawMode);
}

struct RedBeakerRainScratch
{
	u32 centerXY;
	u8 pad_04[0x14];
	u32 colorTop;
	u32 colorBottom;
};

CTR_STATIC_ASSERT(offsetof(struct RedBeakerRainScratch, centerXY) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct RedBeakerRainScratch, colorTop) == 0x18);
CTR_STATIC_ASSERT(offsetof(struct RedBeakerRainScratch, colorBottom) == 0x1C);

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8006dc30-0x8006e26c
void RedBeaker_RenderRain(struct PushBuffer *pb, struct PrimMem *primMem, struct JitPool *rain, u8 numPlyr, int gameMode1)
{
	u32 *prim = (u32 *)primMem->cursor;
	struct RainLocal *firstRain = (struct RainLocal *)rain->taken.first;
	struct RedBeakerRainScratch *scratch = CTR_SCRATCHPAD_PTR(struct RedBeakerRainScratch, 0x30);

	// NOTE(aalhendi): PSX-backfeed blocker: retail saves/restores callee registers in scratchpad 0x00-0x2c and stores 32-bit pointer cursors at 0x34/0x38.
	// Native C keeps pointer cursors as host-width locals; restore those exact scratchpad pointer stores before PSX backfeed.
	if (firstRain == NULL)
	{
		primMem->cursor = prim;
		return;
	}

#if defined(CTR_NATIVE)
	// NOTE: The cloud translation is its queued MVP view position, which is
	// shifted left by 2 for near instances, while the rain offsets are not. The
	// cloud is queued after this runs, so its per-model depth scale is not known
	// yet: use retail order (no depth) instead of a depth 4x too far.
	float nativeDepthContext = NativePgxp_SetDepthContext(0.0f);
#endif
	scratch->centerXY = RED_BEAKER_CENTER_XY;
	scratch->colorTop = 0;
	scratch->colorBottom = 0;

	for (int playerIndex = 0; playerIndex < numPlyr; playerIndex++, pb++)
	{
		struct RainLocal *rainLocal;
		u32 screenBounds;
		uint32_t *otBase;
		int playerOffset;

		CTC2(RedBeaker_ReadWord(&pb->matrix_ViewProj, 0x00), 0);
		CTC2(RedBeaker_ReadWord(&pb->matrix_ViewProj, 0x04), 1);
		CTC2(RedBeaker_ReadWord(&pb->matrix_ViewProj, 0x08), 2);
		CTC2(RedBeaker_ReadWord(&pb->matrix_ViewProj, 0x0c), 3);
		CTC2(RedBeaker_ReadWord(&pb->matrix_ViewProj, 0x10), 4);
#if defined(CTR_NATIVE)
	NativePgxp_LoadTransform(&pb->matrix_ViewProj, &pb->matrix_ViewProj.m[0][0], pb->matrix_ViewProj.t, 0);
#endif


		CTC2((u32)(s32)pb->rect.w << 15, 24);
		CTC2((u32)(s32)pb->rect.h << 15, 25);
		CTC2((u32)pb->distanceToScreen_PREV, 26);

		screenBounds = RedBeaker_ReadWord(pb, 0x20);
		otBase = pb->ptrOT;
		int instancePlayerIndex = playerIndex;
#if defined(__vita__)
		if (NativeAdhoc_IsSingleViewRenderActive())
		{
			instancePlayerIndex = NativeAdhoc_GetLocalPlayerIndex();
		}
#endif
		playerOffset = instancePlayerIndex * sizeof(struct InstDrawPerPlayer);

		for (rainLocal = firstRain; rainLocal != NULL; rainLocal = rainLocal->next)
		{
			char *instBase;
			s32 cloudZ;
			u32 scrollXY;
			s32 scrollZ;
			u32 velocityXY;
			s32 velocityZ;
			u32 nextScrollXY;
			s32 nextScrollZ;
			u8 fade;
			u8 top;
			s32 otOffset;
			uint32_t *ot;

			if (rainLocal->cloudInst == NULL)
			{
				continue;
			}

			scrollXY = CTR_PackS16Pair(rainLocal->scroll.x, rainLocal->scroll.y);
			scrollZ = rainLocal->scroll.z;
			velocityXY = CTR_PackS16Pair(rainLocal->vel.x, rainLocal->vel.y) & RED_BEAKER_XY_MASK;
			velocityZ = rainLocal->vel.z;
			nextScrollXY = (scrollXY + velocityXY) & RED_BEAKER_XY_MASK;
			nextScrollZ = scrollZ + velocityZ;

			if (gameMode1 == 0)
			{
				CTR_WriteU32LE(&rainLocal->scroll.x, nextScrollXY);
				rainLocal->scroll.z = (s16)nextScrollZ;
			}

			instBase = (char *)rainLocal->cloudInst + playerOffset;
			CTC2((u32)(s32)RedBeaker_ReadS16(instBase, 0x8c), 5);
			CTC2((u32)(s32)RedBeaker_ReadS16(instBase, 0x90), 6);
			cloudZ = RedBeaker_ReadS16(instBase, 0x94);
			CTC2((u32)cloudZ, 7);

			if (cloudZ < 0 || cloudZ >= 0xc00)
			{
				continue;
			}

			if (cloudZ < 0x400)
			{
				fade = 0xff;
			}
			else
			{
				fade = (u8)(0xff - (((u32)(cloudZ - 0x400) >> 3) & 0xff));
			}

			top = (u8)(fade - (fade >> 2));
			scratch->colorTop = (u32)top | ((u32)top << 8) | ((u32)fade << 16);
			scratch->colorBottom = (u32)fade | ((u32)fade << 8) | ((u32)fade << 16);

			otOffset = ((cloudZ >> 7) + RedBeaker_ReadS8(instBase, 0x50) - 6);
			if (otOffset < 0)
			{
				otOffset = 0;
			}
			else
			{
				otOffset <<= 2;
			}

			if (otOffset >= 0x1000)
			{
				otOffset = 0xffc;
			}

			ot = (uint32_t *)(void *)((char *)otBase + otOffset);

			RedBeaker_RenderPass(&prim, ot, scratch->colorTop, 0xe1000a20, rainLocal->frameCount, scrollXY, nextScrollXY, scrollZ, nextScrollZ, velocityXY,
			                     velocityZ, screenBounds, pb, rainLocal->cloudInst, primMem);
			RedBeaker_RenderPass(&prim, ot, scratch->colorBottom, 0xe1000a40, rainLocal->frameCount, scrollXY, nextScrollXY, scrollZ, nextScrollZ, velocityXY,
			                     velocityZ, screenBounds, pb, rainLocal->cloudInst, primMem);
		}
	}

#if defined(CTR_NATIVE)
	NativePgxp_SetDepthContext(nativeDepthContext);
#endif
	primMem->cursor = prim;
}
