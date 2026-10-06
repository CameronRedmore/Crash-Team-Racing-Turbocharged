#include <common.h>

enum RenderListsSlot1P2P
{
	RENDER_LIST_SLOT_4X4 = 0,
	RENDER_LIST_SLOT_DYNAMIC_SUBDIV = 1,
	RENDER_LIST_SLOT_4X2 = 2,
	RENDER_LIST_SLOT_4X1 = 3,
	RENDER_LIST_SLOT_WATER = 4,
	RENDER_LIST_SLOT_FULL_DYNAMIC = 5,
};

enum RenderListsScratchOffset
{
	RENDER_LISTS_CORNER_JUMP_TABLE_OFFSET = 0x84,
	RENDER_LISTS_STACK_OFFSET = 0xd0,
};

struct RenderListsScratchRecord
{
	BspChildId childID;
	s16 unused;
	struct BoundingBox box;
};

CTR_STATIC_ASSERT(sizeof(struct RenderListsScratchRecord) == 0x10);

static int RenderLists_IsVisible(const int *visLeafList, BspChildId childID)
{
	u32 rawChildID = (u16)childID;

#if defined(CTR_NATIVE)
	// Ultrawide side geometry may be absent from the retail camera-cell PVS.
	// Keep the spatial bounds and leaf frustum tests below as the cullers.
	if (NativeAspect_UsesExpandedVisibility())
		return 1;
#endif

	if (visLeafList == 0)
	{
		return 1;
	}

	return ((s32)visLeafList[(rawChildID >> 5) & 0x1ff] << (rawChildID & 0x1f)) < 0;
}

static int RenderLists_BoxOverlapsPushBuffer(const struct BoundingBox *box, const struct BoundingBox *pbBox)
{
	return (box->min.x <= pbBox->max.x) && (pbBox->min.x <= box->max.x) && (box->min.y <= pbBox->max.y) && (pbBox->min.y <= box->max.y) &&
	       (box->min.z <= pbBox->max.z) && (pbBox->min.z <= box->max.z);
}

static void RenderLists_SelectBoxCorner(const struct BoundingBox *box, int cornerIndex, s16 *point)
{
	// NOTE(aalhendi): This is the exact corner order produced by retail helpers 0x80070310..0x8007037c: max/max/max through min/min/min by bit index.
	point[0] = (cornerIndex & 1) ? box->min.x : box->max.x;
	point[1] = (cornerIndex & 2) ? box->min.y : box->max.y;
	point[2] = (cornerIndex & 4) ? box->min.z : box->max.z;
}

static void RenderLists_Load1P2PGteState(struct PushBuffer *pb)
{
	if (pb == 0)
	{
		return;
	}

	// NOTE(aalhendi): Retail 0x8006fecc loads RT/TR/OFX/OFY/H once before traversal; frustum helpers only touch the light-matrix/RBK controls.
	gte_SetRotMatrix(&pb->matrix_ViewProj);
	gte_SetTransMatrix(&pb->matrix_ViewProj);
	gte_SetGeomOffset(pb->rect.w >> 1, pb->rect.h >> 1);
	gte_SetGeomScreen(pb->distanceToScreen_PREV);
}

static int RenderLists_FrustumRejectsCorner(const struct PushBufferFrustumPlane *plane, const s16 *point)
{
	// NOTE(aalhendi): Retail 0x80070290 tests the selected BSP corner with llv0bk and rejects when IR1 is positive.
	MTC2(CTR_PackS16Pair(point[0], point[1]), 0);
	MTC2((u32)(s32)point[2], 1);
	CTC2(CTR_PackS16Pair(plane->normal.x, plane->normal.y), 8);
	CTC2(CTR_PackS16Pair(plane->normal.z, plane->halfDistance), 9);
	CTC2((u32)(s32)(-2 * plane->halfDistance), 13);
	doCOP2(0x04a2012);

	return MFC2_S(9) > 0;
}

#if defined(CTR_NATIVE)
// Prepared once for each BSP walk and native level pass. These planes use
// the same precise transform and projection as submitted world vertices.
static struct PushBuffer *sRenderListsNativeCamera;
static double sRenderListsNativePlanes[5][4];

static void RenderLists_PrepareNativeFrustum(struct PushBuffer *pb)
{
	double rotation[9], translation[3];
	NativePgxp_GetTransform(&pb->matrix_ViewProj, &pb->matrix_ViewProj.m[0][0], pb->matrix_ViewProj.t, rotation, translation);
	for (int plane = 0; plane < 5; plane++)
	{
		const int axis = plane / 2;
		const double extent = plane < 2 ? pb->rect.w * 0.5 + 1.0 : pb->rect.h * 0.5 + 1.0;
		const double sign = (plane & 1) ? -1.0 : 1.0;
		for (int component = 0; component < 4; component++)
		{
			const double z = component < 3 ? rotation[6 + component] / 4096.0 : translation[2];
			const double xy = component < 3 ? rotation[axis * 3 + component] / 4096.0 : translation[axis];
			sRenderListsNativePlanes[plane][component] = plane == 4 ? z : extent * z + sign * pb->distanceToScreen_PREV * xy;
		}
	}
	sRenderListsNativeCamera = pb;
}

static int RenderLists_BoxPassesNativeFrustum(struct PushBuffer *pb, const struct BoundingBox *box)
{
	if (sRenderListsNativeCamera != pb)
		RenderLists_PrepareNativeFrustum(pb);
	for (int plane = 0; plane < 5; plane++)
	{
		const double *p = sRenderListsNativePlanes[plane];
		double maximum = p[3];
		for (int axis = 0; axis < 3; axis++)
			maximum += p[axis] * (p[axis] < 0 ? box->min.v[axis] : box->max.v[axis]);
		// Reject only when the entire box is outside. No artificial far plane:
		// retail's clipped corner-ray AABB does not contain a wide frustum.
		if (maximum < -0.001)
			return 0;
	}
	return 1;
}
#endif

static int RenderLists_BoxPassesFrustum(struct PushBuffer *pb, const struct BoundingBox *box)
{
	s16 point[3];
	const struct PushBufferFrustumPlane *plane;
#if defined(CTR_NATIVE)
	if (pb && NativeAspect_UsesExpandedVisibility())
	{
		return RenderLists_BoxPassesNativeFrustum(pb, box);
	}
#endif

	if (pb == 0)
	{
		return 1;
	}

	for (int planeIndex = 0; planeIndex < 4; planeIndex++)
	{
		// NOTE(aalhendi): Retail tests planes in 1,3,0,2 order; each test is independent, so native preserves the plane/corner pairing in a simple loop.
		plane = &pb->frustumPlanes[planeIndex];
		RenderLists_SelectBoxCorner(box, pb->RenderListJmpIndex[planeIndex] & 7, point);

		if (RenderLists_FrustumRejectsCorner(plane, point))
		{
			return 0;
		}
	}

	return 1;
}

static int RenderLists_BoxPassesSpatialBounds(struct PushBuffer *pb, const struct BoundingBox *box)
{
#if defined(CTR_NATIVE)
	if (NativeAspect_UsesExpandedVisibility())
		return RenderLists_BoxPassesFrustum(pb, box);
#endif
	return RenderLists_BoxOverlapsPushBuffer(box, &pb->bbox);
}

static int RenderLists_ProjectDistance(struct PushBuffer *pb, const struct BoundingBox *box)
{
	s16 point[3];
	int distance;

	if (pb == 0)
	{
		return 0;
	}

	RenderLists_SelectBoxCorner(box, pb->RenderListJmpIndex[5] & 7, point);

	CTR_GteLoadS16TripletV0(point);
	gte_rtps();
	gte_stsz(&distance);

	return distance;
}

static int RenderLists_Select1P2PSlot(const struct BSP *bsp, struct PushBuffer *pb, int lodDistanceThreshold)
{
	if ((bsp->flag & BSP_LEAF_FLAG_WATER) != 0)
	{
		return RENDER_LIST_SLOT_WATER;
	}

	if ((bsp->flag & BSP_RENDER_LEAF_FLAG_DYNAMIC_SUBDIV) != 0)
	{
		return RENDER_LIST_SLOT_DYNAMIC_SUBDIV;
	}

	if (RenderLists_ProjectDistance(pb, &bsp->box) > lodDistanceThreshold)
	{
		return RENDER_LIST_SLOT_FULL_DYNAMIC;
	}

	if ((bsp->flag & BSP_RENDER_LEAF_FLAG_4X4) != 0)
	{
		return RENDER_LIST_SLOT_4X4;
	}

	if ((bsp->flag & BSP_RENDER_LEAF_FLAG_4X1) != 0)
	{
		return RENDER_LIST_SLOT_4X1;
	}

	if ((bsp->flag & BSP_RENDER_LEAF_FLAG_4X2) != 0)
	{
		return RENDER_LIST_SLOT_4X2;
	}

	return RENDER_LIST_SLOT_DYNAMIC_SUBDIV;
}

static P32(struct VisMemBspListNode *) *RenderLists_Get1P2PHead(void *LevRenderList, int slotIndex)
{
	if (slotIndex == RENDER_LIST_SLOT_FULL_DYNAMIC)
	{
		return (P32(struct VisMemBspListNode *) *)((char *)LevRenderList + 0x28);
	}

	return (P32(struct VisMemBspListNode *) *)((char *)LevRenderList + slotIndex * 8 + 4);
}

static int RenderLists_Select3P4PSlot(const struct BSP *bsp)
{
	if ((bsp->flag & BSP_LEAF_FLAG_WATER) != 0)
	{
		return 2;
	}

	if ((bsp->flag & BSP_RENDER_LEAF_FLAG_DYNAMIC_SUBDIV) != 0)
	{
		return 3;
	}

	if ((bsp->flag & BSP_RENDER_LEAF_FLAG_4X4) != 0)
	{
		return 0;
	}

	return 1;
}

static void RenderLists_LinkBsp(struct BSP *bspRoot, struct BSP *bsp, P32(struct VisMemBspListNode *) *head, struct VisMemBspListNode *bspList)
{
	struct VisMemBspListNode *node;
	int bspIndex = bsp - bspRoot;

	node = &bspList[bspIndex];

	P32_SET(node->next, P32_GET(struct VisMemBspListNode *, *head));
	P32_SET(*head, node);
}

static void RenderLists_PushChild(struct BSP *bspRoot, const int *visLeafList, struct PushBuffer *pb, BspChildId childID,
                                  struct RenderListsScratchRecord **stack, struct RenderListsScratchRecord *stackEnd)
{
	struct BSP *child;
	struct RenderListsScratchRecord *record;

	if (childID < 0)
	{
		return;
	}

	if (!RenderLists_IsVisible(visLeafList, childID))
	{
		return;
	}

	child = &bspRoot[((u16)childID) & BSP_CHILD_ID_INDEX_MASK];
	if (!RenderLists_BoxPassesSpatialBounds(pb, &child->box))
	{
		return;
	}

	if (*stack >= stackEnd)
	{
		return;
	}

	record = *stack;
	record->childID = childID;
	record->unused = 0;
	record->box = child->box;
	*stack = record + 1;
}

static int RenderLists_Walk1P2P(struct BSP *bspRoot, const int *visLeafList, struct PushBuffer *pb, void *LevRenderList, struct VisMemBspListNode *bspList,
                                u8 numPlyr)
{
	struct RenderListsScratchRecord *stackBase = CTR_SCRATCHPAD_PTR(struct RenderListsScratchRecord, RENDER_LISTS_STACK_OFFSET);
	struct RenderListsScratchRecord *stackEnd = CTR_SCRATCHPAD_PTR(struct RenderListsScratchRecord, CTR_SCRATCHPAD_SIZE);
	struct RenderListsScratchRecord *stack = stackBase;
	struct BSP *branch = bspRoot;
	int lodDistanceThreshold = (numPlyr == 1) ? CTR_SCRATCHPAD_PTR(struct MainRenderLevelGeometryScratch, 0)->bspLodDistanceThreshold : 0x1540;
	int count = 0;

	if (bspRoot == 0 || pb == 0)
	{
		return 0;
	}

	if ((bspRoot->flag & BSP_NODE_FLAG_LEAF) != 0)
	{
		int slotIndex = RenderLists_Select1P2PSlot(bspRoot, pb, lodDistanceThreshold);

		RenderLists_LinkBsp(bspRoot, bspRoot, RenderLists_Get1P2PHead(LevRenderList, slotIndex), bspList);
		return 1;
	}

	for (;;)
	{
		RenderLists_PushChild(bspRoot, visLeafList, pb, branch->data.branch.childID[0], &stack, stackEnd);
		RenderLists_PushChild(bspRoot, visLeafList, pb, branch->data.branch.childID[1], &stack, stackEnd);

		while (stack != stackBase)
		{
			struct RenderListsScratchRecord record = *--stack;
			struct BSP *bsp = &bspRoot[((u16)record.childID) & BSP_CHILD_ID_INDEX_MASK];

			if (((u16)record.childID & BSP_CHILD_ID_LEAF_FLAG) == 0)
			{
				branch = bsp;
				goto nextBranch;
			}

			if (!RenderLists_BoxPassesFrustum(pb, &record.box))
			{
				continue;
			}

			int slotIndex = RenderLists_Select1P2PSlot(bsp, pb, lodDistanceThreshold);
			RenderLists_LinkBsp(bspRoot, bsp, RenderLists_Get1P2PHead(LevRenderList, slotIndex), bspList);
			count++;
		}

		return count;

	nextBranch:
		continue;
	}
}

static int RenderLists_Walk3P4P(struct BSP *bspRoot, const int *visLeafList, struct PushBuffer *pb, void *LevRenderList, struct VisMemBspListNode *bspList)
{
	struct RenderListsScratchRecord *stackBase = CTR_SCRATCHPAD_PTR(struct RenderListsScratchRecord, RENDER_LISTS_STACK_OFFSET);
	struct RenderListsScratchRecord *stackEnd = CTR_SCRATCHPAD_PTR(struct RenderListsScratchRecord, CTR_SCRATCHPAD_SIZE);
	struct RenderListsScratchRecord *stack = stackBase;
	struct BSP *branch = bspRoot;
	int count = 0;

	if (bspRoot == 0 || pb == 0)
	{
		return 0;
	}

	if ((bspRoot->flag & BSP_NODE_FLAG_LEAF) != 0)
	{
		int slotIndex = RenderLists_Select3P4PSlot(bspRoot);
		P32(struct VisMemBspListNode *) *head = (P32(struct VisMemBspListNode *) *)((char *)LevRenderList + slotIndex * 8 + 4);

		RenderLists_LinkBsp(bspRoot, bspRoot, head, bspList);
		return 1;
	}

	for (;;)
	{
		RenderLists_PushChild(bspRoot, visLeafList, pb, branch->data.branch.childID[0], &stack, stackEnd);
		RenderLists_PushChild(bspRoot, visLeafList, pb, branch->data.branch.childID[1], &stack, stackEnd);

		while (stack != stackBase)
		{
			struct RenderListsScratchRecord record = *--stack;
			struct BSP *bsp = &bspRoot[((u16)record.childID) & BSP_CHILD_ID_INDEX_MASK];

			if (((u16)record.childID & BSP_CHILD_ID_LEAF_FLAG) == 0)
			{
				branch = bsp;
				goto nextBranch;
			}

			if (!RenderLists_BoxPassesFrustum(pb, &record.box))
			{
				continue;
			}

			int slotIndex = RenderLists_Select3P4PSlot(bsp);
			P32(struct VisMemBspListNode *) *head = (P32(struct VisMemBspListNode *) *)((char *)LevRenderList + slotIndex * 8 + 4);

			RenderLists_LinkBsp(bspRoot, bsp, head, bspList);
			count++;
		}

		return count;

	nextBranch:
		continue;
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800702d4-0x80070388.
// NOTE(aalhendi): Retail corner helpers 0x80070308..0x80070384 are represented by RenderLists_SelectBoxCorner.
void RenderLists_PreInit()
{
	static const u32 renderListJumpTable[] = {
	    0x80070310, 0x80070320, 0x80070330, 0x80070340, 0x8007034c, 0x8007035c, 0x8007036c, 0x8007037c,
	};
	u32 *scratchJumpTable = CTR_SCRATCHPAD_PTR(u32, RENDER_LISTS_CORNER_JUMP_TABLE_OFFSET);

	for (int i = 0; i < 8; i++)
	{
		scratchJumpTable[i] = renderListJumpTable[i];
	}
}

int RenderLists_Init1P2P(struct BSP *bspRoot, int *visLeafList, struct PushBuffer *pb, void *LevRenderList, void *bspList, u8 numPlyr)
{
#if defined(CTR_NATIVE)
	if (pb && NativeAspect_UsesExpandedVisibility())
		RenderLists_PrepareNativeFrustum(pb);
#endif
	// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8006fe70-0x800702d4.
	RenderLists_Load1P2PGteState(pb);
	return RenderLists_Walk1P2P(bspRoot, visLeafList, pb, LevRenderList, bspList, numPlyr);
}

int RenderLists_Init3P4P(struct BSP *bspRoot, int *visLeafList, struct PushBuffer *pb, void *LevRenderList, void *bspList)
{
#if defined(CTR_NATIVE)
	if (pb && NativeAspect_UsesExpandedVisibility())
		RenderLists_PrepareNativeFrustum(pb);
#endif
	// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80070388-0x80070720.
	RenderLists_Load1P2PGteState(pb);
	return RenderLists_Walk3P4P(bspRoot, visLeafList, pb, LevRenderList, bspList);
}
