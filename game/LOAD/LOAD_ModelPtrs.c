#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031b50-0x80031bdc.
void LOAD_GlobalModelPtrs_MPK()
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	for (int i = 0; i < LOAD_DRIVER_MODEL_EXTRA_COUNT; i++)
	{
		struct Model *m = P32_GET(struct Model *, data.driverModelExtras[i].model);

		if (m == NULL)
		{
			continue;
		}

		if (m->id == -1)
		{
			continue;
		}

		P32_SET(gGT->modelPtr[m->id], m);
	}

	if (P32_GET(P32(int *) *, sdata->PLYROBJECTLIST) != 0)
	{
		LibraryOfModels_Store(gGT, -1, (P32(struct Model *) *)P32_GET(P32(int *) *, sdata->PLYROBJECTLIST));
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031bdc-0x80031c1c.
void LOAD_HubSwapPtrs(struct GameTracker *gGT)
{
	struct Level *oldLev1;
	struct VisMem *oldVisMem1;
	struct VisMem *oldVisMem2;

	// if no secondary lev exists, quit
	if (P32_GET(struct Level *, gGT->level2) == 0)
	{
		return;
	}

	oldLev1 = P32_GET(struct Level *, gGT->level1);
	oldVisMem1 = P32_GET(struct VisMem *, gGT->visMem1);
	oldVisMem2 = P32_GET(struct VisMem *, gGT->visMem2);

	P32_SET(gGT->level1, P32_GET(struct Level *, gGT->level2));
	gGT->boolHubSwapped = 1;

	P32_SET(gGT->level2, oldLev1);
	P32_SET(gGT->visMem1, oldVisMem2);
	P32_SET(gGT->visMem2, oldVisMem1);
}
