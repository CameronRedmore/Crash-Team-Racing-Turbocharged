#include <common.h>

#ifdef CTR_NATIVE
#include <platform/native_cd.h>
#include <platform/native_custom_racer.h>
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80032d30-0x80032d8c.
void LOAD_AppendQueue(struct BigHeader *bigfile, int type, int fileIndex, void *destinationPtr, void (*callback)(struct LoadQueueSlot *))
{
	if (sdata->queueLength >= LOAD_QUEUE_SLOT_COUNT)
	{
	#ifdef CTR_NATIVE
		fprintf(stderr, "[CTR Native] Load queue overflow: type=%d file=%d length=%d\n",
		        type, fileIndex, (int)sdata->queueLength);
	#endif
		return;
	}

	struct LoadQueueSlot *lqs = &sdata->queueSlots[(s32)sdata->queueLength];
	P32_SET(lqs->ptrBigfileCdPos_UNUSED, bigfile);
	lqs->flags = 0;
	lqs->type_UNUSED = type;
	lqs->subfileIndex = fileIndex;
	P32_SET(lqs->ptrDestination, destinationPtr);
	lqs->size_UNUSED = 0;
	P32_SET(lqs->callbackFuncPtr, callback);

	sdata->queueLength++;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80032d8c-0x80032dc0.
void LOAD_CDRequestCallback(struct LoadQueueSlot *lqs)
{
	if (P32_GET(void (*)(struct LoadQueueSlot *), lqs->callbackFuncPtr) != NULL)
	{
		P32_GET(void (*)(struct LoadQueueSlot *), lqs->callbackFuncPtr)(lqs);
	}

	sdata->queueReady = 1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 PS1 path 0x80032dc0-0x80032ffc.
void LOAD_NextQueuedFile()
{
#ifdef CTR_NATIVE
	NativeCD_PumpCallbacks();
#endif

	struct LoadQueueSlot *curr = &data.currSlot;

	if ((sdata->queueReady != 0) && (sdata->XA_State == 0) && (sdata->queueLength != 0))
	{
		sdata->queueReady = 0;

		if (sdata->queueRetry != 0)
		{
			sdata->queueRetry = 0;
		}
		else
		{
			*curr = sdata->queueSlots[0];

			for (int i = LOAD_QUEUE_FIRST_PENDING_SLOT; i < sdata->queueLength; i++)
			{
				sdata->queueSlots[i - 1] = sdata->queueSlots[i];
			}
		}

	#ifdef CTR_NATIVE
		if (NativeCustomRacer_IsBigHeader(P32_GET(struct BigHeader *, curr->ptrBigfileCdPos_UNUSED)))
		{
			NativeCustomRacer_LoadQueueSlot(curr);
		}
		else
	#endif
		switch (curr->type_UNUSED)
		{
		case LT_RAW:
			P32_SET(curr->ptrDestination, LOAD_ReadFile_ex(P32_GET(struct BigHeader *, curr->ptrBigfileCdPos_UNUSED), LT_SETADDR, curr->subfileIndex, P32_GET(void *, curr->ptrDestination), &curr->size_UNUSED,
			                                        LOAD_CDRequestCallback));
			break;

		case LT_DRAM:
			P32_SET(curr->ptrDestination, LOAD_DramFile(P32_GET(struct BigHeader *, curr->ptrBigfileCdPos_UNUSED), curr->subfileIndex, P32_GET(void *, curr->ptrDestination), &curr->size_UNUSED, (int)(intptr_t)P32_GET(void (*)(struct LoadQueueSlot *), curr->callbackFuncPtr)));
			break;

		case LT_VRAM:
			P32_SET(curr->ptrDestination, LOAD_VramFile(P32_GET(struct BigHeader *, curr->ptrBigfileCdPos_UNUSED), curr->subfileIndex, P32_GET(void *, curr->ptrDestination), &curr->size_UNUSED, (int)(intptr_t)P32_GET(void (*)(struct LoadQueueSlot *), curr->callbackFuncPtr)));
			break;
		}

		sdata->queueLength--;
	}

	if (sdata->frameFinishedVRAM != 0)
	{
		if ((u32)(P32_GET(struct GameTracker *, sdata->gGT)->frameTimer_VsyncCallback - sdata->frameFinishedVRAM) >= LOAD_QUEUE_VRAM_CALLBACK_DELAY_FRAMES)
		{
			if (P32_GET(void (*)(struct LoadQueueSlot *), curr->callbackFuncPtr) != NULL)
			{
				P32_GET(void (*)(struct LoadQueueSlot *), curr->callbackFuncPtr)(curr);
			}

			sdata->frameFinishedVRAM = 0;

#if defined(CTR_NATIVE)
			NativeCustomRacer_FinishQueueSlot(curr);
			// NOTE(aalhendi): CTR_NATIVE marks Mempack allocations with a host-only
			// flag while the retail path uses LT_SETADDR for the same ownership.
			if ((curr->flags & (LT_SETADDR | LT_MEMPACK)) != 0)
#else
			if ((curr->flags & LT_SETADDR) != 0)
#endif
			{
				MEMPACK_PopState();
			}

			sdata->queueReady = 1;
		}
	}
}
