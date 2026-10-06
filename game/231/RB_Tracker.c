#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b28c0-0x800b295c.
// Symbol tail 0x800b295c-0x800b3120 is overlay data before Baron.
struct Thread *RB_GetThread_ClosestTracker(struct Driver *d)
{
	int distX;
	int distZ;
	struct Thread *currThread;
	struct Thread *closestTh = 0;
	int smallestDist;

	// assume farthest distance
	smallestDist = 0x3fffffff;

	// loop through all threads
	for (currThread = P32_GET(struct Thread *, P32_GET(struct GameTracker *, sdata->gGT)->threadBuckets[TRACKING].thread); currThread != NULL; currThread = P32_GET(struct Thread *, currThread->siblingThread))
	{
		struct TrackerWeapon *tw = P32_GET(void *, currThread->object);

		if (P32_GET(struct Driver *, tw->driverTarget) != d)
		{
			continue;
		}

		struct Instance *dInst = P32_GET(struct Instance *, d->instSelf);
		struct Instance *currInst = P32_GET(struct Instance *, currThread->inst);

		if (
		    // get distance between posX and posZ of
		    // driver->instSelf->position, and tracker's position,
		    distX = dInst->matrix.t[0] - currInst->matrix.t[0], distZ = dInst->matrix.t[2] - currInst->matrix.t[2],

		    // if this is a new closest distance
		    distX = distX * distX + distZ * distZ, distX < smallestDist)

		{
			// save closest distance
			smallestDist = distX;
			closestTh = currThread;
		}
	}
	// return thread of closest tracker
	return closestTh;
}
