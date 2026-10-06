#include <common.h>

#if defined(CTR_NATIVE)
#include "platform/native_adhoc.h"
#include "platform/native_pgxp.h"
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b95fc-0x800b9848.
void RB_Spider_DrawWebs(struct Thread *t, struct PushBuffer *pb)
{
	typedef struct
	{
		u32 tag;
		u32 tpage;
		LINE_F2 f2;
	} multiCmdPacket;

	typedef struct
	{
		u32 topXY;
		u32 bottomXY;
		s32 z;
	} WebLine;

	CTR_STATIC_ASSERT(sizeof(multiCmdPacket) == 0x18);
	CTR_STATIC_ASSERT(sizeof(WebLine) == 0xc);

	struct GameTracker *gGT;
	struct PrimMem *primMem;
	MATRIX *m;
	multiCmdPacket *p;
	multiCmdPacket *nextPrim;

	u32 lineColor;
	u32 *ot;
	int depth;
	WebLine *scratchpad;
	WebLine *line;
	int numSpiders;

	gGT = P32_GET(struct GameTracker *, sdata->gGT);
	primMem = &P32_GET(struct DB *, gGT->backBuffer)->primMem;

	// quit if there are no spiders
	if (t == NULL)
	{
		return;
	}

	scratchpad = CTR_SCRATCHPAD_PTR(WebLine, 0);
	line = scratchpad;

	// all threads
	for (numSpiders = 0; t != NULL; numSpiders++)
	{
		struct Instance *inst = P32_GET(struct Instance *, t->inst);
		struct InstDef *instDef = P32_GET(struct InstDef *, inst->instDef);
		u16 x = (u16)instDef->pos.x;
		s32 z = instDef->pos.z;

		line->topXY = (u32)x | ((u32)(u16)(instDef->pos.y + 0x540) << 16);
		line->bottomXY = (u32)x | ((u32)(u16)(inst->matrix.t[1] + 0x60) << 16);
		line->z = z;
		line++;

		t = P32_GET(struct Thread *, t->siblingThread);
	}

	int i, j;
	int numPlyr;
	numPlyr = gGT->numPlyrCurrGame;

	p = P32_GET(void *, primMem->cursor);
	nextPrim = p + (numSpiders * numPlyr);
	if (nextPrim >= (multiCmdPacket *)P32_GET(void *, primMem->guardEnd))
	{
		return;
	}

	// loop through all players
	for (i = 0; i < numPlyr; i++)
	{
		m = &pb->matrix_ViewProj;

		// store on GTE
		gte_SetRotMatrix(m);
		gte_SetTransMatrix(m);

		line = scratchpad;

		// loop through spiders
		for (j = 0; j < numSpiders; j++)
		{
			MTC2(line->topXY, 0);
			MTC2((u32)line->z, 1);
			MTC2(line->bottomXY, 2);
			MTC2((u32)line->z, 3);
			gte_rtpt_b();
			depth = (s32)MFC2(17);

			// if line is close enough to the screen
			// to be seen, then generate primitives
			if ((u32)(depth - 1) < (0x1200 - 1))
			{
				p->tpage = 0xe1000a20;
				p->f2.tag = 0;

				CtrGpu_WritePackedXY(&p->f2.x0, MFC2(12));
				CtrGpu_WritePackedXY(&p->f2.x1, MFC2(13));

				lineColor = 0x3f;
				if (depth > 0xa00)
				{
					lineColor = ((0x1200 - depth) * 0x3f) >> 0xb;
				}

				p->f2.r0 = lineColor;
				p->f2.g0 = lineColor;
				p->f2.b0 = lineColor;
				p->f2.code = 0x42;

				depth = depth >> 6;
				if (depth > 0x3ff)
				{
					depth = 0x3ff;
				}

				// pushBuffer 0xf4, ptrOT
				ot = (u32 *)&P32_GET(uint32_t *, pb->ptrOT)[depth];

#if defined(CTR_NATIVE) && NATIVE_DRAW3D_SUPPORTED
				if (NATIVE_DRAW3D_ACTIVE())
				{
					NativeDraw3DView view = {0};
					double rotation[9], translation[3];
					NativePgxp_GetTransform(m, &m->m[0][0], m->t, rotation, translation);
					for (int k = 0; k < 9; k++) view.rotation[k] = rotation[k] / 4096.0;
					for (int k = 0; k < 3; k++) view.translation[k] = translation[k];
					view.projection = (float)pb->distanceToScreen_PREV;
					view.centerX = (float)pb->rect.w * 0.5f;
					view.centerY = (float)pb->rect.h * 0.5f;
					view.width = (float)pb->rect.w;
					view.height = (float)pb->rect.h;
					view.mirror = gNativeMirrorModeRenderActive != 0;
					int layer = NativeDraw3D_BeginLayer(&view);
					if (layer >= 0)
					{
						NativeDraw3DVertex vertices[2] = {
							{.x=(s16)line->topXY, .y=(s16)(line->topXY >> 16), .z=(s16)line->z,
							 .r=lineColor, .g=lineColor, .b=lineColor},
							{.x=(s16)line->bottomXY, .y=(s16)(line->bottomXY >> 16), .z=(s16)line->z,
							 .r=lineColor, .g=lineColor, .b=lineColor},
						};
						NativeDraw3DMaterial material = {0};
						material.tpage = 0x20;
						material.flags = NATIVE_DRAW3D_SEMI_TRANS | NATIVE_DRAW3D_ORDERED_BLEND;
						NativeDraw3D_AddLine(layer, &vertices[0], &vertices[1], &material, 1.0f);
						NativeDraw3D_EndLayer(layer);
						DR_PSYX_DRAW3D *marker = (DR_PSYX_DRAW3D *)p;
						NativeDraw3D_SetMarker(marker, layer);
						AddPrim(ot, marker);
						p = (multiCmdPacket *)(marker + 1);
						line++;
						continue;
					}
				}
#endif
				// prim header, OT and prim len
				*(int *)p = CtrGpu_PackOTTag(*ot, 0x5000000);
				*ot = CtrGpu_PrimToOTLink24(p);
				p++;
			}

			line++;
		}

		pb++;
	}

	P32_SET(primMem->cursor, p);
}

s16 spiderArr[] = {
    // first 13
    0x4c0, 0x439, 0x3A6, 0x306, 0x266, 0x1c8, 0x139, 0xb9, 0x59, 0x17, 0, 0, 0,

    // next 13
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b9848-0x800b9bc0.

void RB_Spider_ThTick(struct Thread *t)
{
	u8 prevKartState;
	struct GameTracker *gGT;
	struct Instance *hitInst;
	struct Driver *victim;
	struct Instance *spiderInst;
	struct Spider *spider;

	spider = P32_GET(void *, t->object);
	spiderInst = P32_GET(struct Instance *, t->inst);

	if (spider->delay != 0)
	{
#if CTR_NATIVE_60FPS
		if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
		{
			spider->delay--;
		}
#else
		spider->delay--;
#endif
		return;
	}

	spider->unused++;

	// If spider is on ground
	if (spider->isNearRoof == 0)
	{
		if (4 < spider->animLoopCount)
		{
			// Play animation backwards
			s16 animFrame = spiderInst->animFrame;
#if CTR_NATIVE_60FPS
			if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
			{
				animFrame--;
			}
#else
			animFrame--;
#endif

			// if animation is at beginning
			if (animFrame < 1)
			{
				spiderInst->animFrame = 0;
				spider->animLoopCount = 0;
				spider->isNearRoof = 1;
				goto setWiggleAnimation;
			}

			spiderInst->animFrame = animFrame;

			// last frame of last animation
			if (animFrame == 0xc)
			{
				PlaySound3D(0x79, spiderInst);
			}

			goto updatePosScale;
		}

		s16 animFrame = spiderInst->animFrame;
		int numAnimFrames = INSTANCE_GetNumAnimFrames(spiderInst, spiderInst->animIndex);

		if (numAnimFrames <= animFrame + 1)
		{
			spiderInst->animFrame = 0;
			s16 animLoopCount = spider->animLoopCount;
			spider->animLoopCount = animLoopCount + 1;

			if ((s16)(animLoopCount + 1) == 5)
			{
				spiderInst->animIndex = 0;
				spiderInst->animFrame = INSTANCE_GetNumAnimFrames(spiderInst, 0) - 1;
			}

			goto checkCollision;
		}
	}

	// if spider is near ceiling
	else
	{
		if (4 < spider->animLoopCount)
		{
			s16 animFrame = spiderInst->animFrame;
			int numAnimFrames = INSTANCE_GetNumAnimFrames(spiderInst, 0);

			if (animFrame + 1 < numAnimFrames)
			{
#if CTR_NATIVE_60FPS
				if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
				{
					spiderInst->animFrame++;
				}
#else
				spiderInst->animFrame++;
#endif
			}
			else
			{
				spider->animLoopCount = 0;
				spider->isNearRoof = 0;
			setWiggleAnimation:
				spiderInst->animIndex = 1;
			}

		updatePosScale:
			spiderInst->matrix.t[1] = (int)P32_GET(struct InstDef *, spiderInst->instDef)->pos.y + spiderArr[spiderInst->animFrame];

			if (spiderInst->animFrame < 0xb)
			{
				P32_GET(struct Instance *, spider->shadowInst)->scale.x = (s16)((spiderInst->animFrame << 0xc) / 10) + 0x1800;
				P32_GET(struct Instance *, spider->shadowInst)->scale.z = (s16)((spiderInst->animFrame << 0xc) / 10) + 0x1800;
			}

			goto checkCollision;
		}

		s16 animFrame = spiderInst->animFrame;
		int numAnimFrames = INSTANCE_GetNumAnimFrames(spiderInst, spiderInst->animIndex);

		if (numAnimFrames <= animFrame + 1)
		{
			spiderInst->animFrame = 0;
			s16 animLoopCount = spider->animLoopCount;
			spider->animLoopCount = animLoopCount + 1;

			if ((s16)(animLoopCount + 1) == 5)
			{
				spiderInst->animIndex = 0;
				spiderInst->animFrame = 0;
				PlaySound3D(0x7a, spiderInst);
			}

			goto checkCollision;
		}
	}

#if CTR_NATIVE_60FPS
	if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
	{
		spiderInst->animFrame++;
	}
#else
	spiderInst->animFrame++;
#endif

checkCollision:
	gGT = P32_GET(struct GameTracker *, sdata->gGT);

	hitInst = (struct Instance *)LinkedCollide_Radius(spiderInst, t, P32_GET(struct Thread *, gGT->threadBuckets[PLAYER].thread), 0x9000);
	if (hitInst == NULL)
	{
		hitInst = (struct Instance *)LinkedCollide_Radius(spiderInst, t, P32_GET(struct Thread *, gGT->threadBuckets[ROBOT].thread), 0x9000);
		if (hitInst == NULL)
		{
			hitInst = (struct Instance *)LinkedCollide_Radius(spiderInst, t, P32_GET(struct Thread *, gGT->threadBuckets[MINE].thread), 0x9000);
			if (hitInst != NULL)
			{
				((ThreadSimpleCollideFunc)P32_GET(void *, P32_GET(struct Thread *, hitInst->thread)->funcThCollide))(P32_GET(struct Thread *, hitInst->thread));
			}

			return;
		}

		victim = (struct Driver *)P32_GET(void *, P32_GET(struct Thread *, hitInst->thread)->object);
		RB_Hazard_HurtDriver(victim, 1, 0, 0);
		return;
	}

	victim = (struct Driver *)P32_GET(void *, P32_GET(struct Thread *, hitInst->thread)->object);
	prevKartState = victim->kartState;
	if ((RB_Hazard_HurtDriver(victim, 1, 0, 0) != 0) && (prevKartState != KS_SPINNING))
	{
		OtherFX_Play(0x7b, 1);
#if defined(__vita__)
		if (NativeAdhoc_ShouldPresentDriver(victim->driverID))
#endif
		{
			Voiceline_RequestPlayDriver(1, victim->driverID, 0x10);
		}
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b9bc0-0x800b9bd4.

int RB_Spider_ThCollide(struct Thread *spiderThread, struct Thread *driverTh, void *funcThCollide, struct ScratchpadStruct *sps)
{
	(void)spiderThread;
	(void)driverTh;
	(void)funcThCollide;

	return sps->Input1.modelID == DYNAMIC_PLAYER;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b9bd4-0x800b9dd8.

void RB_Spider_LInB(struct Instance *inst)
{
	struct Spider *spider;
	SVec3 rot;
	struct Thread *t;
	struct Instance *shadowInst;
	int spiderID;

	if (P32_GET(struct Thread *, inst->thread) != NULL)
	{
		return;
	}

	t = PROC_BirthWithObject(SIZE_RELATIVE_POOL_BUCKET(sizeof(struct Spider), NONE, SMALL, SPIDER), RB_Spider_ThTick, "spider", 0);
	P32_SET(inst->thread, t);
	if (t == NULL)
	{
		return;
	}

	spider = P32_GET(void *, t->object);
	P32_SET(t->funcThCollide, (void *)RB_Spider_ThCollide);
	P32_SET(t->inst, inst);

	inst->scale.x = 0x1c00;
	inst->scale.y = 0x1c00;
	inst->scale.z = 0x1c00;
	inst->animIndex = 1;

	spiderID = inst->name[strlen(inst->name) - 1] - '0';
	spider->spiderID = spiderID;
	spider->isNearRoof = 1;
	spider->animLoopCount = 0;

	if (spiderID == 3)
	{
		spider->delay = 91;
	}
	else if (spiderID == 2)
	{
		spider->delay = 69;
	}
	else
	{
		spider->delay = 0;
	}

	shadowInst = INSTANCE_Birth3D(P32_GET(struct Model *, P32_GET(struct GameTracker *, sdata->gGT)->modelPtr[DYNAMIC_SPIDERSHADOW]), 0, t);

	P32_SET(spider->shadowInst, shadowInst);

	CTR_MatrixCopyRot(&shadowInst->matrix, &inst->matrix);

	shadowInst->matrix.t[0] = inst->matrix.t[0];
	shadowInst->matrix.t[1] = inst->matrix.t[1] - 8;
	shadowInst->matrix.t[2] = inst->matrix.t[2];
	inst->matrix.t[1] += 0x4c0;

	shadowInst->scale.x = 0x2000;
	shadowInst->scale.y = 0x2000;
	shadowInst->scale.z = 0x2000;

	rot.x = 0;
	rot.y = 0x200;
	rot.z = 0;
	ConvertRotToMatrix(&shadowInst->matrix, &rot);
}
