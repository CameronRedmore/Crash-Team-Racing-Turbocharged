#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b5090-0x800b5210.

int RB_CtrLetter_ThCollide(struct Thread *letterTh, struct Thread *driverTh, void *funcThCollide, struct ScratchpadStruct *sps)
{
	(void)funcThCollide;
	s16 posScreen[2];
	struct Driver *driver;
	struct Instance *letterInst;
	struct PushBuffer *pb;

	if (sps->Input1.modelID != DYNAMIC_PLAYER)
	{
		return 0;
	}

	letterInst = P32_GET(struct Instance *, letterTh->inst);
	driver = P32_GET(void *, driverTh->object);

	pb = &P32_GET(struct GameTracker *, sdata->gGT)->pushBuffer[driver->driverID];
	RB_Fruit_GetScreenCoords(pb, letterInst, &posScreen[0]);

	driver->PickupLetterHUD.startX = pb->rect.x + posScreen[0];
	driver->PickupLetterHUD.startY = pb->rect.y + posScreen[1] - 0x14;
	driver->PickupLetterHUD.cooldown = FPS_DOUBLE(10);
	driver->PickupLetterHUD.numCollected++;
	driver->PickupLetterHUD.modelID = P32_GET(struct Model *, letterInst->model)->id;

	letterInst->scale.x = 0;
	letterInst->scale.y = 0;
	letterInst->scale.z = 0;
	P32_SET(letterInst->thread, 0);
	letterInst->flags |= HIDE_MODEL;

	OtherFX_Play(100, 1);
	letterTh->flags |= THREAD_FLAG_DEAD;

	return 1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b5210-0x800b52dc.

int RB_CtrLetter_LInC(struct Instance *letterInst, struct Thread *driverTh, struct ScratchpadStruct *sps)
{
	struct Thread *letterTh;

	letterTh = P32_GET(struct Thread *, letterInst->thread);
	if (letterTh == NULL)
	{
		letterTh = PROC_BirthWithObject(SIZE_RELATIVE_POOL_BUCKET(4, NONE, SMALL, STATIC), RB_CtrLetter_ThTick, "ctr", NULL);

		P32_SET(letterInst->thread, letterTh);
		if (letterTh == NULL)
		{
			return 0;
		}

		P32_SET(letterTh->inst, letterInst);
		P32_SET(letterTh->funcThCollide, (void *)RB_CtrLetter_ThCollide);
		letterTh = P32_GET(struct Thread *, letterInst->thread);
	}

	if ((letterTh == NULL) || (P32_GET(void *, letterTh->funcThCollide) == NULL))
	{
		return 0;
	}

	if (letterInst->scale.x == 0)
	{
		return 0;
	}

	return ((ThreadScratchCollideFunc)P32_GET(void *, letterTh->funcThCollide))(letterTh, driverTh, P32_GET(void *, letterTh->funcThCollide), sps);
}

SVec3 letterLightDir = {{0x94F, 0x94F, -0x94F}};

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b52dc-0x800b5334.

void RB_CtrLetter_ThTick(struct Thread *t)
{
	struct Instance *letterInst;
	struct CtrLetter *letterObj;

	letterInst = P32_GET(struct Instance *, t->inst);
	letterObj = P32_GET(void *, t->object);

	// rotate each frame
	letterObj->rot.y += CTR_FRAME_STEP(0x40, sdata->gGT->timer);
	ConvertRotToMatrix(&letterInst->matrix, &letterObj->rot);

	Vector_SpecLightSpin3D(letterInst, &letterObj->rot, &letterLightDir);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b5334-0x800b53e0.

void RB_CtrLetter_LInB(struct Instance *inst)
{
	struct CtrLetter *letterObj;
	struct Thread *t;

	if (P32_GET(struct Thread *, inst->thread) == NULL)
	{
		t = PROC_BirthWithObject(
		    // creation flags
		    SIZE_RELATIVE_POOL_BUCKET(sizeof(struct CtrLetter), NONE, SMALL, STATIC),

		    RB_CtrLetter_ThTick, // behavior
		    "ctr",               // debug name
		    0                    // thread relative
		);

		P32_SET(inst->thread, t);
		if (t == 0)
		{
			return;
		}

		P32_SET(t->funcThCollide, (void *)RB_CtrLetter_ThCollide);
		P32_SET(t->inst, inst);

		letterObj = ((struct CtrLetter *)P32_GET(void *, t->object));
		letterObj->rot.x = 0;
		letterObj->rot.y = 0;
		letterObj->rot.z = 0;

		inst->scale.x = 0x1800;
		inst->scale.y = 0x1800;
		inst->scale.z = 0x1800;

		inst->colorRGBA = 0xffc8000;

		inst->flags |= (DRAW_TRANSPARENT | USE_SPECULAR_LIGHT);
	}

	RB_Default_LInB(inst);
}
