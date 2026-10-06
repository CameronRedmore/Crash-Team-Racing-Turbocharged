#ifndef PLATFORM_NATIVE_MEMORY_H
#define PLATFORM_NATIVE_MEMORY_H

#include <macros.h>

#include <stddef.h>

void Platform_ConfigureMempackArena(void);
void Platform_RepairResidentPointers(s32 activeMempackIndex);
void *Platform_GetMempackBacking(void);
int Platform_GetMempackBackingSize(void);

// Buffers that game structs (CtrPtr32 handles) may point at. See
// platform/native_image_heap.c.
void *Platform_ImageAlloc(size_t size);
void Platform_ImageFree(void *p);

#endif
