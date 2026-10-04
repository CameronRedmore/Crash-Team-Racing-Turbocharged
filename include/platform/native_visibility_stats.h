#ifndef NATIVE_VISIBILITY_STATS_H
#define NATIVE_VISIBILITY_STATS_H

#include <macros.h>

// Counters for the world visibility passes: BSP descent, quadblock emission and
// scenery instance queueing. Wide FOV and ultrawide aspects bypass the retail
// visibility masks (NativeAspect_UsesExpandedVisibility), which makes the cost
// of those passes jump; these counters say by how much, and are what the F6
// debug overlay and the rate-limited log line report.
//
// They are plain increments on already-touched data, so they stay in the noise
// next to the cull tests themselves.
enum NativeVisibilityCounter
{
	// BSP children that reached the cull tests in RenderLists_PushChild.
	NATIVE_VIS_BSP_CHILDREN_TESTED,
	// BSP children the pushbuffer world-AABB test rejected. This AABB is clamped
	// to the level bounds, so at wide FOV it stops excluding anything and the
	// frustum test below carries the culling instead.
	NATIVE_VIS_BSP_CHILDREN_BBOX_REJECTED,
	// BSP children the expanded-visibility far range limit rejected.
	NATIVE_VIS_BSP_CHILDREN_FAR_REJECTED,
	// BSP children that survived the cull tests and went on the walk stack.
	// Descending a branch costs its whole subtree, so this is the number that
	// drives traversal time.
	NATIVE_VIS_BSP_CHILDREN_PUSHED,
	// Leaves linked into a level render list.
	NATIVE_VIS_BSP_LEAVES_LINKED,
	// Leaves the camera frustum rejected.
	NATIVE_VIS_BSP_LEAVES_REJECTED,
	// Quadblocks considered for world geometry emission.
	NATIVE_VIS_QUAD_BLOCKS_TESTED,
	// Quadblocks actually emitted as vertices.
	NATIVE_VIS_QUAD_BLOCKS_EMITTED,
	// Scenery instances that entered the render bucket.
	NATIVE_VIS_INSTANCES_QUEUED,
	// Scenery instances dropped because the render bucket was full.
	NATIVE_VIS_INSTANCES_DROPPED,
	NATIVE_VIS_COUNTER_COUNT
};

void NativeVisibilityCount(enum NativeVisibilityCounter counter, u32 amount);
void NativeVisibilityCountOnce(enum NativeVisibilityCounter counter);

// Fills out[NATIVE_VIS_COUNTER_COUNT] with the counts from the last completed
// frame. Reads are cheap enough for the debug overlay to call every frame.
void NativeVisibilityLastFrame(u64 *out);

// Total for the whole session, for log lines that want a running figure.
u64 NativeVisibilitySessionTotal(enum NativeVisibilityCounter counter);

// Called once per frame by the draw3D frame boundary; logs a rate-limited
// summary when anything changed.
void NativeVisibility_ReportFrameBoundary(void);

#endif
