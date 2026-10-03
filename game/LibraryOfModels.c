#include <common.h>

enum LibraryOfModelsConstants
{
	LIBRARY_OF_MODELS_CLEAR_COUNT = 0xe2,
};

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003147c-0x800314c0.
void LibraryOfModels_Store(struct GameTracker *gGT, u32 numModels, P32(struct Model *) *ptrModelArray)
{
	while (numModels != 0)
	{
		struct Model *m = P32_GET(struct Model *, *ptrModelArray);
		if (m == NULL)
		{
			return;
		}
		if (m->id != -1)
		{
			P32_SET(gGT->modelPtr[m->id], m);
		}
		numModels--;
		ptrModelArray++;
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800314c0-0x800314e0.
void LibraryOfModels_Clear(struct GameTracker *gGT)
{
	for (s32 i = 0; i < LIBRARY_OF_MODELS_CLEAR_COUNT; i++)
	{
		P32_SET(gGT->modelPtr[i], 0);
	}
}
