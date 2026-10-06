#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../platform/native_ptr32.c"
#include "../platform/native_image_heap.c"

static int s_failures;

#define CHECK(cond)                                                       \
	do                                                                    \
	{                                                                     \
		if (!(cond))                                                      \
		{                                                                 \
			fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
			s_failures++;                                                 \
		}                                                                 \
	} while (0)

static void CheckBlock(void *p, size_t size)
{
	CHECK(p != NULL);
	if (p == NULL)
		return;
	CHECK(((uintptr_t)p & 7u) == 0);
	uint32_t h = 0;
	CHECK(P32_TRY_ENC(p, &h));
	CHECK(P32_TRY_ENC((unsigned char *)p + size - 1u, &h));
	memset(p, 0xa5, size);
}

int main(void)
{
	void *a = Platform_ImageAlloc(100);
	void *b = Platform_ImageAlloc(4096);
	void *c = Platform_ImageAlloc(1);
	void *d = Platform_ImageAlloc(300000);
	CheckBlock(a, 100);
	CheckBlock(b, 4096);
	CheckBlock(c, 1);
	CheckBlock(d, 300000);
	CHECK((a != b) && (b != c) && (c != d));
	CHECK(Platform_ImageAlloc(0) == NULL);

	// Free out of order so blocks merge with both neighbours.
	Platform_ImageFree(b);
	Platform_ImageFree(d);
	Platform_ImageFree(c);
	Platform_ImageFree(a);
	Platform_ImageFree(NULL);

#if defined(CTR_NATIVE_64BIT)
	// After everything is freed the heap is one block again, so nearly all of it
	// can be handed out at once, and a reused block lands where the first did.
	void *big = Platform_ImageAlloc(NATIVE_IMAGE_HEAP_SIZE - 4096u);
	CheckBlock(big, NATIVE_IMAGE_HEAP_SIZE - 4096u);
	CHECK(Platform_ImageAlloc(8192) == NULL);
	Platform_ImageFree(big);
	CHECK(Platform_ImageAlloc(64) == a);
	CHECK(Platform_ImageAlloc(NATIVE_IMAGE_HEAP_SIZE + 1u) == NULL);
#endif

	if (s_failures != 0)
	{
		fprintf(stderr, "%d image heap check(s) failed\n", s_failures);
		return 1;
	}
	printf("image heap ok\n");
	return 0;
}
