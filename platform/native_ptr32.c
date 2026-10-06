#include <ctr_ptr32.h>

#if defined(CTR_NATIVE_64BIT)
#include <stdio.h>
#include <stdlib.h>

// Holds the origin for CtrPtr32 handles (see ctr_ptr32.h). Nothing else may
// ever be allocated inside it.
CtrPtr32Anchor gCtrPtr32Anchor;

void CtrPtr32_RangeError(uintptr_t p)
{
	fprintf(stderr, "CtrPtr32: pointer %p is outside +-2 GiB of the image\n", (void *)p);
	abort();
}
#endif
