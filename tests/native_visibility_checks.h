// Compare the visibility gates with the actual native camera, rather than
// comparing two implementations of the same retail planes.
static void VisibilityTest_Camera(void)
{
	struct GameTracker tracker = {0};
	sdata->gGT = &tracker;
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	gNativeProjectionMode = NATIVE_PROJECTION_PERSPECTIVE;
	Platform_InitScratchpad();
	unsigned randomState = 12345;
	int visible = 0, planeRejected = 0, boundsRejected = 0, legacyBoundsRejected = 0;
	const int positions[] = {0, 12000, 32000, -32000};
	const int fovs[] = {0, 90, 100};
	for (int aspect = NATIVE_ASPECT_21_9; aspect <= NATIVE_ASPECT_32_9; aspect++)
		for (int fov = 0; fov < 3; fov++)
			for (int position = 0; position < 4; position++)
				for (int angle = 0; angle < 8; angle++)
				{
					gNativeAspectRatio = aspect;
					gNativeFovDegrees = fovs[fov];
					struct PushBuffer *pb = &tracker.pushBuffer[0];
					memset(pb, 0, sizeof(*pb));
					pb->rect.w = 512;
					pb->rect.h = 216;
					pb->distanceToScreen_PREV = 256;
					pb->pos.x = positions[position];
					pb->pos.z = positions[position];
					pb->rot.y = angle * 512;
					pb->rot.x = (angle & 1) ? 160 : -160;
					PushBuffer_UpdateFrustum(pb);
					RenderLists_PrepareNativeFrustum(pb);
					NativeDraw3DView view;
					NativeDrawLevel_BuildView(pb, &view);
					for (int sample = 0; sample < 4000; sample++)
					{
						int point[3];
						for (int axis = 0; axis < 3; axis++)
						{
							randomState = randomState * 1664525u + 1013904223u;
							point[axis] = (int)((randomState >> 16) % 65520) - 32760;
						}
						double camera[3];
						for (int row = 0; row < 3; row++)
							camera[row] = view.translation[row] + view.rotation[row * 3] * point[0] + view.rotation[row * 3 + 1] * point[1] +
							              view.rotation[row * 3 + 2] * point[2];
						if (camera[2] <= 32 || fabs(camera[0] * view.projection) >= (view.width * 0.5 - 4) * camera[2] ||
						    fabs(camera[1] * view.projection) >= (view.height * 0.5 - 4) * camera[2])
							continue;
						struct BoundingBox box;
						for (int axis = 0; axis < 3; axis++)
						{
							box.min.v[axis] = point[axis] - 4;
							box.max.v[axis] = point[axis] + 4;
						}
						visible++;
						planeRejected += !RenderLists_BoxPassesFrustum(pb, &box);
						boundsRejected += !RenderLists_BoxPassesSpatialBounds(pb, &box);
						legacyBoundsRejected += !RenderLists_BoxOverlapsPushBuffer(&box, &pb->bbox);
					}
				}
	printf("Actual-camera visibility: %d visible boxes, %d plane rejects, %d spatial rejects (%d legacy bounds rejects)\n", visible, planeRejected,
	       boundsRejected, legacyBoundsRejected);
	fflush(stdout);
	assert(visible > 10000);
	assert(planeRejected == 0);
	assert(boundsRejected == 0);
	sdata->gGT = NULL;
}

// Conservative expanded-view culling must not reintroduce retail PVS/LOD gates.
static void VisibilityTest_Expanded(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAspectRatio = NATIVE_ASPECT_16_9;
	gNativeFovDegrees = 90;
	Platform_InitScratchpad();
	struct PushBuffer pb = {0};
	pb.rect.w = 512;
	pb.rect.h = 216;
	pb.distanceToScreen_PREV = 256;
	pb.matrix_ViewProj.m[0][0] = pb.matrix_ViewProj.m[1][1] = pb.matrix_ViewProj.m[2][2] = 4096;
	pb.bbox.min = (SVec3){.x = -32767, .y = -32767, .z = -32767};
	pb.bbox.max = (SVec3){.x = 32767, .y = 32767, .z = 32767};
	RenderLists_PrepareNativeFrustum(&pb);
	struct BoundingBox crossing = {.min = {.x = 995, .y = -20, .z = 1000}, .max = {.x = 1020, .y = 20, .z = 1000}};
	struct BoundingBox outside = {.min = {.x = 1010, .y = -20, .z = 1000}, .max = {.x = 1050, .y = 20, .z = 1000}};
	struct BoundingBox behind = {.min = {.x = -10, .y = -20, .z = -1000}, .max = {.x = 10, .y = 20, .z = -900}};
	assert(RenderLists_BoxPassesFrustum(&pb, &crossing));
	assert(!RenderLists_BoxPassesFrustum(&pb, &outside));
	assert(!RenderLists_BoxPassesFrustum(&pb, &behind));
	struct BSP tree[3] = {0};
	tree[0].data.branch.childID[0] = BSP_CHILD_ID_LEAF_FLAG | 1;
	tree[0].data.branch.childID[1] = BSP_CHILD_ID_LEAF_FLAG | 2;
	tree[1].flag = tree[2].flag = BSP_NODE_FLAG_LEAF;
	tree[1].box = (struct BoundingBox){.min = {.x = -10, .y = -10, .z = 9000}, .max = {.x = 10, .y = 10, .z = 9100}};
	tree[2].box = (struct BoundingBox){.min = {.x = 1100, .y = -10, .z = 100}, .max = {.x = 1200, .y = 10, .z = 200}};
	struct DrawLevelOvr1PRenderList list = {0};
	struct VisMemBspListNode nodes[3] = {0};
	for (int i = 0; i < 3; i++)
		nodes[i].bsp = &tree[i];
	int hidden[1] = {0};
	// The distant leaf is still visible, despite the camera-cell mask being
	// empty and its distance exceeding the old H*26 recovery cutoff.
	assert(RenderLists_Init1P2P(tree, hidden, &pb, (u32)&list, nodes, 1) == 1);
	assert(list.bspListStart_FullDynamic == &nodes[1]);
	memset(&list, 0, sizeof(list));
	assert(RenderLists_Init1P2P(tree, hidden, &pb, (u32)&list, nodes, 2) == 1);
	memset(&list, 0, sizeof(list));
	assert(RenderLists_Init3P4P(tree, hidden, &pb, (u32)&list, nodes) == 1);
	// Retail framing continues to honour an empty mask.
	gNativeFovDegrees = 0;
	memset(&list, 0, sizeof(list));
	assert(RenderLists_Init1P2P(tree, hidden, &pb, (u32)&list, nodes, 1) == 0);
}
