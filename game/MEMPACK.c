#include <common.h>


// NOTE(aalhendi): ASM-verified NTSC-U 926 PS1 path 0x8003e740-0x8003e80c; CTR_NATIVE uses host RAM.
void MEMPACK_Init(int ramSize)
{
	(void)ramSize;
	u32 startPtr;
	s32 packSize;

#if defined(CTR_NATIVE)

	const struct PlatformMempackArena *arena = Platform_InitMempackArena();

	startPtr = (u32)arena->start;
	packSize = arena->size;

	printf("[CTR] MEMPACK native backing: base=%08x\n", (u32)arena->base);

	MEMPACK_NewPack((void *)startPtr, packSize);
	P32_SET(P32_GET(struct Mempack *, sdata->PtrMempack)->endOfAllocator, (void *)(startPtr + packSize));
	P32_SET(P32_GET(struct Mempack *, sdata->PtrMempack)->endOfMemory, arena->endOfMemory);

	printf("[CTR] MEMPACK native arena: start=%08x size=%08x end=%08x\n", startPtr, packSize, (u32)P32_GET(void *, P32_GET(struct Mempack *, sdata->PtrMempack)->endOfAllocator));

#else

	maxOverlayEnd = (u32)AH_EndOfFile;
	if (maxOverlayEnd < (u32)RB_EndOfFile)
		maxOverlayEnd = (u32)RB_EndOfFile;
	if (maxOverlayEnd < (u32)MM_EndOfFile)
		maxOverlayEnd = (u32)MM_EndOfFile;
	if (maxOverlayEnd < (u32)CS_EndOfFile)
		maxOverlayEnd = (u32)CS_EndOfFile;

	startPtr = (u32)OVR_Region3 + (((maxOverlayEnd - (u32)OVR_Region3) + MEMPACK_PS1_OVERLAY_ALIGNMENT_MASK) & ~MEMPACK_PS1_OVERLAY_ALIGNMENT_MASK);
	packSize = ramSize - (int)(startPtr & MEMPACK_PS1_RAM_ADDRESS_MASK) - MEMPACK_PS1_END_GUARD_SIZE;

	ptrMempack = sdata->PtrMempack;
	ptrMempack->start = (void *)startPtr;
	ptrMempack->endOfAllocator = (void *)(startPtr + packSize);
	ptrMempack->lastFreeByte = (void *)(startPtr + packSize);
	ptrMempack->packSize = packSize;
	ptrMempack->numBookmarks = 0;
	ptrMempack->endOfMemory = (void *)MEMPACK_PS1_END_OF_MEMORY;
	ptrMempack->firstFreeByte = (void *)startPtr;
#endif
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e80c-0x8003e830.
void MEMPACK_SwapPacks(int index)
{
	P32_SET(sdata->PtrMempack, &sdata->mempack[index]);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e830-0x8003e85c.
void MEMPACK_NewPack(void *start, int size)
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);
	void *end = (void *)((u32)start + size);

	ptrMempack->packSize = size;
	P32_SET(ptrMempack->start, start);
	P32_SET(ptrMempack->lastFreeByte, end);
	P32_SET(ptrMempack->endOfMemory, end);
	P32_SET(ptrMempack->firstFreeByte, start);
	ptrMempack->numBookmarks = 0;
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e85c-0x8003e874.
int MEMPACK_GetFreeBytes()
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);

	return (u32)P32_GET(void *, ptrMempack->lastFreeByte) - (u32)P32_GET(void *, ptrMempack->firstFreeByte);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e874-0x8003e8e8.
void *MEMPACK_AllocMem(int allocSize)
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);

	if (MEMPACK_GetFreeBytes() < allocSize)
	{
		CTR_ErrorScreen(0xFF, 0, 0);
		for (;;)
		{
		}
	}

	s32 newAllocSize = MEMPACK_ALIGN_SIZE(allocSize);
	ptrMempack->sizeOfPrevAllocation = newAllocSize;

	s32 firstFreeByte = (s32)P32_GET(void *, ptrMempack->firstFreeByte);
	P32_SET(ptrMempack->firstFreeByte, (void *)(firstFreeByte + newAllocSize));

	return (void *)firstFreeByte;
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e8e8-0x8003e938.
void *MEMPACK_AllocHighMem(int allocSize)
{
	while (MEMPACK_GetFreeBytes() < allocSize)
	{
	}

	allocSize = MEMPACK_ALIGN_SIZE(allocSize);
	P32_GET(struct Mempack *, sdata->PtrMempack)->sizeOfPrevAllocation = allocSize;

	s32 newLastFreeByte = (s32)P32_GET(void *, P32_GET(struct Mempack *, sdata->PtrMempack)->lastFreeByte) - allocSize;
	P32_SET(P32_GET(struct Mempack *, sdata->PtrMempack)->lastFreeByte, (void *)newLastFreeByte);

	return (void *)newLastFreeByte;
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e938-0x8003e94c.
void MEMPACK_ClearHighMem()
{
	P32_SET(P32_GET(struct Mempack *, sdata->PtrMempack)->lastFreeByte, P32_GET(void *, P32_GET(struct Mempack *, sdata->PtrMempack)->endOfAllocator));
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e94c-0x8003e978.
void *MEMPACK_ReallocMem(int allocSize)
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);

	s32 newAllocSize = MEMPACK_ALIGN_SIZE(allocSize);
	P32_SET(ptrMempack->firstFreeByte, (void *)((s32)P32_GET(void *, ptrMempack->firstFreeByte) - ptrMempack->sizeOfPrevAllocation + newAllocSize));
	ptrMempack->sizeOfPrevAllocation = newAllocSize;

	return P32_GET(void *, ptrMempack->firstFreeByte);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e978-0x8003e9b8.
int MEMPACK_PushState()
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);
	s32 numBookmarks = ptrMempack->numBookmarks;
	if (numBookmarks < MEMPACK_BOOKMARK_COUNT)
	{
		P32_SET(ptrMempack->bookmarks[numBookmarks], P32_GET(void *, ptrMempack->firstFreeByte));
		ptrMempack->numBookmarks++;
	}

	return numBookmarks;
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e9b8-0x8003e9d0.
void MEMPACK_ClearLowMem()
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);

	ptrMempack->numBookmarks = 0;
	P32_SET(ptrMempack->firstFreeByte, P32_GET(void *, ptrMempack->start));
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e9d0-0x8003ea08.
void MEMPACK_PopState()
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);
	s32 numBookmarks = ptrMempack->numBookmarks;
	if (numBookmarks > 0)
	{
		numBookmarks--;
		P32_SET(ptrMempack->firstFreeByte, P32_GET(void *, ptrMempack->bookmarks[numBookmarks]));
		ptrMempack->numBookmarks = numBookmarks;
	}
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003ea08-0x8003ea28.
void MEMPACK_PopToState(int id)
{
	struct Mempack *ptrMempack = P32_GET(struct Mempack *, sdata->PtrMempack);

	ptrMempack->numBookmarks = id;
	P32_SET(ptrMempack->firstFreeByte, P32_GET(void *, ptrMempack->bookmarks[id]));
}
