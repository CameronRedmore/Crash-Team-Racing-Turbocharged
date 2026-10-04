#ifndef NATIVE_PROJECTION_PACKET_H
#define NATIVE_PROJECTION_PACKET_H
#include <psx/libgpu.h>
#include "platform/native_projection.h"
// Host-only OT barrier: low code bits are camera+1, or zero for UI.
typedef struct NativeProjectionMarker
{
	DECLARE_P_ADDR
	uint32_t code;
	RECT16 rect;
	NativeProjectionParams params;
} NativeProjectionMarker;
#endif
