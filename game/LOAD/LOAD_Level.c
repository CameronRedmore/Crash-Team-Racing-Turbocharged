#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800347d0-0x80034874.
// Dont get confused, packID is LOAD_GetAdvPackIndex(),
// which gives the pack of the hub you're NOT on, because the
// game does 3-GetAdvPackIndex to load the hub you ARE on

#if defined(CTR_NATIVE)
// NOTE: The mask loads into the hub pack you are NOT on, which is the pack a hub
// preload targets. Native CD is fast enough that both can land together, so a request that
// finds the queue busy is parked and retried by the mask hint while it waits for the model.
static struct
{
	b32 pending;
	int packID;
	int maskID;
} s_pendingTalkingMask;

static void LOAD_TalkingMask_Now(int packID, int maskID);

void LOAD_TalkingMask(int packID, int maskID)
{
	if (LOAD_NativeQueueBusy())
	{
		s_pendingTalkingMask.pending = 1;
		s_pendingTalkingMask.packID = packID;
		s_pendingTalkingMask.maskID = maskID;
		return;
	}

	s_pendingTalkingMask.pending = 0;
	LOAD_TalkingMask_Now(packID, maskID);
}

void LOAD_TalkingMask_Retry(void)
{
	if (s_pendingTalkingMask.pending && !LOAD_NativeQueueBusy())
	{
		s_pendingTalkingMask.pending = 0;
		LOAD_TalkingMask_Now(s_pendingTalkingMask.packID, s_pendingTalkingMask.maskID);
	}
}

static void LOAD_TalkingMask_Now(int packID, int maskID)
#else
void LOAD_TalkingMask(int packID, int maskID)
#endif
{
	sdata->modelMaskHints3D = 0;

#if defined(CTR_NATIVE)
	// NOTE: If the hub preloaded into this pack is still waiting to be swapped in, the
	// wipe below destroys it while gGT->level2 points at it. Drop level2 and let SwapNow reload it.
	{
		struct GameTracker *gGT = sdata->gGT;
		const struct Mempack *pack = &sdata->mempack[packID];

		if ((gGT->level2 != NULL) && ((u32)gGT->level2 >= (u32)pack->start) && ((u32)gGT->level2 < (u32)pack->endOfMemory))
		{
			LOAD_Hub_NoteClobbered(packID, gGT->levID_in_each_mempack[packID]);
			gGT->level2 = NULL;
		}
	}
#endif

	// invalidate alternative-hub, because
	// the mask will load in that level's RAM
	sdata->gGT->levID_in_each_mempack[packID] = -1;

	// Swap to pack of hub you're NOT on,
	// wipe the pack to reload the new MASK
	MEMPACK_SwapPacks(packID);
	MEMPACK_ClearLowMem();

	sdata->load_inProgress = 1;

	int offset = maskID * 4 + (packID - 1) * 2;

	// NOTE(aalhendi): Retail queues legacy VRAM type 3 with no final callback.
	LOAD_AppendQueue(sdata->ptrBigfileCdPos_2, LT_VRAM, BI_UKAHEAD + offset, NULL, NULL);

	LOAD_AppendQueue(sdata->ptrBigfileCdPos_2, LT_GETADDR, BI_UKAHEAD + offset + 1, NULL, LOAD_Callback_MaskHints3D);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034874-0x800348e8.
void LOAD_LevelFile(int levelID)
{
	struct GameTracker *gGT = sdata->gGT;

	// why here?
	sdata->modelMaskHints3D = 0;

	gGT->hudFlags &= HUD_FLAG_CLEAR_RACE_HUD_MASK;

	gGT->prevLEV = gGT->levelID;
	gGT->levelID = levelID;

	// disable all rendering except checkeredFlag
	gGT->renderFlags &= RENDER_FLAG_CHECKERED_FLAG;

	if (RaceFlag_IsFullyOffScreen())
	{
		RaceFlag_BeginTransition(1);
	}

	// start loading
	sdata->Loading.stage = LOAD_TEN_STAGES_0;
}
