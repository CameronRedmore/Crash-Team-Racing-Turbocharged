#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b7b8c-0x800b81e8.

struct ParticleEmitter emSet_OrcaSplash[7] = {
    [0] =
        {
            .flags = 1,
            .initOffset = 0xC,
            .InitTypes.FuncInit =
                {
                    .particle_funcPtr = 0,
                    .particle_colorFlags = 0x4A0,
                    .particle_lifespan = 0xF,
                    .particle_Type = 0,
                },
        },

    [1] =
        {
            .flags = 0x12,
            .initOffset = 0,
            .InitTypes.AxisInit =
                {
                    .baseValue = {.startVal = 0, .velocity = -0x1400, .accel = 0},
                    .rngSeed = {.startVal = 0, .velocity = 0x2800, .accel = 0},
                },
        },

    [2] =
        {
            .flags = 0x12,
            .initOffset = 2,
            .InitTypes.AxisInit =
                {
                    .baseValue = {.startVal = 0, .velocity = -0x1400, .accel = 0},
                    .rngSeed = {.startVal = 0, .velocity = 0x2800, .accel = 0},
                },
        },

    [3] =
        {
            .flags = 0x3E,
            .initOffset = 1,
            .InitTypes.AxisInit =
                {
                    .baseValue = {.startVal = 0, .velocity = 0x4C00, .accel = -0xDAC},
                    .rngSeed = {.startVal = 0x1000, .velocity = 0x4000, .accel = -0xC8},
                },
        },

    [4] =
        {
            .flags = 0xB,
            .initOffset = 5,
            .InitTypes.AxisInit =
                {
                    .baseValue = {.startVal = 0x3E8, .velocity = 0x28, .accel = 0},
                    .rngSeed = {.startVal = 0x64},
                },
        },

    [5] =
        {
            .flags = 3,
            .initOffset = 7,
            .InitTypes.AxisInit =
                {
                    .baseValue = {.startVal = 0x8000, .velocity = -0x700, .accel = 0},
                },
        },

    [6] = {0},
};

static void RB_Orca_SpawnSplash(struct Instance *orcaInst)
{
	struct Particle *particle;
	struct GameTracker *gGT;
	int i;

	gGT = P32_GET(struct GameTracker *, sdata->gGT);

	PARTICLE_SPAWN_UNGATED_BEGIN();
	for (i = 0; i < 0xF; i++)
	{
		particle = Particle_Init(0, P32_GET(struct IconGroup *, gGT->iconGroup[1]), &emSet_OrcaSplash[0]);

		if (particle == 0)
		{
			continue;
		}

		particle->axis[0].startVal += (orcaInst->matrix.t[0] << 8) + (particle->axis[0].velocity << 4);
		particle->axis[1].startVal += (orcaInst->matrix.t[1] << 8) + (particle->axis[1].velocity << 1);
		particle->renderDepthLimit = 0x1000;
		particle->axis[2].startVal += (orcaInst->matrix.t[2] << 8) + (particle->axis[2].velocity << 4);
	}
	PARTICLE_SPAWN_UNGATED_END();
}

void RB_Orca_ThTick(struct Thread *t)
{
	struct Orca *orcaObj;
	struct Instance *orcaInst;
	struct GameTracker *gGT;
	int numFrames;
	int pathFrame;
	int denominator;
	int nextFrame;
	s16 direction;

	orcaObj = (struct Orca *)P32_GET(void *, t->object);
	orcaInst = P32_GET(struct Instance *, t->inst);

	if (orcaObj->cooldown != 0)
	{
		// cooldown is in 30 FPS frames
		if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
		{
			orcaObj->cooldown--;
		}

		if ((u16)orcaObj->cooldown != 0)
		{
			return;
		}

		orcaInst->flags &= ~HIDE_MODEL;
		return;
	}

	numFrames = orcaObj->numFrames;
	pathFrame = orcaObj->animIndex;

	// animFrame and animIndex count in frame-rate-scaled animation frames,
	// so scale the 30 FPS path offsets to match
	if ((numFrames - INSTANCE_ScaleAnimFrames(orcaInst, 0, 0x14)) < pathFrame)
	{
		pathFrame = numFrames - INSTANCE_ScaleAnimFrames(orcaInst, 0, 0x14);
	}

	direction = orcaObj->direction;
	denominator = numFrames - INSTANCE_ScaleAnimFrames(orcaInst, 0, 0x17);

	if (direction == 0)
	{
		if ((numFrames - INSTANCE_ScaleAnimFrames(orcaInst, 0, 0x1A)) < (s16)pathFrame)
		{
			pathFrame = numFrames - INSTANCE_ScaleAnimFrames(orcaInst, 0, 0x1A);
		}
	}
	else
	{
		pathFrame -= INSTANCE_ScaleAnimFrames(orcaInst, 0, 3);
	}

	if ((s16)pathFrame < 0)
	{
		pathFrame = 0;
	}

	orcaInst->matrix.t[0] = orcaObj->startPos.x - ((pathFrame * orcaObj->pathDelta.x) / denominator);
	orcaInst->matrix.t[1] = orcaObj->startPos.y - ((pathFrame * orcaObj->pathDelta.y) / denominator);
	orcaInst->matrix.t[2] = orcaObj->startPos.z - ((pathFrame * orcaObj->pathDelta.z) / denominator);

	nextFrame = orcaInst->animFrame + 1;

	if (nextFrame < orcaObj->numFrames)
	{
		gGT = P32_GET(struct GameTracker *, sdata->gGT);

		if ((gGT->numPlyrCurrGame < 2) && ((nextFrame == INSTANCE_ScaleAnimFrames(orcaInst, 0, 5)) || (nextFrame == INSTANCE_ScaleAnimFrames(orcaInst, 0, 0x31))))
		{
			RB_Orca_SpawnSplash(orcaInst);
		}

		orcaInst->animFrame = nextFrame;

		if (direction == 0)
		{
			orcaObj->animIndex--;
		}
		else
		{
			orcaObj->animIndex++;
		}

		return;
	}

	orcaInst->flags |= HIDE_MODEL;
	orcaObj->cooldown = 0x5A;
	orcaInst->animFrame = 0;
	orcaObj->direction = direction ^ 1;
	orcaObj->instDefRot.y += 0x800;
	ConvertRotToMatrix(&orcaInst->matrix, &orcaObj->instDefRot);
}

int RB_Orca_ThCollide(struct Thread *orcaThread, struct Thread *driverTh, void *funcThCollide, struct ScratchpadStruct *sps)
{
	(void)orcaThread;
	(void)driverTh;
	(void)funcThCollide;

	return sps->Input1.modelID == DYNAMIC_PLAYER;
}

void RB_Orca_LInB(struct Instance *inst)
{
	struct Orca *orcaObj;
	struct SpawnType2 *spawnType2;
	struct Thread *t;
	void **pointers;
	s16 *metaArray;
	int orcaID;

	if (P32_GET(struct Thread *, inst->thread) != 0)
	{
		return;
	}

	t = PROC_BirthWithObject(
	    // creation flags
	    SIZE_RELATIVE_POOL_BUCKET(sizeof(struct Orca), NONE, SMALL, STATIC),

	    RB_Orca_ThTick, // behavior
	    "orca",         // debug name
	    0               // thread relative
	);

	if (t == 0)
	{
		return;
	}

	P32_SET(inst->thread, t);
	P32_SET(t->funcThCollide, (void *)RB_Orca_ThCollide);
	P32_SET(t->inst, inst);

	inst->scale.x = 0xC00;
	inst->scale.y = 0xC00;
	inst->scale.z = 0xC00;
	inst->flags |= DRAW_HUGE;

	orcaObj = (struct Orca *)P32_GET(void *, t->object);
	orcaObj->animIndex = INSTANCE_ScaleAnimFrames(inst, 0, -10);
	orcaObj->direction = 1;
	orcaObj->instDefRot.x = P32_GET(struct InstDef *, inst->instDef)->rot.x;
	orcaObj->instDefRot.y = P32_GET(struct InstDef *, inst->instDef)->rot.y;
	orcaObj->instDefRot.z = P32_GET(struct InstDef *, inst->instDef)->rot.z;

	orcaID = inst->name[strlen(inst->name) - 1] - '0';
	orcaObj->orcaID = orcaID;

	if (P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->numSpawnType2 != 0)
	{
		spawnType2 = &P32_GET(struct SpawnType2 *, P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->ptrSpawnType2)[orcaID + 4];

		orcaObj->startPos = P32_GET(SVec3 *, spawnType2->positions)[0];
		orcaObj->endPos = P32_GET(SVec3 *, spawnType2->positions)[1];
	}

	orcaObj->pathDelta.x = orcaObj->startPos.x - orcaObj->endPos.x;
	orcaObj->pathDelta.y = orcaObj->startPos.y - orcaObj->endPos.y;
	orcaObj->pathDelta.z = orcaObj->startPos.z - orcaObj->endPos.z;

	orcaObj->numFrames = INSTANCE_GetNumAnimFrames(inst, 0);

	if (P32_GET(struct SpawnType1 *, P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->ptrSpawnType1)->count <= 0)
	{
		return;
	}

	pointers = ST1_GETPOINTERS(P32_GET(struct SpawnType1 *, P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->ptrSpawnType1));
	metaArray = (s16 *)pointers[ST1_SPAWN];
	orcaObj->cooldown = metaArray[orcaObj->orcaID];

	if (orcaObj->cooldown != 0)
	{
		inst->flags |= HIDE_MODEL;
	}
}
