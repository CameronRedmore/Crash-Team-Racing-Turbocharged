#include <common.h>

// NOTE(aalhendi): ASM-audited NTSC-U 926 0x80032ffc-0x80033108.
// packID will always be 3-gGT->activeMempackIndex
void LOAD_Hub_ReadFile(struct BigHeader *bigfile, int levID, int packID)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	// if level is already loaded, quit
	if (gGT->levID_in_each_mempack[packID] == levID)
	{
		return;
	}

	P32_SET(sdata->modelMaskHints3D, 0);

	// Swap to pack of hub you're NOT on,
	// wipe the pack to reload the new hub
	MEMPACK_SwapPacks(packID);
	MEMPACK_ClearLowMem();

	sdata->load_inProgress = 1;
	P32_SET(gGT->level2, 0);
	gGT->levID_in_each_mempack[packID] = levID;

	LOAD_AppendQueue(bigfile, LT_VRAM, LOAD_GetBigfileIndex(levID, LOAD_LEVEL_LOD_1P, LVI_VRAM), NULL, NULL);
	LOAD_AppendQueue(bigfile, LT_GETADDR, LOAD_GetBigfileIndex(levID, LOAD_LEVEL_LOD_1P, LVI_LEV), NULL, LOAD_Callback_LEV);
	LOAD_AppendQueue(bigfile, LT_SETADDR, LOAD_GetBigfileIndex(levID, LOAD_LEVEL_LOD_1P, LVI_PTR), P32_GET(void *, sdata->PatchMem_Ptr), LOAD_HubCallback);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80033108-0x80033318.
void LOAD_Hub_SwapNow()
{
	struct Level *level1;
	struct VisMem *visMem;
	struct CameraDC *cDC;
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	// stall until load is done
	while (P32_GET(struct Level *, gGT->level2) == 0)
	{
		LOAD_NextQueuedFile();
		VSync(0);
	}

	// Aug 5
	// ptrintf("gGT->level2 = 0x%08x\n",gGT->level2);
	// ptrintf("SWAPPING 1...\n");

	LevInstDef_RePack(P32_GET(struct mesh_info *, P32_GET(struct Level *, gGT->level1)->ptr_mesh_info), 1);

	// Aug 5
	// ptrintf("SWAPPING 2...\n");

	LOAD_HubSwapPtrs(gGT);

	// 0,1,2
	gGT->activeMempackIndex = LOAD_HUB_MEMPACK_PAIR_INDEX_SUM - gGT->activeMempackIndex;

	gGT->prevLEV = gGT->levelID;
	gGT->levelID = gGT->levID_in_each_mempack[gGT->activeMempackIndex];

	Audio_AdvHub_SwapSong(gGT->levelID);

	// Aug 5
	// ptrintf("SWAPPING 3...\n");

	LibraryOfModels_Clear(gGT);

	/*
	In Aug 5
	if (sdata->PLYROBJECTLIST == 0)
	{
	    printf("ERROR: No PLYROBJECTLIST!\n");
	}
	*/

	if (P32_GET(P32(int *) *, sdata->PLYROBJECTLIST) != 0)
	{
		LOAD_GlobalModelPtrs_MPK();
	}

	level1 = P32_GET(struct Level *, gGT->level1);

	/*
	In Aug 5
	if (level1 == 0)
	{
	    printf("ERROR: No LEVEL!\n");
	}
	*/

	if (level1 != 0)
	{
		LibraryOfModels_Store(gGT, level1->numModels, P32_GET(P32(struct Model *) *, level1->ptrModelsPtrArray));

		INSTANCE_LevInitAll(P32_GET(struct InstDef *, level1->ptrInstDefs), level1->numInstances);

		LevInstDef_UnPack(P32_GET(struct mesh_info *, level1->ptr_mesh_info));

		DecalGlobal_Store(gGT, P32_GET(struct LevTexLookup *, level1->levTexLookup));
	}

	MEMPACK_SwapPacks(gGT->activeMempackIndex);
	MainInit_VisMem(gGT);

	cDC = &gGT->cameraDC[0];
	P32_SET(cDC->ptrQuadBlock, 0);
	P32_SET(cDC->visLeafSrc, 0);
	P32_SET(cDC->visFaceSrc, 0);
	P32_SET(cDC->visInstSrc, 0);
	P32_SET(cDC->visOVertSrc, 0);
	P32_SET(cDC->visSCVertSrc, 0);

	visMem = P32_GET(struct VisMem *, gGT->visMem1);
	P32_SET(visMem->visLeafSrc[0], 0);
	P32_SET(visMem->visFaceSrc[0], 0);
	P32_SET(visMem->visOVertSrc[0], 0);
	P32_SET(visMem->visSCVertSrc[0], 0);

	P32_SET(P32_GET(struct Driver *, gGT->drivers[0])->underDriver, 0);

	gGT->framesInThisLEV = 0;
	gGT->msInThisLEV = 0;
}

// NOTE(aalhendi): Native mirrors retail rdata 0x80011180 because CTR_NATIVE
// does not expose the retail rdata object.
#if defined(CTR_NATIVE)
static const int s_advHubConnectedLevID[LOAD_ADV_HUB_COUNT][LOAD_ADV_HUB_CONNECTION_COUNT] = {
    {N_SANITY_BEACH, THE_LOST_RUINS, -1},
    {GEM_STONE_VALLEY, GLACIER_PARK, -1},
    {GEM_STONE_VALLEY, GLACIER_PARK, -1},
    {N_SANITY_BEACH, THE_LOST_RUINS, CITADEL_CITY},
    {GLACIER_PARK, -1, -1},
};
#define LOAD_HUB_CONNECTED_LEV(hub, index) s_advHubConnectedLevID[(hub)][(index)]
#else
#define LOAD_HUB_CONNECTED_LEV(hub, index) rdata.MetaDataHubs[(hub)].connectedHub_LevID[(index)]
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80033318-0x80033474.
void LOAD_Hub_Main(struct BigHeader *bigfilePtr)
{
	struct GameTracker *gGT;

	// quit if already loading
	if (sdata->Loading.stage != LOAD_IDLE)
	{
		return;
	}

	gGT = P32_GET(struct GameTracker *, sdata->gGT);

	for (int i = 0; i < gGT->numPlyrCurrGame; i++)
	{
		CollStepFlags stepFlagSet = P32_GET(struct Driver *, gGT->drivers[i])->stepFlagSet;
		int nextLevelID = (stepFlagSet & COLL_STEP_TRIGGER_HUB_LEVEL_ID_MASK) >> COLL_STEP_TRIGGER_HUB_LEVEL_ID_SHIFT;
		int needSwapNow = (stepFlagSet & COLL_STEP_TRIGGER_HUB_SWAP_NOW_MASK) >> COLL_STEP_TRIGGER_HUB_SWAP_NOW_SHIFT;

		// if new level does not need to load
		if (nextLevelID == LOAD_HUB_TRIGGER_NONE)
		{
			if ((needSwapNow != 0) || (gGT->bool_AdvHub_NeedToSwapLEV != 0))
			{
				gGT->bool_AdvHub_NeedToSwapLEV = 0;
				LOAD_Hub_SwapNow();
			}
		}

		// if new level needs to load
		else
		{
			// only in AdvHub, or else the game
			// crashes in 4P Nitro Court Life Limit
			u32 currLevelID = gGT->levelID - GEM_STONE_VALLEY;

			// ctr hubs are 0-4
			if (currLevelID < LOAD_ADV_HUB_COUNT)
			{
				LOAD_Hub_ReadFile(bigfilePtr, LOAD_HUB_CONNECTED_LEV(currLevelID, nextLevelID - LOAD_HUB_TRIGGER_ID_BIAS),
				                  LOAD_HUB_MEMPACK_PAIR_INDEX_SUM - gGT->activeMempackIndex);
			}
		}
	}
}
