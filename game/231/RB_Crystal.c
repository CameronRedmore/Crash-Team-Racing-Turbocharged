#include <common.h>

SVec3 crystalLightDir = {{0x94F, 0x94F, 0x94F}};

static void RB_Crystal_RotateStep(struct Instance *crystalInst, struct Crystal *crystalObj)
{
	crystalObj->rot.y += CTR_FRAME_STEP(0x40, sdata->gGT->timer);
	ConvertRotToMatrix(&crystalInst->matrix, &crystalObj->rot);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4c5c-0x800b4dd8.
int RB_Crystal_ThCollide(struct Thread *crystalTh, struct Thread *driverTh, void *funcThCollide, struct ScratchpadStruct *sps)
{
	(void)funcThCollide;
	struct PushBuffer *pb;
	s16 posScreen[2];
	struct Driver *driver;
	struct Instance *crystalInst;
	int modelID;

	modelID = sps->Input1.modelID;
	crystalInst = P32_GET(struct Instance *, crystalTh->inst);

	// wumpa fruit or crystal can be grabbed
	// by player, or robotcar, and there's no
	// AIs in Crystal Challenge anyway
	if (
	    // not player model
	    (modelID != DYNAMIC_PLAYER) &&

	    // not bot model
	    (modelID != DYNAMIC_ROBOT_CAR))
	{
		return 0;
	}

	// player gets pickup HUD feedback,
	// bots only erase the crystal/fruit
	if (modelID == DYNAMIC_PLAYER)
	{
		// get driver object, get screen coords
		driver = P32_GET(void *, driverTh->object);
		pb = &P32_GET(struct GameTracker *, sdata->gGT)->pushBuffer[driver->driverID];
		RB_Fruit_GetScreenCoords(pb, crystalInst, &posScreen[0]);

		// lasts 5 frames, give start position, count numCollected
		driver->PickupWumpaHUD.startX = pb->rect.x + posScreen[0];
		driver->PickupWumpaHUD.startY = pb->rect.y + posScreen[1] - 0x14;
		driver->PickupWumpaHUD.cooldown = 5;
		driver->PickupWumpaHUD.numCollected++;
	}

	CTR_WriteU32LE(&crystalInst->scale.x, 0);
	crystalInst->scale.z = 0;
	P32_SET(crystalInst->thread, 0);

	// play sound
	PlaySound3D(0x43, crystalInst);
	crystalTh->flags |= THREAD_FLAG_DEAD;

	return 1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4dd8-0x800b4e7c.
void RB_Crystal_ThTick(struct Thread *t)
{
	int sine;
	struct Instance *crystalInst;
	struct Crystal *crystalObj;

	crystalInst = P32_GET(struct Instance *, t->inst);
	crystalObj = P32_GET(void *, t->object);

	RB_Crystal_RotateStep(crystalInst, crystalObj);
	RB_Crystal_RotateStep(crystalInst, crystalObj);

	// sine curve for vertical bounce
	sine = MATH_Sin(crystalObj->rot.y);

	// set posY
	crystalInst->matrix.t[1] = P32_GET(struct InstDef *, crystalInst->instDef)->pos.y + // original posY
	                           ((sine << 4) >> 0xc) +        // sine (bounce up/down)
	                           0x30;                         // airborne bump

	Vector_SpecLightSpin3D(crystalInst, &crystalObj->rot, &crystalLightDir);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4e7c-0x800b4f48.
int RB_Crystal_LInC(struct Instance *crystalInst, struct Thread *driverTh, struct ScratchpadStruct *sps)
{
	struct Thread *crystalTh;

	crystalTh = P32_GET(struct Thread *, crystalInst->thread);
	if (crystalTh == NULL)
	{
		crystalTh = PROC_BirthWithObject(
		    // creation flags
		    SIZE_RELATIVE_POOL_BUCKET(sizeof(struct Crystal), NONE, SMALL, STATIC),

		    RB_Crystal_ThTick, // behavior
		    "crystal",         // debug name
		    0                  // thread relative
		);

		P32_SET(crystalInst->thread, crystalTh);
		if (crystalTh == NULL)
		{
			return 0;
		}

		P32_SET(crystalTh->inst, crystalInst);
		P32_SET(crystalTh->funcThCollide, (void *)RB_Crystal_ThCollide);
		crystalTh = P32_GET(struct Thread *, crystalInst->thread);
	}

	if ((crystalTh == NULL) || (P32_GET(void *, crystalTh->funcThCollide) == NULL))
	{
		return 0;
	}

	if (crystalInst->scale.x == 0)
	{
		return 0;
	}

	return ((ThreadScratchCollideFunc)P32_GET(void *, crystalTh->funcThCollide))(crystalTh, driverTh, P32_GET(void *, crystalTh->funcThCollide), sps);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4f48-0x800b4fe4.
void RB_Crystal_LInB(struct Instance *inst)
{
	struct Crystal *crystalObj;
	struct Thread *t;

	if (P32_GET(struct Thread *, inst->thread) == NULL)
	{
		t = PROC_BirthWithObject(
		    // creation flags
		    SIZE_RELATIVE_POOL_BUCKET(sizeof(struct Crystal), NONE, SMALL, STATIC),

		    RB_Crystal_ThTick, // behavior
		    "crystal",         // debug name
		    0                  // thread relative
		);

		P32_SET(inst->thread, t);
		if (t == 0)
		{
			return;
		}

		crystalObj = ((struct Crystal *)P32_GET(void *, t->object));
		P32_SET(t->inst, inst);
		P32_SET(t->funcThCollide, (void *)RB_Crystal_ThCollide);

		// rotX, rotY, rotZ
		CTR_WriteU32LE(&crystalObj->rot.x, 0);
		crystalObj->rot.z = 0;

		inst->colorRGBA = 0xd22fff0;

		inst->flags |= USE_SPECULAR_LIGHT;
	}

	RB_Default_LInB(inst);
}
