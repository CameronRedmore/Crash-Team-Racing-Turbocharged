#include <ctr_ptr32.h>

#include <stddef.h>
#include <stdlib.h>

// Heap for native buffers that game structs point at (custom racer models and
// VRMs, extra AI models). On 64-bit builds those pointers are CtrPtr32 handles,
// which can only name memory inside the executable image, so the heap is a
// first-fit free list over a static array. On 32-bit builds it is malloc.
// Main thread only.

#if defined(CTR_NATIVE_64BIT)
#include <stdint.h>
#include <stdio.h>

#ifndef NATIVE_IMAGE_HEAP_SIZE
#define NATIVE_IMAGE_HEAP_SIZE (32u * 1024u * 1024u)
#endif
#define NATIVE_IMAGE_HEAP_ALIGN 16u

// Blocks tile the heap. size includes the header and is a multiple of
// NATIVE_IMAGE_HEAP_ALIGN; prevSize is 0 for the first block.
struct NativeImageHeapBlock
{
	uint32_t size;
	uint32_t prevSize;
	uint32_t used;
	uint32_t magic;
};

#define NATIVE_IMAGE_HEAP_MAGIC 0x50414548u

static union
{
	unsigned char bytes[NATIVE_IMAGE_HEAP_SIZE];
	uint64_t align;
} s_nativeImageHeap;
static unsigned char *s_nativeImageHeapStart;
static unsigned char *s_nativeImageHeapEnd;

static void NativeImageHeap_Init(void)
{
	if (s_nativeImageHeapStart != NULL)
		return;
	uintptr_t base = (uintptr_t)&s_nativeImageHeap.bytes[0];
	uintptr_t start = (base + NATIVE_IMAGE_HEAP_ALIGN - 1u) & ~(uintptr_t)(NATIVE_IMAGE_HEAP_ALIGN - 1u);
	uintptr_t end = (base + NATIVE_IMAGE_HEAP_SIZE) & ~(uintptr_t)(NATIVE_IMAGE_HEAP_ALIGN - 1u);
	s_nativeImageHeapStart = (unsigned char *)start;
	s_nativeImageHeapEnd = (unsigned char *)end;

	struct NativeImageHeapBlock *block = (struct NativeImageHeapBlock *)s_nativeImageHeapStart;
	block->size = (uint32_t)(end - start);
	block->prevSize = 0;
	block->used = 0;
	block->magic = NATIVE_IMAGE_HEAP_MAGIC;
}

static struct NativeImageHeapBlock *NativeImageHeap_Next(struct NativeImageHeapBlock *block)
{
	unsigned char *next = (unsigned char *)block + block->size;
	return (next < s_nativeImageHeapEnd) ? (struct NativeImageHeapBlock *)next : NULL;
}

static void NativeImageHeap_Corrupt(const void *p)
{
	fprintf(stderr, "Platform_ImageFree: %p is not a live image heap block\n", p);
	abort();
}
#endif

void *Platform_ImageAlloc(size_t size)
{
#if defined(CTR_NATIVE_64BIT)
	if ((size == 0) || (size > NATIVE_IMAGE_HEAP_SIZE))
		return NULL;
	NativeImageHeap_Init();

	const uint32_t need = (uint32_t)((size + sizeof(struct NativeImageHeapBlock) + NATIVE_IMAGE_HEAP_ALIGN - 1u) &
	                                 ~(size_t)(NATIVE_IMAGE_HEAP_ALIGN - 1u));
	for (struct NativeImageHeapBlock *block = (struct NativeImageHeapBlock *)s_nativeImageHeapStart; block != NULL;
	     block = NativeImageHeap_Next(block))
	{
		if (block->used || (block->size < need))
			continue;
		if (block->size - need >= 2u * sizeof(struct NativeImageHeapBlock))
		{
			struct NativeImageHeapBlock *rest = (struct NativeImageHeapBlock *)((unsigned char *)block + need);
			rest->size = block->size - need;
			rest->prevSize = need;
			rest->used = 0;
			rest->magic = NATIVE_IMAGE_HEAP_MAGIC;
			struct NativeImageHeapBlock *after = NativeImageHeap_Next(rest);
			if (after != NULL)
				after->prevSize = rest->size;
			block->size = need;
		}
		block->used = 1;
		return block + 1;
	}
	fprintf(stderr, "[CTR Native] Image heap is out of memory (%zu bytes requested)\n", size);
	return NULL;
#else
	return (size != 0) ? malloc(size) : NULL;
#endif
}

void Platform_ImageFree(void *p)
{
#if defined(CTR_NATIVE_64BIT)
	if (p == NULL)
		return;
	struct NativeImageHeapBlock *block = (struct NativeImageHeapBlock *)p - 1;
	if (((unsigned char *)block < s_nativeImageHeapStart) || ((unsigned char *)block >= s_nativeImageHeapEnd) ||
	    (block->magic != NATIVE_IMAGE_HEAP_MAGIC) || !block->used)
	{
		NativeImageHeap_Corrupt(p);
	}
	block->used = 0;

	struct NativeImageHeapBlock *next = NativeImageHeap_Next(block);
	if ((next != NULL) && !next->used)
	{
		block->size += next->size;
		next->magic = 0;
	}
	if (block->prevSize != 0)
	{
		struct NativeImageHeapBlock *prev = (struct NativeImageHeapBlock *)((unsigned char *)block - block->prevSize);
		if (!prev->used)
		{
			prev->size += block->size;
			block->magic = 0;
			block = prev;
		}
	}
	next = NativeImageHeap_Next(block);
	if (next != NULL)
		next->prevSize = block->size;
#else
	free(p);
#endif
}
