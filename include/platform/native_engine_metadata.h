#ifndef PLATFORM_NATIVE_ENGINE_METADATA_H
#define PLATFORM_NATIVE_ENGINE_METADATA_H

#include <stdint.h>
#include <string.h>
#include "native_engine.h"

#define NATIVE_ENGINE_METADATA_TAG      UINT32_C(0x4e455000)
#define NATIVE_ENGINE_METADATA_TAG_MASK UINT32_C(0xffffff00)
#define NATIVE_ENGINE_METADATA_INVALID  (-2)

/* Unrecognized padding is legacy data; a recognized but bad profile is corrupt. */
static inline int NativeEngineMetadata_DecodeWord(uint32_t word)
{
	if ((word & NATIVE_ENGINE_METADATA_TAG_MASK) != NATIVE_ENGINE_METADATA_TAG)
		return NATIVE_ENGINE_DEFAULT;
	int profile = (int)(word & 0xff);
	return profile < NATIVE_ENGINE_COUNT ? profile : NATIVE_ENGINE_METADATA_INVALID;
}

static inline uint32_t NativeEngineMetadata_EncodeWord(int profile)
{
	return profile >= 0 && profile < NATIVE_ENGINE_COUNT ? NATIVE_ENGINE_METADATA_TAG | (uint32_t)profile : 0;
}

static inline void NativeEngineMetadata_StoreRetail(char padding[20], int profile)
{
	memset(padding, 0, 20);
	if (profile < 0 || profile >= NATIVE_ENGINE_COUNT)
		return;
	padding[0] = 'N';
	padding[1] = 'E';
	padding[2] = 'P';
	padding[3] = 1;
	padding[4] = (char)profile;
}

static inline int NativeEngineMetadata_LoadRetail(const char padding[20])
{
	if (padding[0] != 'N' || padding[1] != 'E' || padding[2] != 'P' || padding[3] != 1)
		return NATIVE_ENGINE_DEFAULT;
	int profile = (unsigned char)padding[4];
	return profile < NATIVE_ENGINE_COUNT ? profile : NATIVE_ENGINE_METADATA_INVALID;
}

#endif
