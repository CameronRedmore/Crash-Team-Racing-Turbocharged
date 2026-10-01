#include <macros.h>
#include "platform/native_pgxp.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "platform/native_log.h"

#if NATIVE_PGXP_SUPPORTED
int gNativePgxpMode = NATIVE_PGXP_MODE_PERSPECTIVE;
#else
int gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
#endif

#if NATIVE_PGXP_SUPPORTED

// Recently read or stored GTE results, for CPU code that copies SXY words
// around by value before writing them into a packet.
#define NATIVE_PGXP_RECENT_COUNT 64
// Precise vertices stored outside primitive memory (scratchpad, stack, small
// packet buffers). These are consumed quickly, so a direct-mapped cache is
// enough; a collision only costs the evicted vertex its precision.
#define NATIVE_PGXP_CACHE_BITS   12
#define NATIVE_PGXP_CACHE_COUNT  (1u << NATIVE_PGXP_CACHE_BITS)

typedef struct
{
	NativePgxpVertex vertex;
	b32 valid;
} NativePgxpRegister;

typedef struct
{
	NativePgxpVertex vertex;
	u32 generation;
} NativePgxpPrimSlot;

typedef struct
{
	const void *key;
	NativePgxpVertex vertex;
	u32 generation;
} NativePgxpCacheSlot;

global_variable NativePgxpRegister s_pgxpFifo[3];
global_variable NativePgxpVertex s_pgxpRecent[NATIVE_PGXP_RECENT_COUNT];
global_variable u32 s_pgxpRecentGeneration[NATIVE_PGXP_RECENT_COUNT];
global_variable u32 s_pgxpRecentHead;
global_variable NativePgxpCacheSlot s_pgxpCache[NATIVE_PGXP_CACHE_COUNT];
global_variable NativePgxpPrimSlot *s_pgxpPrimSlots;
global_variable uintptr_t s_pgxpPrimStart;
global_variable uintptr_t s_pgxpPrimSize;
// Shadow entries are only trusted for the frame that wrote them; the GPU
// parses every frame's packets before the next frame starts building.
global_variable u32 s_pgxpGeneration = 1;
global_variable b32 s_pgxpWorldPhase;

void NativePgxp_SetPrimRegion(const void *start, size_t size)
{
	const uintptr_t regionStart = (uintptr_t)start;
	const uintptr_t regionSize = (uintptr_t)size & ~(uintptr_t)3;

	if ((regionStart == s_pgxpPrimStart) && (regionSize == s_pgxpPrimSize) && (s_pgxpPrimSlots != NULL))
	{
		return;
	}

	free(s_pgxpPrimSlots);
	s_pgxpPrimSlots = (NativePgxpPrimSlot *)calloc(regionSize / sizeof(u32), sizeof(NativePgxpPrimSlot));
	s_pgxpPrimStart = regionStart;
	s_pgxpPrimSize = (s_pgxpPrimSlots != NULL) ? regionSize : 0;
}

internal NativePgxpPrimSlot *NativePgxp_PrimSlot(const void *addr)
{
	const uintptr_t offset = (uintptr_t)addr - s_pgxpPrimStart;

	if (offset >= s_pgxpPrimSize)
	{
		return NULL;
	}

	return &s_pgxpPrimSlots[offset / sizeof(u32)];
}

internal NativePgxpCacheSlot *NativePgxp_CacheSlot(const void *addr)
{
	const u32 hash = (u32)((uintptr_t)addr >> 1) * 2654435761u;

	return &s_pgxpCache[hash >> (32 - NATIVE_PGXP_CACHE_BITS)];
}

internal void NativePgxp_SetShadow(const void *dst, NativePgxpVertex vertex)
{
	NativePgxpPrimSlot *primSlot = NativePgxp_PrimSlot(dst);

	if (primSlot != NULL)
	{
		primSlot->vertex = vertex;
		primSlot->generation = s_pgxpGeneration;
		return;
	}

	NativePgxpCacheSlot *cacheSlot = NativePgxp_CacheSlot(dst);
	cacheSlot->key = dst;
	cacheSlot->vertex = vertex;
	cacheSlot->generation = s_pgxpGeneration;
}

internal void NativePgxp_ClearShadow(const void *dst)
{
	NativePgxpPrimSlot *primSlot = NativePgxp_PrimSlot(dst);

	if (primSlot != NULL)
	{
		primSlot->generation = 0;
		return;
	}

	NativePgxpCacheSlot *cacheSlot = NativePgxp_CacheSlot(dst);
	if (cacheSlot->key == dst)
	{
		cacheSlot->generation = 0;
	}
}

internal const NativePgxpVertex *NativePgxp_FindShadow(const void *addr, u32 value)
{
	const NativePgxpPrimSlot *primSlot = NativePgxp_PrimSlot(addr);

	if (primSlot != NULL)
	{
		if ((primSlot->generation == s_pgxpGeneration) && (primSlot->vertex.value == value))
		{
			return &primSlot->vertex;
		}
		return NULL;
	}

	const NativePgxpCacheSlot *cacheSlot = NativePgxp_CacheSlot(addr);
	if ((cacheSlot->key == addr) && (cacheSlot->generation == s_pgxpGeneration) && (cacheSlot->vertex.value == value))
	{
		return &cacheSlot->vertex;
	}

	return NULL;
}

internal const NativePgxpVertex *NativePgxp_FindRecent(u32 value)
{
	for (u32 i = 1; i <= NATIVE_PGXP_RECENT_COUNT; i++)
	{
		const u32 index = (s_pgxpRecentHead - i) & (NATIVE_PGXP_RECENT_COUNT - 1);

		if ((s_pgxpRecentGeneration[index] == s_pgxpGeneration) && (s_pgxpRecent[index].value == value))
		{
			return &s_pgxpRecent[index];
		}
	}

	return NULL;
}

internal void NativePgxp_RememberRecent(const NativePgxpVertex *vertex)
{
	const u32 last = (s_pgxpRecentHead - 1) & (NATIVE_PGXP_RECENT_COUNT - 1);

	// Culling helpers read the same SXY registers right before the packet
	// writer does; do not let those repeats flush the history.
	if ((s_pgxpRecentGeneration[last] == s_pgxpGeneration) && (s_pgxpRecent[last].value == vertex->value) && (s_pgxpRecent[last].x == vertex->x) &&
	    (s_pgxpRecent[last].y == vertex->y))
	{
		return;
	}

	const u32 index = s_pgxpRecentHead++ & (NATIVE_PGXP_RECENT_COUNT - 1);
	s_pgxpRecent[index] = *vertex;
	s_pgxpRecentGeneration[index] = s_pgxpGeneration;
}

internal NativePgxpRegister *NativePgxp_FifoRegister(int reg)
{
	// SXYP (15) reads back as SXY2.
	return &s_pgxpFifo[(reg >= 14) ? 2 : (reg - 12)];
}

internal const NativePgxpVertex *NativePgxp_FifoMatch(int reg, u32 value)
{
	const NativePgxpRegister *fifoReg = NativePgxp_FifoRegister(reg);

	return (fifoReg->valid && (fifoReg->vertex.value == value)) ? &fifoReg->vertex : NULL;
}

void NativePgxp_GteProject(float x, float y, float w, u32 value)
{
	s_pgxpFifo[0] = s_pgxpFifo[1];
	s_pgxpFifo[1] = s_pgxpFifo[2];
	s_pgxpFifo[2].vertex.x = x;
	s_pgxpFifo[2].vertex.y = y;
	s_pgxpFifo[2].vertex.w = w;
	s_pgxpFifo[2].vertex.value = value;
	s_pgxpFifo[2].valid = 1;
}

void NativePgxp_GteWriteSXY(int reg, u32 value)
{
	NativePgxpRegister written = {0};
	const NativePgxpVertex *source = NULL;

	// Game code saves and restores SXY registers around helper calls, or
	// shuffles them to continue a strip; recover the precise vertex the value
	// came from.
	for (int i = 2; (i >= 0) && (source == NULL); i--)
	{
		if (s_pgxpFifo[i].valid && (s_pgxpFifo[i].vertex.value == value))
		{
			source = &s_pgxpFifo[i].vertex;
		}
	}

	if (source == NULL)
	{
		source = NativePgxp_FindRecent(value);
	}

	if (source != NULL)
	{
		written.vertex = *source;
		written.valid = 1;
	}

	if (reg == 15)
	{
		s_pgxpFifo[0] = s_pgxpFifo[1];
		s_pgxpFifo[1] = s_pgxpFifo[2];
		s_pgxpFifo[2] = written;
	}
	else
	{
		s_pgxpFifo[reg - 12] = written;
	}
}

void NativePgxp_GteLoadSXY(int reg, const void *src, u32 value)
{
	const NativePgxpVertex *vertex = NativePgxp_FindShadow(src, value);
	NativePgxpRegister *fifoReg = NativePgxp_FifoRegister(reg);

	if (vertex != NULL)
	{
		fifoReg->vertex = *vertex;
		fifoReg->valid = 1;
	}
}

int NativePgxp_GteNclip(u32 sxy0, u32 sxy1, u32 sxy2, s64 *out)
{
	const NativePgxpVertex *v0 = NativePgxp_FifoMatch(12, sxy0);
	const NativePgxpVertex *v1 = NativePgxp_FifoMatch(13, sxy1);
	const NativePgxpVertex *v2 = NativePgxp_FifoMatch(14, sxy2);

	if ((v0 == NULL) || (v1 == NULL) || (v2 == NULL))
	{
		return 0;
	}

	// Same winding test as NCLIP, on the positions the GPU will draw. A sliver
	// that rounds to zero or negative area on the integer grid can still cover
	// pixels at its precise position, and culling it leaves a hole.
	double nclip = ((double)v0->x * v1->y) + ((double)v1->x * v2->y) + ((double)v2->x * v0->y) - ((double)v0->x * v2->y) - ((double)v1->x * v0->y) -
	               ((double)v2->x * v1->y);

	// Keep small fractional areas from truncating to zero (DuckStation does
	// the same).
	const double nclipAbs = (nclip < 0.0) ? -nclip : nclip;
	if ((nclipAbs > 0.1) && (nclipAbs < 1.0))
	{
		nclip += (nclip < 0.0) ? -1.0 : 1.0;
	}

	*out = (s64)nclip;
	return 1;
}

void NativePgxp_GteReadSXY(int reg, u32 value)
{
	const NativePgxpVertex *vertex = NativePgxp_FifoMatch(reg, value);

	if (vertex != NULL)
	{
		NativePgxp_RememberRecent(vertex);
	}
}

void NativePgxp_StoreGteSXY(const void *dst, int reg, u32 value)
{
	const NativePgxpVertex *vertex = NativePgxp_FifoMatch(reg, value);

	if (vertex != NULL)
	{
		NativePgxp_SetShadow(dst, *vertex);
	}
	else
	{
		NativePgxp_ClearShadow(dst);
	}
}

void NativePgxp_CopyXY(const void *dst, const void *src, u32 value)
{
	const NativePgxpVertex *vertex = NativePgxp_FindShadow(src, value);

	if (vertex != NULL)
	{
		NativePgxp_SetShadow(dst, *vertex);
	}
	else
	{
		NativePgxp_ClearShadow(dst);
	}
}

void NativePgxp_BindWrittenXY(const void *dst, u32 value)
{
	const NativePgxpVertex *vertex = s_pgxpWorldPhase ? NativePgxp_FindRecent(value) : NULL;

	if (vertex != NULL)
	{
		NativePgxp_SetShadow(dst, *vertex);
	}
	else
	{
		NativePgxp_ClearShadow(dst);
	}
}

void NativePgxp_SetWorldPhase(int active)
{
	s_pgxpWorldPhase = active != 0;
}

#define NATIVE_PGXP_POSITION_BITS  13
#define NATIVE_PGXP_POSITION_COUNT (1u << NATIVE_PGXP_POSITION_BITS)

typedef struct
{
	const void *key;
	s16 vector[3];
	float precise[3];
	u32 generation;
} NativePgxpPositionSlot;

typedef struct
{
	s16 vector[3];
	b32 valid;
	double precise[3];
} NativePgxpInput;

global_variable NativePgxpPositionSlot s_pgxpPositions[NATIVE_PGXP_POSITION_COUNT];
global_variable NativePgxpInput s_pgxpInputs[3];
global_variable NativePgxpInput s_pgxpMvmvaResult;

internal NativePgxpPositionSlot *NativePgxp_PositionSlot(const void *key)
{
	const u32 hash = (u32)((uintptr_t)key >> 1) * 2654435761u;

	return &s_pgxpPositions[hash >> (32 - NATIVE_PGXP_POSITION_BITS)];
}

void NativePgxp_SetPosition(const void *key, const s16 *vector, const float *precise)
{
	NativePgxpPositionSlot *slot = NativePgxp_PositionSlot(key);

	slot->key = key;
	slot->vector[0] = vector[0];
	slot->vector[1] = vector[1];
	slot->vector[2] = vector[2];
	slot->precise[0] = precise[0];
	slot->precise[1] = precise[1];
	slot->precise[2] = precise[2];
	slot->generation = s_pgxpGeneration;
}

void NativePgxp_ClearPosition(const void *key)
{
	NativePgxpPositionSlot *slot = NativePgxp_PositionSlot(key);

	if (slot->key == key)
	{
		slot->generation = 0;
	}
}

int NativePgxp_GetPosition(const void *key, const s16 *vector, float *out)
{
	const NativePgxpPositionSlot *slot = NativePgxp_PositionSlot(key);

	if ((slot->key == key) && (slot->generation == s_pgxpGeneration) && (slot->vector[0] == vector[0]) && (slot->vector[1] == vector[1]) &&
	    (slot->vector[2] == vector[2]))
	{
		out[0] = slot->precise[0];
		out[1] = slot->precise[1];
		out[2] = slot->precise[2];
		return 1;
	}

	out[0] = (float)vector[0];
	out[1] = (float)vector[1];
	out[2] = (float)vector[2];
	return 0;
}

void NativePgxp_CopyPosition(const void *dstKey, const void *srcKey, const s16 *vector)
{
	float precise[3];

	if (dstKey == srcKey)
	{
		return;
	}

	if (NativePgxp_GetPosition(srcKey, vector, precise))
	{
		NativePgxp_SetPosition(dstKey, vector, precise);
	}
	else
	{
		NativePgxp_ClearPosition(dstKey);
	}
}

void NativePgxp_GteInvalidateInput(int reg)
{
	if ((reg >= 0) && (reg <= 5))
	{
		s_pgxpInputs[reg >> 1].valid = 0;
	}
}

void NativePgxp_GteSetInput(int slot, const s16 *vector, const float *precise)
{
	NativePgxpInput *input = &s_pgxpInputs[slot];

	input->vector[0] = vector[0];
	input->vector[1] = vector[1];
	input->vector[2] = vector[2];
	input->precise[0] = precise[0];
	input->precise[1] = precise[1];
	input->precise[2] = precise[2];
	input->valid = 1;
}

int NativePgxp_GteGetInput(int slot, s16 vx, s16 vy, s16 vz, double *out)
{
	if ((slot < 0) || (slot >= 3)) return 0;
	const NativePgxpInput *input = &s_pgxpInputs[slot];

	if (!input->valid || (input->vector[0] != vx) || (input->vector[1] != vy) || (input->vector[2] != vz))
	{
		return 0;
	}

	out[0] = input->precise[0];
	out[1] = input->precise[1];
	out[2] = input->precise[2];
	return 1;
}

void NativePgxp_GteSetMvmvaResult(const double *precise, s16 ir1, s16 ir2, s16 ir3)
{
	s_pgxpMvmvaResult.vector[0] = ir1;
	s_pgxpMvmvaResult.vector[1] = ir2;
	s_pgxpMvmvaResult.vector[2] = ir3;
	s_pgxpMvmvaResult.precise[0] = precise[0];
	s_pgxpMvmvaResult.precise[1] = precise[1];
	s_pgxpMvmvaResult.precise[2] = precise[2];
	s_pgxpMvmvaResult.valid = 1;
}

int NativePgxp_GteGetMvmvaResult(const s16 *ir, float *out)
{
	if (!s_pgxpMvmvaResult.valid || (s_pgxpMvmvaResult.vector[0] != ir[0]) || (s_pgxpMvmvaResult.vector[1] != ir[1]) ||
	    (s_pgxpMvmvaResult.vector[2] != ir[2]))
	{
		return 0;
	}

	out[0] = (float)s_pgxpMvmvaResult.precise[0];
	out[1] = (float)s_pgxpMvmvaResult.precise[1];
	out[2] = (float)s_pgxpMvmvaResult.precise[2];
	return 1;
}

void NativePgxp_CameraRotation(const float *rot, double *rotation)
{
	const double radians = 6.28318530717958647692 / 4096.0;
	const double sx = sin(rot[0] * radians), cx = cos(rot[0] * radians);
	const double sy = sin(rot[1] * radians), cy = cos(rot[1] * radians);
	const double sz = sin(rot[2] * radians), cz = cos(rot[2] * radians);
	// ConvertRotToMatrix composes Ry * Rx * Rz.
	const double camera[9] = {
		cy*cz + sy*sx*sz, -cy*sz + sy*sx*cz, sy*cx,
		cx*sz, cx*cz, -sx,
		-sy*cz + cy*sx*sz, sy*sz + cy*sx*cz, cy*cx
	};
	memcpy(rotation, camera, sizeof(camera));
}

// Address and full integer snapshots prevent stale scratch/stack transforms
// from applying to another matrix. Register writes invalidate loaded shadows.
#define NATIVE_PGXP_TRANSFORM_COUNT 4096
typedef struct
{
	const void *key;
	s16 rotation[9];
	s32 translation[3];
	double preciseRotation[9]; // Q12 units, without rounding
	double preciseTranslation[3];
	u32 generation;
} NativePgxpTransform;
static NativePgxpTransform s_pgxpTransforms[NATIVE_PGXP_TRANSFORM_COUNT];
static NativePgxpTransform s_pgxpTransformBanks[3];
static b32 s_pgxpRotationValid[3], s_pgxpTranslationValid[3];

static NativePgxpTransform *NativePgxp_TransformSlot(const void *key)
{
	return &s_pgxpTransforms[((u32)((uintptr_t)key >> 2) * 2654435761u) >> 20];
}

void NativePgxp_SetTransform(const void *key, const s16 *rotation, const s32 *translation, const double *pr, const double *pt)
{
	NativePgxpTransform *slot = NativePgxp_TransformSlot(key);
	slot->key = key;
	memcpy(slot->rotation, rotation, sizeof(slot->rotation));
	memcpy(slot->translation, translation, sizeof(slot->translation));
	memcpy(slot->preciseRotation, pr, sizeof(slot->preciseRotation));
	memcpy(slot->preciseTranslation, pt, sizeof(slot->preciseTranslation));
	slot->generation = s_pgxpGeneration;
}

void NativePgxp_GetTransform(const void *key, const s16 *rotation, const s32 *translation, double *pr, double *pt)
{
	const NativePgxpTransform *slot = NativePgxp_TransformSlot(key);
	b32 valid = slot->key == key && slot->generation == s_pgxpGeneration;
	b32 rotValid = valid && memcmp(slot->rotation, rotation, sizeof(slot->rotation)) == 0;
	b32 transValid = valid && memcmp(slot->translation, translation, sizeof(slot->translation)) == 0;
	for (int i = 0; i < 9; i++) pr[i] = rotValid ? slot->preciseRotation[i] : rotation[i];
	for (int i = 0; i < 3; i++) pt[i] = transValid ? slot->preciseTranslation[i] : translation[i];
}

void NativePgxp_LoadTransform(const void *key, const s16 *rotation, const s32 *translation, int bank)
{
	NativePgxpTransform *slot = NativePgxp_TransformSlot(key);
	int index = bank & 3;
	if (index >= 3) return;
	// bank 0..2 loads rotation; bank 4..6 loads translation only.
	b32 valid = slot->key == key && slot->generation == s_pgxpGeneration;
	if (bank < 4)
	{
		s_pgxpRotationValid[index] = valid && memcmp(slot->rotation, rotation, sizeof(slot->rotation)) == 0;
		if (s_pgxpRotationValid[index])
			memcpy(s_pgxpTransformBanks[index].preciseRotation, slot->preciseRotation, sizeof(slot->preciseRotation));
	}
	else
	{
		s_pgxpTranslationValid[index] = valid && memcmp(slot->translation, translation, sizeof(slot->translation)) == 0;
		if (s_pgxpTranslationValid[index])
			memcpy(s_pgxpTransformBanks[index].preciseTranslation, slot->preciseTranslation, sizeof(slot->preciseTranslation));
	}
}

void NativePgxp_InvalidateTransform(int reg)
{
	for (int i = 0; i < 3; i++)
	{
		if (reg >= i * 8 && reg < i * 8 + 5) s_pgxpRotationValid[i] = 0;
		if (reg >= i * 8 + 5 && reg < i * 8 + 8) s_pgxpTranslationValid[i] = 0;
	}
}

void NativePgxp_Transform(int mx, int cv, const double *rotation, const double *translation, const double *input, double *result)
{
	for (int row = 0; row < 3; row++)
	{
		const double *r = (mx < 3 && s_pgxpRotationValid[mx]) ?
			&s_pgxpTransformBanks[mx].preciseRotation[row * 3] : &rotation[row * 3];
		const double t = (cv < 3 && s_pgxpTranslationValid[cv]) ?
			s_pgxpTransformBanks[cv].preciseTranslation[row] : translation[row];
		result[row] = r[0] * input[0] + r[1] * input[1] + r[2] * input[2] + t * 4096.0;
	}
}

int NativePgxp_Lookup(const void *addr, u32 value, NativePgxpVertex *out)
{
	const NativePgxpVertex *vertex = NativePgxp_FindShadow(addr, value);

	if (vertex == NULL)
	{
		return 0;
	}

	*out = *vertex;
	return 1;
}

#if defined(CTR_INTERNAL)
enum
{
	NATIVE_PGXP_DEBUG_PERSPECTIVE,
	NATIVE_PGXP_DEBUG_SUBPIXEL,
	NATIVE_PGXP_DEBUG_PARTIAL,
	NATIVE_PGXP_DEBUG_RETAIL,
	NATIVE_PGXP_DEBUG_COUNT,
};

#define NATIVE_PGXP_DEBUG_INTERVAL 300

global_variable int s_pgxpDebug = -1;
global_variable u32 s_pgxpDebugFrames;
global_variable u32 s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_COUNT];

void NativePgxp_DebugCountPolygon(int vertexCount, int recoveredCount, int perspective)
{
	if (s_pgxpDebug <= 0)
	{
		return;
	}

	if (perspective)
	{
		s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_PERSPECTIVE]++;
	}
	else if (recoveredCount == vertexCount)
	{
		s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_SUBPIXEL]++;
	}
	else if (recoveredCount > 0)
	{
		s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_PARTIAL]++;
	}
	else
	{
		s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_RETAIL]++;
	}
}

internal void NativePgxp_DebugEndFrame(void)
{
	if (s_pgxpDebug < 0)
	{
		const char *env = getenv("CTR_PGXP_DEBUG");
		s_pgxpDebug = (env != NULL) && (env[0] != '\0') && (env[0] != '0');
	}

	if ((s_pgxpDebug <= 0) || (++s_pgxpDebugFrames < NATIVE_PGXP_DEBUG_INTERVAL))
	{
		return;
	}

	Platform_Log("[CTR PGXP] polygons/frame: perspective %u, sub-pixel affine %u, partial %u, retail %u\n",
	             s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_PERSPECTIVE] / s_pgxpDebugFrames, s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_SUBPIXEL] / s_pgxpDebugFrames,
	             s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_PARTIAL] / s_pgxpDebugFrames, s_pgxpDebugPolygons[NATIVE_PGXP_DEBUG_RETAIL] / s_pgxpDebugFrames);
	s_pgxpDebugFrames = 0;
	for (int i = 0; i < NATIVE_PGXP_DEBUG_COUNT; i++)
	{
		s_pgxpDebugPolygons[i] = 0;
	}
}
#else
void NativePgxp_DebugCountPolygon(int vertexCount, int recoveredCount, int perspective)
{
	(void)vertexCount;
	(void)recoveredCount;
	(void)perspective;
}
#endif

void NativePgxp_EndFrame(void)
{
#if defined(CTR_INTERNAL)
	NativePgxp_DebugEndFrame();
#endif
	s_pgxpGeneration++;
	if (s_pgxpGeneration == 0)
	{
		s_pgxpGeneration = 1;
	}
	s_pgxpWorldPhase = 0;
	memset(s_pgxpFifo, 0, sizeof(s_pgxpFifo));
	memset(s_pgxpInputs, 0, sizeof(s_pgxpInputs));
	s_pgxpMvmvaResult.valid = 0;
	memset(s_pgxpRotationValid, 0, sizeof(s_pgxpRotationValid));
	memset(s_pgxpTranslationValid, 0, sizeof(s_pgxpTranslationValid));
}

#endif
