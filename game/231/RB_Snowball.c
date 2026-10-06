#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b92ac-0x800b95fc.

void RB_Snowball_ThTick(struct Thread *t)
{
	struct Instance *snowInst;
	struct Snowball *snowObj;

	int modelID;
	int soundID;
	int pointIndex;
	struct SpawnType2 *ptrSpawnType2;
	const struct SpawnPosRot *frame;

	snowInst = P32_GET(struct Instance *, t->inst);
	snowObj = (struct Snowball *)P32_GET(void *, t->object);

	modelID = P32_GET(struct Model *, snowInst->model)->id;

	if (P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->numSpawnType2_PosRot != 0)
	{
		ptrSpawnType2 = &P32_GET(struct SpawnType2 *, P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->ptrSpawnType2_PosRot)[snowObj->snowID];

		// Retail checks DYNAMIC_SNOWBALL, but Blizzard Bluff uses TEMP_SNOWBALL.
		if (modelID == DYNAMIC_SNOWBALL)
		{
			// snowball roll
			soundID = 0x73;
			PlaySound3D_Flags(&snowObj->soundIDCount, soundID, snowInst);
		}

		// sewer speedway barrel
		else if (modelID == DYNAMIC_BARREL)
		{
			// barrel roll
			soundID = 0x74;
			PlaySound3D_Flags(&snowObj->soundIDCount, soundID, snowInst);
		}

		pointIndex = snowObj->pointIndex;
		if (pointIndex > snowObj->numPoints)
		{
			pointIndex = (snowObj->numPoints * 2) - pointIndex;
		}

		frame = &P32_GET(struct SpawnPosRot *, ptrSpawnType2->posRot)[pointIndex];

		SVec3 pos = frame->pos;
		SVec3 rot = frame->rot;

		if (CTR_NATIVE_60FPS_ACTIVE)
		{
			pointIndex = (snowObj->pointIndex + 1) % (snowObj->numPoints * 2);
			if (pointIndex > snowObj->numPoints)
			{
				pointIndex = (snowObj->numPoints * 2) - pointIndex;
			}

			const struct SpawnPosRot *nextFrame = &P32_GET(struct SpawnPosRot *, ptrSpawnType2->posRot)[pointIndex];
			int fraction = (int)(((u32)P32_GET(struct GameTracker *, sdata->gGT)->timer * 30ull) % CTR_FRAMES_PER_SECOND);
			for (int axis = 0; axis < 3; axis++)
			{
				pos.v[axis] = (s16)(((s32)pos.v[axis] * (CTR_FRAMES_PER_SECOND - fraction) + (s32)nextFrame->pos.v[axis] * fraction) / CTR_FRAMES_PER_SECOND);
				if (rot.z == nextFrame->rot.z)
				{
					rot.v[axis] = (s16)(((s32)rot.v[axis] * (CTR_FRAMES_PER_SECOND - fraction) + (s32)nextFrame->rot.v[axis] * fraction) / CTR_FRAMES_PER_SECOND);
				}
			}
		}

		ConvertRotToMatrix(&snowInst->matrix, &rot);

		snowInst->matrix.t[0] = pos.x;
		snowInst->matrix.t[1] = pos.y;
		snowInst->matrix.t[2] = pos.z;

		RB_Minecart_CheckColl(snowInst, t);
	}

	if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
	{
		snowObj->pointIndex = (snowObj->pointIndex + 1) % (snowObj->numPoints * 2);
	}
}

void RB_Snowball_LInB(struct Instance *inst)
{
	struct Snowball *snowObj;
	struct Thread *t;

	if (P32_GET(struct Thread *, inst->thread) != 0)
	{
		return;
	}

	t = PROC_BirthWithObject(
	    // creation flags
	    SIZE_RELATIVE_POOL_BUCKET(sizeof(struct Snowball), NONE, SMALL, STATIC),

	    RB_Snowball_ThTick, // behavior
	    "snowball",         // debug name
	    0                   // thread relative
	);

	if (t == 0)
	{
		return;
	}
	P32_SET(inst->thread, t);
	P32_SET(t->inst, inst);

	snowObj = ((struct Snowball *)P32_GET(void *, t->object));
	snowObj->pointIndex = 0;
	snowObj->rot_unused.x = 0;

	snowObj->snowID = inst->name[strlen(inst->name) - 1] - '0';

	snowObj->numPoints = P32_GET(struct SpawnType2 *, P32_GET(struct Level *, P32_GET(struct GameTracker *, sdata->gGT)->level1)->ptrSpawnType2_PosRot)[snowObj->snowID].numCoords - 1;

	inst->scale.x = 0x1000;
	inst->scale.y = 0x1000;
	inst->scale.z = 0x1000;

	snowObj->rot_unused.y = inst->matrix.m[0][2] >> 2;
	snowObj->rot_unused.z = inst->matrix.m[2][2] >> 2;
	snowObj->soundIDCount = 0;
}
