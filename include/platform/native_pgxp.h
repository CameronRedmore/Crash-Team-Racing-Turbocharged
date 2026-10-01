#ifndef NATIVE_PGXP_H
#define NATIVE_PGXP_H

#include <macros.h>
#include <stddef.h>

// NOTE: Precision geometry transform pipeline, modelled on DuckStation's PGXP.
// RTPS/RTPT truncate every projected vertex to an integer SXY before the CPU
// writes it into a GPU packet. That truncation is what makes PS1 polygons
// wobble as the camera moves, and the missing depth is why the GPU can only
// map textures affinely. Native keeps the sub-pixel screen position and view
// depth of each projection next to the retail integer, follows it through the
// GTE SXY FIFO and the memory words game code stores it into, and lets the GPU
// parser look it up by packet address. Every lookup is validated against the
// integer the packet actually holds, so anything the game computed some other
// way falls back to retail data.

#if defined(__vita__)
#define NATIVE_PGXP_SUPPORTED 0
#else
#define NATIVE_PGXP_SUPPORTED 1
#endif

enum NativePgxpMode
{
	NATIVE_PGXP_MODE_OFF,
	// Sub-pixel vertex positions, retail affine texture mapping.
	NATIVE_PGXP_MODE_GEOMETRY,
	// Sub-pixel vertex positions and perspective-correct texture mapping.
	NATIVE_PGXP_MODE_PERSPECTIVE,
	NATIVE_PGXP_MODE_COUNT,
};

typedef struct
{
	float x;
	float y;
	// View-space depth; <= 0 when the projection has no usable depth.
	float w;
	// Camera-space depth in world units; 0 for HUD, sky, or unknown geometry.
	float depth;
	// Retail packed SXY (x low, y high) this vertex was truncated to.
	u32 value;
} NativePgxpVertex;

extern int gNativePgxpMode;
// Use retail integer winding calculations while retaining PGXP geometry.
extern int gNativePgxpIntegerNclipEnabled;
extern int gNativeDepthBufferEnabled;

#if NATIVE_PGXP_SUPPORTED

#define NATIVE_PGXP_ACTIVE() (gNativePgxpMode != NATIVE_PGXP_MODE_OFF)
#define NATIVE_VERTEX_TRACKING_ACTIVE() (NATIVE_PGXP_ACTIVE() || gNativeDepthBufferEnabled)

// Host range holding the double-buffered primitive memory. Lookups inside it
// use a collision-free direct map; everything else goes through a small cache.
void NativePgxp_SetPrimRegion(const void *start, size_t size);

// GTE side: mirror of the SXY0..SXY2 FIFO.
void NativePgxp_GteProject(float x, float y, float w, u32 value);
void NativePgxp_GteWriteSXY(int reg, u32 value);
void NativePgxp_GteReadSXY(int reg, u32 value);

// Memory side: `dst` is the address of a packed SXY word (or of a primitive's
// x field), `value` the packed SXY being written there.
void NativePgxp_StoreGteSXY(const void *dst, int reg, u32 value);
void NativePgxp_CopyXY(const void *dst, const void *src, u32 value);
void NativePgxp_BindWrittenXY(const void *dst, u32 value);

// World rendering phases may bind CPU-written SXY words to recent GTE results
// by value. HUD and menu drawing never does, so 2D packets stay retail.
void NativePgxp_SetWorldPhase(int active);
// Retail model/tire transforms can scale view coordinates by four. Normalize
// their depth without changing the divisor used for texture interpolation.
float NativePgxp_SetDepthContext(float scale);
void NativePgxp_SetModelDepthScale(const void *key, float scale);
float NativePgxp_GetModelDepthScale(const void *key);

// CPU-side precision, like DuckStation's "PGXP CPU" mode, for the vertices
// CTR builds itself (subdivision midpoints, LOD fades, near-plane clipping).
// Retail truncates those to integer GTE inputs, which leaves them slightly off
// the edges they split; at sub-pixel precision that opens cracks against
// neighbours drawn without the split.
//
// Exact positions are keyed by the address of an s16 xyz triplet and only
// trusted while that triplet still holds the integers they were stored with.
void NativePgxp_SetPosition(const void *key, const s16 *vector, const float *precise);
void NativePgxp_ClearPosition(const void *key);
// Returns 1 with the exact position, or 0 with `vector` converted to float.
int NativePgxp_GetPosition(const void *key, const s16 *vector, float *out);
void NativePgxp_CopyPosition(const void *dstKey, const void *srcKey, const s16 *vector);

// SXY register loaded with MTC2 from a stored projected vertex: bind the
// precise vertex shadowed at that address.
void NativePgxp_GteLoadSXY(int reg, const void *src, u32 value);
// NCLIP from the precise SXY0..SXY2; returns 0 when any is unknown.
int NativePgxp_GteNclip(u32 sxy0, u32 sxy1, u32 sxy2, s64 *out);

// GTE input vectors V0..V2. Loading a vector register drops its exact value,
// so set it after the MTC2s; RTPS/RTPT/MVMVA use it while the integers match.
void NativePgxp_GteInvalidateInput(int reg);
void NativePgxp_GteSetInput(int slot, const s16 *vector, const float *precise);
int NativePgxp_GteGetInput(int slot, s16 vx, s16 vy, s16 vz, double *out);
// Exact MAC1..MAC3 of the last MVMVA, matched against the IR1..IR3 it produced.
void NativePgxp_GteSetMvmvaResult(const double *precise, s16 ir1, s16 ir2, s16 ir3);
int NativePgxp_GteGetMvmvaResult(const s16 *ir, float *out);

void NativePgxp_CameraRotation(const float *angles, double *rotation);
// Model translation follows the GTE's signed input and IR saturation boundaries.
void NativePgxp_ModelViewTranslation(const double *view, const s32 *position, const s16 *camera, const float *preciseCamera, double *translation);

// Camera transforms retain fractional rotation and translation outside PS1 layouts.
void NativePgxp_SetTransform(const void *key, const s16 *rotation, const s32 *translation, const double *preciseRotation, const double *preciseTranslation);
void NativePgxp_GetTransform(const void *key, const s16 *rotation, const s32 *translation, double *pr, double *pt);
void NativePgxp_LoadTransform(const void *key, const s16 *rotation, const s32 *translation, int bank);
void NativePgxp_InvalidateTransform(int reg);
void NativePgxp_Transform(int mx, int cv, const double *rotation, const double *translation, const double *input, double *result);

// GPU side.
int NativePgxp_Lookup(const void *addr, u32 value, NativePgxpVertex *out);
void NativePgxp_EndFrame(void);

// Internal builds: CTR_PGXP_DEBUG=1 logs how many polygons were recovered.
void NativePgxp_DebugCountPolygon(int vertexCount, int recoveredCount, int perspective);

#else

#define NATIVE_PGXP_ACTIVE() 0
#define NATIVE_VERTEX_TRACKING_ACTIVE() 0

static inline void NativePgxp_SetPrimRegion(const void *start, size_t size)
{
	(void)start;
	(void)size;
}
static inline void NativePgxp_GteProject(float x, float y, float w, u32 value)
{
	(void)x;
	(void)y;
	(void)w;
	(void)value;
}
static inline void NativePgxp_GteWriteSXY(int reg, u32 value)
{
	(void)reg;
	(void)value;
}
static inline void NativePgxp_GteReadSXY(int reg, u32 value)
{
	(void)reg;
	(void)value;
}
static inline void NativePgxp_GteLoadSXY(int reg, const void *src, u32 value)
{
	(void)reg;
	(void)src;
	(void)value;
}
static inline int NativePgxp_GteNclip(u32 sxy0, u32 sxy1, u32 sxy2, s64 *out)
{
	(void)sxy0;
	(void)sxy1;
	(void)sxy2;
	(void)out;
	return 0;
}
static inline void NativePgxp_StoreGteSXY(const void *dst, int reg, u32 value)
{
	(void)dst;
	(void)reg;
	(void)value;
}
static inline void NativePgxp_CopyXY(const void *dst, const void *src, u32 value)
{
	(void)dst;
	(void)src;
	(void)value;
}
static inline void NativePgxp_BindWrittenXY(const void *dst, u32 value)
{
	(void)dst;
	(void)value;
}
static inline void NativePgxp_SetWorldPhase(int active)
{
	(void)active;
}
static inline float NativePgxp_SetDepthContext(float scale)
{
	(void)scale;
	return 1.0f;
}
static inline void NativePgxp_SetModelDepthScale(const void *key, float scale)
{
	(void)key;
	(void)scale;
}
static inline float NativePgxp_GetModelDepthScale(const void *key)
{
	(void)key;
	return 0.0f;
}
static inline void NativePgxp_SetPosition(const void *key, const s16 *vector, const float *precise)
{
	(void)key;
	(void)vector;
	(void)precise;
}
static inline void NativePgxp_ClearPosition(const void *key)
{
	(void)key;
}
static inline int NativePgxp_GetPosition(const void *key, const s16 *vector, float *out)
{
	(void)key;
	out[0] = (float)vector[0];
	out[1] = (float)vector[1];
	out[2] = (float)vector[2];
	return 0;
}
static inline void NativePgxp_CopyPosition(const void *dstKey, const void *srcKey, const s16 *vector)
{
	(void)dstKey;
	(void)srcKey;
	(void)vector;
}
static inline void NativePgxp_GteInvalidateInput(int reg)
{
	(void)reg;
}
static inline void NativePgxp_GteSetInput(int slot, const s16 *vector, const float *precise)
{
	(void)slot;
	(void)vector;
	(void)precise;
}
static inline int NativePgxp_GteGetInput(int slot, s16 vx, s16 vy, s16 vz, double *out)
{
	(void)slot;
	(void)vx;
	(void)vy;
	(void)vz;
	(void)out;
	return 0;
}
static inline void NativePgxp_GteSetMvmvaResult(const double *precise, s16 ir1, s16 ir2, s16 ir3)
{
	(void)precise;
	(void)ir1;
	(void)ir2;
	(void)ir3;
}
static inline int NativePgxp_GteGetMvmvaResult(const s16 *ir, float *out)
{
	(void)ir;
	(void)out;
	return 0;
}
static inline void NativePgxp_CameraRotation(const float *angles, double *rotation)
{ (void)angles; (void)rotation; }
static inline void NativePgxp_ModelViewTranslation(const double *view, const s32 *position, const s16 *camera, const float *preciseCamera, double *translation)
{ (void)view; (void)position; (void)camera; (void)preciseCamera; (void)translation; }
static inline void NativePgxp_SetTransform(const void *key, const s16 *r, const s32 *t, const double *pr, const double *pt)
{ (void)key; (void)r; (void)t; (void)pr; (void)pt; }
static inline void NativePgxp_GetTransform(const void *key, const s16 *r, const s32 *t, double *pr, double *pt)
{ (void)key; for (int i = 0; i < 9; i++) pr[i] = r[i]; for (int i = 0; i < 3; i++) pt[i] = t[i]; }
static inline void NativePgxp_LoadTransform(const void *key, const s16 *r, const s32 *t, int bank)
{ (void)key; (void)r; (void)t; (void)bank; }
static inline void NativePgxp_InvalidateTransform(int reg) { (void)reg; }
static inline void NativePgxp_Transform(int mx, int cv, const double *rotation, const double *translation, const double *input, double *result)
{ (void)mx; (void)cv; (void)rotation; (void)translation; (void)input; (void)result; }
static inline int NativePgxp_Lookup(const void *addr, u32 value, NativePgxpVertex *out)
{
	(void)addr;
	(void)value;
	(void)out;
	return 0;
}
static inline void NativePgxp_EndFrame(void)
{
}

#endif

#endif
