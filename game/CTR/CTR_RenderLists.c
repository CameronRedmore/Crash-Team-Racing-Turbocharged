#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021bbc-0x80021c2c
void CTR_ClearRenderLists_1P2P(struct GameTracker *gGT, int numPlyrCurrGame)
{
	if (numPlyrCurrGame <= 0)
	{
		return;
	}

	for (int i = 0; i < numPlyrCurrGame; i++)
	{
		void *quadBlocksRendered = P32_GET(void *, data.ptrRenderedQuadblockDestination_forEachPlayer[i]);

		for (int listIndex = 0; listIndex < 5; listIndex++)
		{
			P32_SET(gGT->LevRenderLists[i].list[listIndex].bspListStart, 0);
			P32_SET(gGT->LevRenderLists[i].list[listIndex].ptrQuadBlocksRendered, quadBlocksRendered);
		}

		P32_SET(gGT->LevRenderLists[i].bspListStart_FullDynamic, 0);
		P32_SET(gGT->LevRenderLists[i].ptrQuadBlocksRendered_FullDynamic, 0);
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021c2c-0x80021c8c
void CTR_ClearRenderLists_3P4P(struct GameTracker *gGT, int numPlyrCurrGame)
{
	if (numPlyrCurrGame <= 0)
	{
		return;
	}

	for (int i = 0; i < numPlyrCurrGame; i++)
	{
		void *quadBlocksRendered = P32_GET(void *, data.ptrRenderedQuadblockDestination_again[i]);

		for (int listIndex = 0; listIndex < 4; listIndex++)
		{
			P32_SET(gGT->LevRenderLists[i].list[listIndex].bspListStart, 0);
			P32_SET(gGT->LevRenderLists[i].list[listIndex].ptrQuadBlocksRendered, quadBlocksRendered);
		}

		P32_SET(gGT->LevRenderLists[i].list[4].ptrQuadBlocksRendered, 0);
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021c8c-0x80021c94.
void CTR_EmptyFunc_MainFrame_ResetDB(void)
{
}
