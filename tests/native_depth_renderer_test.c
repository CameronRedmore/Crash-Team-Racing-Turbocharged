// Build this optional integration test with CTR_NATIVE_RENDERER_TESTS=ON.
// Reuse the real renderer/parser, shaders and game globals without booting a
// race or loading save states. A desktop OpenGL context is required.
#ifdef NDEBUG
#undef NDEBUG
#endif
#define main ctr_native_game_main
#include "../main.c"
#undef main

static POLY_G3 depthTestPolys[4];
static POLY_FT3 depthTestTexture;
static DR_PSYX_DRAW3D depthTestMarkers[4];

static void DepthTest_Begin(int enabled, int mode)
{
	NativePgxp_EndFrame();
	gNativeDepthBufferEnabled = enabled;
	gNativePgxpMode = mode;
	ClearSplits();
	SetDefDrawEnv(&activeDrawEnv, 0, 0, 320, 240);
	SetDefDispEnv(&activeDispEnv, 0, 0, 320, 240);
	activeDrawEnv.dfe = 1;
	activeDrawEnv.isbg = 1;
	NativeRenderer_BeginScene();
	NativeRenderer_Clear(0, 0, 320, 240, 0, 0, 0);
	memset(depthTestPolys, 0, sizeof(depthTestPolys));
	memset(depthTestMarkers, 0, sizeof(depthTestMarkers));
	NativeDraw3D_BeginFrame();
}

static void DepthTest_Polygon(int index, int channel, const float depth[3], int world, int semiTrans)
{
	POLY_G3 *p = &depthTestPolys[index];
	setPolyG3(p);
	if (semiTrans) setSemiTrans(p, 1);
	p->x0 = 20; p->y0 = 20;
	p->x1 = 300; p->y1 = 20;
	p->x2 = 160; p->y2 = 220;
	(&p->r0)[channel] = (&p->r1)[channel] = (&p->r2)[channel] = 255;
	if (depth != NULL)
	{
		NativePgxp_SetWorldPhase(world);
		VERTTYPE *xy[] = {&p->x0, &p->x1, &p->x2};
		for (int i = 0; i < 3; i++)
		{
			const u32 packed = (u16)xy[i][0] | ((u32)(u16)xy[i][1] << 16);
			NativePgxp_GteProject(xy[i][0], xy[i][1], depth[i], packed);
			NativePgxp_StoreGteSXY(xy[i], 14, packed);
		}
	}
}

static void DepthTest_Draw(int count)
{
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("test polygons", depthTestPolys, sizeof(depthTestPolys));
	for (int i = 0; i < count; i++)
	{
		depthTestPolys[i].tag = CtrGpu_PackOTTag(
			i + 1 < count ? CtrGpu_PrimToOTLink24(&depthTestPolys[i + 1]) : 0xffffff, 6u << 24);
	}
	ParsePrimitivesLinkedList((u32 *)depthTestPolys, 0);
	DrawAllSplits();
}

static void DepthTest_TexturedForeground(u16 texel)
{
	// Exercise zero-texel discard and both STP passes with the real 16-bit
	// VRAM shader. A discarded texel must not hide the blue background.
	RECT16 rect = {640, 256, 1, 1};
	LoadImage(&rect, &texel);
	memset(&depthTestTexture, 0, sizeof(depthTestTexture));
	POLY_FT3 *p = &depthTestTexture;
	setPolyFT3(p);
	setSemiTrans(p, 1);
	p->r0 = p->g0 = p->b0 = 128;
	p->x0 = 20; p->y0 = 20;
	p->x1 = 300; p->y1 = 20;
	p->x2 = 160; p->y2 = 220;
	p->tpage = getTPage(2, 0, 640, 256);
	NativePgxp_SetWorldPhase(1);
	VERTTYPE *xy[] = {&p->x0, &p->x1, &p->x2};
	for (int i = 0; i < 3; i++)
	{
		const u32 packed = (u16)xy[i][0] | ((u32)(u16)xy[i][1] << 16);
		NativePgxp_GteProject(xy[i][0], xy[i][1], 200, packed);
		NativePgxp_StoreGteSXY(xy[i], 14, packed);
	}
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("test texture", p, sizeof(*p));
	NativeGpuLinks_RegisterRangeChecked("test background", depthTestPolys, sizeof(depthTestPolys));
	p->tag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(depthTestPolys), 7u << 24);
	depthTestPolys[0].tag = CtrGpu_PackOTTag(0xffffff, 6u << 24);
	ParsePrimitivesLinkedList((u32 *)p, 0);
	DrawAllSplits();
}

// Native layers use a 320x240 viewport centred like the GTE, so a camera-space
// point (x, y, z) lands at (160 + 160 * x / z, 120 + 160 * y / z).
static int DepthTest_BeginNativeLayer(int mirror)
{
	NativeDraw3DView view;
	memset(&view, 0, sizeof(view));
	view.rotation[0] = view.rotation[4] = view.rotation[8] = 1.0;
	view.projection = 160.0f;
	view.centerX = 160.0f;
	view.centerY = 120.0f;
	view.width = 320.0f;
	view.height = 240.0f;
	view.mirror = mirror;
	const int layer = NativeDraw3D_BeginLayer(&view);
	assert(layer >= 0);
	return layer;
}

static NativeDraw3DVertex DepthTest_NativeVertex(float sx, float sy, float depth, int channel)
{
	NativeDraw3DVertex v;
	memset(&v, 0, sizeof(v));
	v.x = (sx - 160.0f) * depth / 160.0f;
	v.y = (sy - 120.0f) * depth / 160.0f;
	v.z = depth;
	(&v.r)[channel] = 255;
	return v;
}

// Same screen triangle as DepthTest_Polygon, at a uniform depth.
static void DepthTest_NativeTriangle(int layer, int channel, float depth, u8 flags, s8 bias)
{
	NativeDraw3DVertex a = DepthTest_NativeVertex(20, 20, depth, channel);
	NativeDraw3DVertex b = DepthTest_NativeVertex(300, 20, depth, channel);
	NativeDraw3DVertex c = DepthTest_NativeVertex(160, 220, depth, channel);
	NativeDraw3DMaterial material;
	memset(&material, 0, sizeof(material));
	material.flags = flags;
	material.depthBias = bias;
	assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material) == 1);
}

// Links packets in submission order: markers draw native layers, the rest are
// depthTestPolys entries.
static void DepthTest_DrawPackets(void *const *packets, int count)
{
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("test polygons", depthTestPolys, sizeof(depthTestPolys));
	NativeGpuLinks_RegisterRangeChecked("test markers", depthTestMarkers, sizeof(depthTestMarkers));
	for (int i = 0; i < count; i++)
	{
		const u32 length = ((u8 *)packets[i] >= (u8 *)depthTestMarkers && (u8 *)packets[i] < (u8 *)(depthTestMarkers + 4)) ? 1u : 6u;
		CTR_GPU_WriteTagWord(packets[i], CtrGpu_PackOTTag(i + 1 < count ? CtrGpu_PrimToOTLink24(packets[i + 1]) : 0xffffff, length << 24));
	}
	ParsePrimitivesLinkedList((u32 *)packets[0], 0);
	DrawAllSplits();
}

static void DepthTest_Marker(int index, int layer)
{
	NativeDraw3D_EndLayer(layer);
	NativeDraw3D_SetMarker(&depthTestMarkers[index], layer);
}

static void DepthTest_ReadPixel(int x, int y, u8 *pixel)
{
	const struct NativeRenderTarget *resolved = NativeRenderer_ResolveMainRenderTarget();
	x = x * resolved->width / 320;
	y = (240 - y) * resolved->height / 240;
	glBindFramebuffer(GL_FRAMEBUFFER, resolved->framebuffer);
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	NativeRenderer_BindMainRenderTarget();
	assert(glGetError() == GL_NO_ERROR);
}

static void DepthTest_Pixel(int x, int y, int r, int g, int b)
{
	u8 pixel[4];
	// The render target is scaled to the host window. Sample using the same
	// logical coordinates as the PS1 packets, away from triangle boundaries.
	// Read the resolved image so MSAA and SSAA targets are checked as presented.
	const struct NativeRenderTarget *resolved = NativeRenderer_ResolveMainRenderTarget();
	x = x * resolved->width / 320;
	y = (240 - y) * resolved->height / 240;
	glBindFramebuffer(GL_FRAMEBUFFER, resolved->framebuffer);
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	NativeRenderer_BindMainRenderTarget();
	if (abs(pixel[0] - r) > 10 || abs(pixel[1] - g) > 10 || abs(pixel[2] - b) > 10)
	{
		fprintf(stderr, "Unexpected pixel %d,%d: %u,%u,%u; expected %d,%d,%d\n", x, y, pixel[0], pixel[1], pixel[2], r, g, b);
		abort();
	}
	assert(glGetError() == GL_NO_ERROR);
}

// Native 3D layers share the depth buffer and encoding with PGXP geometry.
static void DepthTest_Native(int mode)
{
	const float farDepth[3] = {800, 800, 800};
	const float nearDepth[3] = {200, 200, 200};
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;

	// Native near surface submitted before a far retail surface.
	DepthTest_Begin(1, mode);
	int layer = DepthTest_BeginNativeLayer(0);
	DepthTest_NativeTriangle(layer, 0, 200, 0, 0);
	DepthTest_Marker(0, layer);
	DepthTest_Polygon(0, 2, farDepth, 1, 0);
	{
		void *packets[] = {&depthTestMarkers[0], &depthTestPolys[0]};
		DepthTest_DrawPackets(packets, 2);
	}
	DepthTest_Pixel(160, 100, 248, 0, 0);

	// Retail near surface submitted before a far native surface. The native
	// renderer implies the depth buffer even with the option off.
	for (int depthOption = 0; depthOption < 2; depthOption++)
	{
		DepthTest_Begin(depthOption, mode);
		DepthTest_Polygon(0, 0, nearDepth, 1, 0);
		layer = DepthTest_BeginNativeLayer(0);
		DepthTest_NativeTriangle(layer, 2, 800, 0, 0);
		DepthTest_Marker(0, layer);
		void *packets[] = {&depthTestPolys[0], &depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 2);
		DepthTest_Pixel(160, 100, 248, 0, 0);
	}

	// Translucent native surface blends over a retail surface drawn after it.
	DepthTest_Begin(1, mode);
	layer = DepthTest_BeginNativeLayer(0);
	DepthTest_NativeTriangle(layer, 1, 200, NATIVE_DRAW3D_SEMI_TRANS, 0);
	DepthTest_Marker(0, layer);
	DepthTest_Polygon(0, 2, farDepth, 1, 0);
	{
		void *packets[] = {&depthTestMarkers[0], &depthTestPolys[0]};
		DepthTest_DrawPackets(packets, 2);
	}
	DepthTest_Pixel(160, 100, 0, 124, 124);

	// A triangle reaching behind the camera is clipped by the host at the
	// near plane: plane y = z - 200, crossing the centre ray at depth 200.
	DepthTest_Begin(1, mode);
	DepthTest_Polygon(0, 2, farDepth, 1, 0);
	layer = DepthTest_BeginNativeLayer(0);
	{
		NativeDraw3DVertex a = {-500, 300, 500, 0, 0, 255, 0, 0};
		NativeDraw3DVertex b = {500, 300, 500, 0, 0, 255, 0, 0};
		NativeDraw3DVertex c = {0, -300, -100, 0, 0, 255, 0, 0};
		NativeDraw3DMaterial material = {0, 0, NATIVE_DRAW3D_DOUBLE_SIDED, 0};
		assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material) == 1);
	}
	DepthTest_Marker(0, layer);
	{
		void *packets[] = {&depthTestPolys[0], &depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 2);
	}
	DepthTest_Pixel(160, 120, 248, 0, 0);

	// Mirror mode reflects the screen around the viewport centre.
	DepthTest_Begin(1, mode);
	layer = DepthTest_BeginNativeLayer(1);
	{
		NativeDraw3DVertex a = DepthTest_NativeVertex(20, 20, 300, 0);
		NativeDraw3DVertex b = DepthTest_NativeVertex(150, 20, 300, 0);
		NativeDraw3DVertex c = DepthTest_NativeVertex(20, 220, 300, 0);
		NativeDraw3DMaterial material = {0, 0, 0, 0};
		assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material) == 1);
	}
	DepthTest_Marker(0, layer);
	{
		void *packets[] = {&depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 1);
	}
	DepthTest_Pixel(280, 40, 248, 0, 0);
	DepthTest_Pixel(40, 40, 0, 0, 0);

	// Coplanar surfaces: the later one wins a depth tie, unless retail's
	// draw-order bias puts the earlier one in front.
	for (int bias = 0; bias < 2; bias++)
	{
		DepthTest_Begin(1, mode);
		layer = DepthTest_BeginNativeLayer(0);
		DepthTest_NativeTriangle(layer, 0, 400, 0, bias ? -1 : 0);
		DepthTest_NativeTriangle(layer, 2, 400, 0, 0);
		DepthTest_Marker(0, layer);
		void *packets[] = {&depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 1);
		DepthTest_Pixel(160, 100, bias ? 248 : 0, 0, bias ? 0 : 248);
	}

	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
}

// Tiger Temple's black lower sky uses a -8 byte offset from OT slot 0x3ff.
// With the native level marker at 0x3fe, the sky paints over the level. Exercise
// the actual level-marker helper and DrawSky_Full, rather than a hand-made
// packet order that would silently miss that regression.
static void DepthTest_TigerSkyOrder(int mode, int draw, int skyOffset)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	if (draw) DepthTest_Begin(1, mode);
	else
	{
		NativePgxp_EndFrame();
		gNativePgxpMode = mode;
		NativeDraw3D_BeginFrame();
	}
	Platform_InitScratchpad();
	uint32_t ot[0x400];
	u32 primitiveWords[64] = {0};
	u8 *primitives = (u8 *)primitiveWords;
	struct PushBuffer pb = {0};
	struct PrimMem primMem = {0};
	struct ShortVertex vertices[3] = {0};
	struct SkyboxFace face = {0};
	struct Skybox sky = {0};
	pb.ptrOT = ot;
	pb.rect.w = 320;
	pb.rect.h = 240;
	pb.distanceToScreen_PREV = 160;
	pb.matrix_ViewProj.m[0][0] = 4096;
	pb.matrix_ViewProj.m[1][1] = 4096;
	pb.matrix_ViewProj.m[2][2] = 4096;
	primMem.cursor = primitives;
	primMem.end = primitives + sizeof(primitiveWords);
	primMem.guardEnd = primMem.end;
	for (int i = 0; i < 3; i++)
	{
		const int sx[] = {20, 300, 160};
		const int sy[] = {20, 20, 220};
		vertices[i].Position.vx = (sx[i] - 160) * 10;
		vertices[i].Position.vy = (sy[i] - 120) * 10;
		vertices[i].Position.vz = 1600;
		vertices[i].Color.cd = 0x30;
	}
	face.B = sizeof(vertices[0]);
	face.C = sizeof(vertices[0]) * 2;
	face.D = (u16)skyOffset;
	sky.ptrVertex = vertices;
	// DrawSky chooses four segments; every segment contains the same black
	// triangle so the test also covers cameras selecting adjacent segments.
	for (int i = 0; i < NUM_SKYBOX_SEGMENTS; i++)
	{
		sky.numFaces[i] = 1;
		sky.ptrFaces[i] = &face;
	}
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("test sky OT", ot, sizeof(ot));
	NativeGpuLinks_RegisterRangeChecked("test sky primitives", primitives, sizeof(primitiveWords));
	ClearOTagR(ot, 0x400);
	gte_SetGeomOffset(160, 120);
	gte_SetGeomScreen(160);
	NativePgxp_SetWorldPhase(1);
	DrawSky_Full(&sky, &pb, &primMem);
	assert((u8 *)primMem.cursor > primitives);
	const int layer = DepthTest_BeginNativeLayer(0);
	DepthTest_NativeTriangle(layer, 0, 400, 0, 0);
	NativeDraw3D_EndLayer(layer);
	NativeDrawLevel_LinkLayer(&pb, &primMem, layer, NATIVE_DRAW_LEVEL_MARKER_OT_INDEX);
	int skyCount = 0;
	int markerCount = 0;
	int steps = 0;
	const u32 *packet = &ot[0x3ff];
	while (packet != NULL)
	{
		assert(++steps <= 0x400 + 5);
		const u32 tag = CTR_GPU_ReadTagWord(packet);
		if ((tag >> 24) == 6)
		{
			// All four sky faces must precede the level marker, including
			// Tiger Temple's negative-D faces.
			assert(markerCount == 0);
			skyCount++;
		}
		else if ((tag >> 24) == 1)
		{
			assert((packet[1] >> 24) == 0xb3);
			const NativeDraw3DLayer *nativeLayer = NativeDraw3D_GetLayer((int)(packet[1] & 0xffffff));
			assert(nativeLayer != NULL && nativeLayer->triangleCount > 0);
			const NativeDraw3DTriangle *triangles = NativeDraw3D_GetTriangles() + nativeLayer->firstTriangle;
			if (triangles[0].material.flags & NATIVE_DRAW3D_BACKGROUND)
			{
				assert(markerCount == 0);
				skyCount += (int)nativeLayer->triangleCount;
			}
			else
			{
				assert(skyCount == 4);
				markerCount++;
			}
		}
		if (NativeGpuLinks_IsTerminator(tag & 0xffffff)) break;
		packet = NativeGpuLinks_ToHostPointer(tag & 0xffffff);
	}
	assert(skyCount == 4 && markerCount == 1);
	if (draw)
	{
		ParsePrimitivesLinkedList(&ot[0x3ff], 0);
		DrawAllSplits();
		DepthTest_Pixel(160, 100, 248, 0, 0);
	}
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
}

// Check the model bridge without SDL/GL: packet screen coordinates are
// deliberately bogus, so the assertions require the animation FIFO geometry.
static void DepthTest_ModelSubmission(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	NativeDraw3D_BeginFrame();
	struct RenderBucketDrawContext ctx = {0};
	struct Instance inst = {0};
	struct InstDrawPerPlayer idpp = {0};
	ctx.inst = &inst;
	ctx.idpp = &idpp;
	ctx.nativeLayer = DepthTest_BeginNativeLayer(0);
	const double rotation[9] = {0.25, 0, 0, 0, 0.25, 0, 0, 0, 0.25};
	const double translation[3] = {0, 0, 0};
	NativeDraw3D_SetObjectTransform(ctx.nativeLayer, rotation, translation);
	ctx.tempPacked[1].xy = CTR_PackS16Pair(-400, -200);
	ctx.tempPacked[2].xy = CTR_PackS16Pair(400, -200);
	ctx.tempPacked[3].xy = CTR_PackS16Pair(0, 200);
	for (int i = 1; i <= 3; i++) ctx.tempPacked[i].z = 1600;
	inst.depthBiasNormal = (u8)-2;
	POLY_G3 flat = {0};
	setPolyG3(&flat);
	flat.r0 = 10; flat.g0 = 20; flat.b0 = 30;
	flat.r1 = 40; flat.g1 = 50; flat.b1 = 60;
	flat.r2 = 70; flat.g2 = 80; flat.b2 = 90;
	flat.x0 = flat.y0 = flat.x1 = flat.y1 = flat.x2 = flat.y2 = -1000;
	assert(RenderBucket_SubmitNativePrim(&ctx, 0x10000000, &flat) == 1);
	const NativeDraw3DLayer *layer = NativeDraw3D_GetLayer(ctx.nativeLayer);
	const NativeDraw3DTriangle *triangles = NativeDraw3D_GetTriangles();
	assert(layer->triangleCount == 1);
	assert(triangles[0].position[0][0] == -100 && triangles[0].position[0][1] == -50);
	assert(triangles[0].position[2][1] == 50 && triangles[0].position[2][2] == 400);
	assert(triangles[0].color[1][0] == 40 && triangles[0].color[2][2] == 90);
	assert(triangles[0].material.depthBias == -2);
	assert(triangles[0].material.flags == 0);

	// Strip command winding, instance winding and the start-banner double flip.
	RenderBucket_SubmitNativePrim(&ctx, 0x30000000, &flat);
	assert(layer->triangleCount == 1);
	idpp.instFlags = 0x8000;
	RenderBucket_SubmitNativePrim(&ctx, 0x30000000, &flat);
	assert(layer->triangleCount == 2);
	idpp.instFlags = 0;
	gNativeMirrorModeDoubleFlipActive = 1;
	RenderBucket_SubmitNativePrim(&ctx, 0x30000000, &flat);
	assert(layer->triangleCount == 3);
	gNativeMirrorModeDoubleFlipActive = 0;

	POLY_GT3 textured = {0};
	setPolyGT3(&textured);
	setSemiTrans(&textured, 1);
	textured.tpage = 0x20;
	textured.clut = 0x123;
	textured.u0 = 2; textured.v0 = 3;
	textured.u1 = 4; textured.v1 = 5;
	textured.u2 = 6; textured.v2 = 7;
	textured.r0 = 11; textured.r1 = 22; textured.r2 = 33;
	RenderBucket_SubmitNativePrim(&ctx, 0, &textured);
	assert(layer->triangleCount == 4);
	assert(triangles[3].material.tpage == 0x20 && triangles[3].material.clut == 0x123);
	assert(triangles[3].material.flags == (NATIVE_DRAW3D_TEXTURED | NATIVE_DRAW3D_SEMI_TRANS | NATIVE_DRAW3D_DOUBLE_SIDED));
	assert(triangles[3].uv[1][0] == 4 && triangles[3].uv[2][1] == 7);
	assert(triangles[3].color[1][0] == 22 && triangles[3].color[2][0] == 33);

	// Lit collectible writers emit one colour for all three vertices (FT3).
	POLY_FT3 lit = {0};
	setPolyFT3(&lit);
	lit.r0 = 77; lit.g0 = 88; lit.b0 = 99;
	lit.tpage = 0x60; lit.clut = 0x456;
	lit.u2 = 42; lit.v2 = 43;
	RenderBucket_SubmitNativePrim(&ctx, 0, &lit);
	assert(layer->triangleCount == 5);
	for (int i = 0; i < 3; i++) assert(triangles[4].color[i][0] == 77 && triangles[4].color[i][2] == 99);
	assert(triangles[4].material.tpage == 0x60 && triangles[4].material.clut == 0x456);
	assert(triangles[4].uv[2][0] == 42 && triangles[4].uv[2][1] == 43);

	// A triangle crossing the camera is retained for host near-plane clipping.
	ctx.tempPacked[3].z = (u32)-40;
	RenderBucket_SubmitNativePrim(&ctx, 0, &flat);
	assert(layer->triangleCount == 6 && triangles[5].position[2][2] == -10);
	// Generated split vertices replace the original FIFO geometry and use
	// the interpolated UV/colour record produced by the split writer.
	struct RenderBucketSplitVertex split[3] = {0};
	split[0].xy = CTR_PackS16Pair(-200, -100);
	split[1].xy = CTR_PackS16Pair(200, -100);
	split[2].xy = CTR_PackS16Pair(0, 100);
	for (int i = 0; i < 3; i++) split[i].z = 1600;
	assert(RenderBucket_SubmitNativeSplit(&ctx, 0, &textured, &split[0], &split[1], &split[2]) == 1);
	assert(layer->triangleCount == 7);
	assert(triangles[6].position[0][0] == -50 && triangles[6].position[0][1] == -25);
	assert(triangles[6].position[2][2] == 400 && triangles[6].uv[2][1] == 7);
	ctx.nativeLayer = -1;
	assert(RenderBucket_SubmitNativePrim(&ctx, 0, &flat) == 0);
	assert(layer->triangleCount == 7);

	// A race may queue 2048 instance/viewport entries, exceeding the old 64
	// layer limit. Ensure those entries leave space for the level layers.
	NativeDraw3D_BeginFrame();
	for (int i = 0; i < 2052; i++) assert(DepthTest_BeginNativeLayer(i & 1) == i);
	NativeDraw3D_BeginFrame();
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
	ctx.nativeLayer = 0;
	assert(RenderBucket_SubmitNativePrim(&ctx, 0, &flat) == 0);
}

// Counts red values on a horizontal gradient that the PS1's 5-bit channels
// cannot represent.
static int DepthTest_GradientNon15BitPixels(int colorDepth)
{
	gNativeColorDepth = colorDepth;
	DepthTest_Begin(0, NATIVE_PGXP_MODE_OFF);
	POLY_G3 *p = &depthTestPolys[0];
	setPolyG3(p);
	p->x0 = 20; p->y0 = 20; p->r0 = 0;
	p->x1 = 300; p->y1 = 20; p->r1 = 255;
	p->x2 = 160; p->y2 = 220; p->r2 = 128;
	DepthTest_Draw(1);
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;

	int count = 0;
	for (int x = 60; x < 260; x++)
	{
		u8 pixel[4];
		DepthTest_ReadPixel(x, 40, pixel);
		const int c5 = pixel[0] >> 3;
		count += ((c5 << 3) | (c5 >> 2)) != pixel[0];
	}
	return count;
}

// Count pixels along the slanted left edge of a red triangle that are neither
// background nor fully covered. Anti-aliasing must produce some; Off none.
static int DepthTest_PartialEdgePixels(void)
{
	DepthTest_Begin(0, NATIVE_PGXP_MODE_OFF);
	DepthTest_Polygon(0, 0, NULL, 0, 0);
	DepthTest_Draw(1);

	// The edge crosses logical (90, 120); read a window around it.
	const struct NativeRenderTarget *resolved = NativeRenderer_ResolveMainRenderTarget();
	const int x = 70 * resolved->width / 320;
	const int y = 110 * resolved->height / 240;
	const int w = 40 * resolved->width / 320;
	const int h = 20 * resolved->height / 240;
	u8 *pixels = malloc((size_t)w * h * 4);
	assert(pixels != NULL);
	glBindFramebuffer(GL_FRAMEBUFFER, resolved->framebuffer);
	glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	NativeRenderer_BindMainRenderTarget();
	assert(glGetError() == GL_NO_ERROR);

	int partial = 0;
	for (int i = 0; i < w * h; i++)
	{
		if (pixels[i * 4] > 16 && pixels[i * 4] < 232)
		{
			partial++;
		}
	}
	free(pixels);
	return partial;
}

static void DepthTest_MinimapPrecision(void)
{
	assert(gNativePreciseMinimapEnabled == 0);
	gNativePreciseMinimapEnabled = 1;
	static struct GameTracker tracker;
	struct GameTracker *savedTracker = sdata->gGT;
	sdata->gGT = &tracker;
	tracker.numPlyrCurrGame = 1;
	struct UIMap map = {2000, 4000, 0, 0, 100, 80, 400, 180, 0};
	s32 world[3] = {-123, 0, 456};
	const double expectedX[4] = {-6.15, -11.4, 6.15, 11.4};
	const double expectedY[4] = {18.24, -9.84, -18.24, 9.84};
	for (int mode = 0; mode < 4; mode++)
	{
		map.mode = mode;
		float x, y;
		UI_Map_GetIconPosPrecise(&map, world, &x, &y);
		assert(fabs(x - (400 + expectedX[mode] * 34.0 / 45.0)) < 0.0001);
		assert(fabs(y - (164 + expectedY[mode])) < 0.0001);
		tracker.numPlyrCurrGame = 3;
		float x3, y3;
		UI_Map_GetIconPosPrecise(&map, world, &x3, &y3);
		assert(fabs(x3 - (x - 60)) < 0.0001);
		assert(fabs(y3 - (y + 10)) < 0.0001);
		tracker.numPlyrCurrGame = 1;
	}
	map.mode = 0;
	float x, y, nextX, nextY;
	UI_Map_GetIconPosPrecise(&map, world, &x, &y);
	world[0]++;
	UI_Map_GetIconPosPrecise(&map, world, &nextX, &nextY);
	assert(nextX > x && nextX - x < 0.1f);
	assert(y == nextY);

	POLY_G4 packets[5] = {0};
	u32 ot = 0xffffff;
	tracker.backBuffer = &tracker.db[0];
	tracker.backBuffer->primMem.cursor = packets;
	tracker.backBuffer->primMem.guardEnd = packets + 4;
	tracker.pushBuffer_UI.ptrOT = &ot;
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("minimap packets", packets, sizeof(packets));
	NativeGpuLinks_RegisterRangeChecked("minimap OT", &ot, sizeof(ot));
	NativePgxp_EndFrame();
	gNativePgxpMode = NATIVE_PGXP_MODE_PERSPECTIVE;
	// Even if the previous draw phase was world geometry, HUD markers have
	// no perspective divisor or depth. The real GPU parser retains fractions.
	NativePgxp_SetWorldPhase(1);
	const SVec2 points[3] = {{{0, -8}}, {{-5, 4}}, {{5, 4}}};
	char colors[12] = {0};
	AH_Map_HubArrowPrecise(100.25f, 80.5f, points, colors, 0x800, 0);
	assert(tracker.backBuffer->primMem.cursor == packets + 5);
	for (int i = 0; i < 5; i++)
	{
		NativePgxpVertex precise;
		const u32 packed = (u16)packets[i].x2 | ((u32)(u16)packets[i].y2 << 16);
		assert(NativePgxp_Lookup(&packets[i].x2, packed, &precise));
		assert(fabs(precise.x - (106.25f + D232.hubArrowPrimOffset[i].x)) < 0.0001);
		assert(fabs(precise.y - (80.5f + D232.hubArrowPrimOffset[i].y)) < 0.0001);
		assert(precise.w == NATIVE_PGXP_SCREEN_W && precise.depth == 0);
		GrVertex vertices[4];
		MakeVertexQuad(vertices, &packets[i].x0, &packets[i].x1, &packets[i].x2, &packets[i].x3);
		assert(fabs(vertices[2].x - precise.x) < 0.0001);
		assert(fabs(vertices[2].y - precise.y) < 0.0001);
	}
	// The minimap toggle works with 3D PGXP and world depth both off.
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	gNativeDepthBufferEnabled = 0;
	// Race dots use the same projection, with the existing sprite shape.
	struct { struct IconGroup group; struct Icon *icons[1]; } group = {0};
	struct Icon icon = {0};
	group.icons[0] = &icon;
	icon.texLayout.u1 = 4;
	icon.texLayout.v2 = 4;
	tracker.iconGroup[UI_MAP_ICON_GROUP] = &group.group;
	u32 colors4[4] = {0};
	u32 *savedColor = data.ptrColor[0];
	data.ptrColor[0] = colors4;
	POLY_GT4 dots[2] = {0};
	NativeGpuLinks_RegisterRangeChecked("minimap dots", dots, sizeof(dots));
	tracker.backBuffer->primMem.cursor = dots;
	UI_Map_DrawRawIcon(&map, world, 0, 0, 0, 0x1000);
	world[0]++;
	UI_Map_DrawRawIcon(&map, world, 0, 0, 0, 0x1000);
	GrVertex first[4], second[4];
	MakeVertexQuad(first, &dots[0].x0, &dots[0].x1, &dots[0].x2, &dots[0].x3);
	MakeVertexQuad(second, &dots[1].x0, &dots[1].x1, &dots[1].x2, &dots[1].x3);
	assert(dots[0].x0 == dots[1].x0);
	for (int i = 0; i < 4; i++)
	{
		assert(fabs(second[i].x - first[i].x - 0.05 * 34.0 / 45.0) < 0.0001);
		assert(second[i].y == first[i].y);
	}
	// Disabling the enhancement returns to the exact integer drawing path.
	NativePgxp_EndFrame();
	gNativePreciseMinimapEnabled = 0;
	tracker.backBuffer->primMem.cursor = dots;
	UI_Map_DrawRawIcon(&map, world, 0, 0, 0, 0x1000);
	MakeVertexQuad(first, &dots[0].x0, &dots[0].x1, &dots[0].x2, &dots[0].x3);
	assert(first[0].x == dots[0].x0 && first[0].y == dots[0].y0);
	NativePgxpVertex rounded;
	const u32 roundedXY = (u16)dots[0].x0 | ((u32)(u16)dots[0].y0 << 16);
	assert(!NativePgxp_Lookup(&dots[0].x0, roundedXY, &rounded));
	data.ptrColor[0] = savedColor;
	NativePgxp_EndFrame();
	NativePgxpVertex expired;
	const u32 packed = (u16)dots[0].x0 | ((u32)(u16)dots[0].y0 << 16);
	assert(!NativePgxp_Lookup(&dots[0].x0, packed, &expired));
	sdata->gGT = savedTracker;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--minimap-only") == 0)
	{
		DepthTest_MinimapPrecision();
		puts("Minimap precision checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--model-submission-only") == 0)
	{
		DepthTest_ModelSubmission();
		puts("Native model submission checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--sky-order-only") == 0)
	{
		for (int mirror = 0; mirror < 2; mirror++)
		{
			gNativeMirrorModeRenderActive = mirror;
			for (int mode = 0; mode < NATIVE_PGXP_MODE_COUNT; mode++)
				for (int offset = 0; offset >= -8; offset -= 4)
					DepthTest_TigerSkyOrder(mode, 0, offset);
		}
		puts("Tiger Temple sky ordering checks passed");
		return 0;
	}
	if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
	gNativeDitheringEnabled = 0;
	gNativeBorderlessEnabled = 0;
	if (!NativeRenderer_InitialiseRender("CTR depth test", 320, 240, 0)) return 77;
	SDL_HideWindow(g_window);
	assert(NativeRenderer_InitialisePSX());
	const float nearDepth[3] = {200, 200, 200};
	const float farDepth[3] = {800, 800, 800};

	// 15-bit colour quantises to PS1 framebuffer precision; true colour keeps
	// the full gradient. Resolving anti-aliasing would average, so AA is off.
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	if (DepthTest_GradientNon15BitPixels(NATIVE_COLOR_DEPTH_15BIT) != 0 || DepthTest_GradientNon15BitPixels(NATIVE_COLOR_DEPTH_TRUE) == 0)
	{
		fprintf(stderr, "%s\n", "15-bit colour output is not quantised as expected");
		abort();
	}

	// FXAA only applies when presenting to the window, so it is not covered here.
	const int aaModes[] = {NATIVE_AA_OFF, NATIVE_AA_MSAA_2X, NATIVE_AA_MSAA_4X, NATIVE_AA_MSAA_8X, NATIVE_AA_SSAA_2X, NATIVE_AA_SSAA_4X};
	for (u32 aa = 0; aa < sizeof(aaModes) / sizeof(aaModes[0]); aa++)
	{
		gNativeAntiAliasingMode = aaModes[aa];
		const int partial = DepthTest_PartialEdgePixels();
		if ((aaModes[aa] == NATIVE_AA_OFF) != (partial == 0))
		{
			fprintf(stderr, "AA mode %d: %d partially covered edge pixels\n", aaModes[aa], partial);
			abort();
		}
		if (aaModes[aa] == NATIVE_AA_MSAA_2X || aaModes[aa] == NATIVE_AA_MSAA_4X || aaModes[aa] == NATIVE_AA_MSAA_8X)
		{
			assert(s_mainRenderTarget.samples > 1);
		}
		for (int mode = 0; mode < NATIVE_PGXP_MODE_COUNT; mode++)
		{
			// Inverted painter order: a distant polygon submitted last must not
			// cover a nearer surface. Disabling the option restores retail order.
			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 0, nearDepth, 1, 0);
			DepthTest_Polygon(1, 2, farDepth, 1, 0);
			DepthTest_Draw(2);
			DepthTest_Pixel(160, 100, 248, 0, 0);
			DepthTest_Begin(0, mode);
			DepthTest_Polygon(0, 0, nearDepth, 1, 0);
			DepthTest_Polygon(1, 2, farDepth, 1, 0);
			DepthTest_Draw(2);
			DepthTest_Pixel(160, 100, 0, 0, 248);

			// Two intersecting triangles need different winners across the same
			// overlap. A single polygon sorting key cannot satisfy both pixels.
			const float sloped[3] = {100, 900, 100};
			const float middle[3] = {350, 350, 350};
			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 0, sloped, 1, 0);
			DepthTest_Polygon(1, 2, middle, 1, 0);
			DepthTest_Draw(2);
			DepthTest_Pixel(60, 40, 248, 0, 0);
			DepthTest_Pixel(270, 40, 0, 0, 248);

			// Transparent foreground submitted before opaque background still
			// blends over it, and does not block a later transparent surface.
			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 1, nearDepth, 1, 1);
			DepthTest_Polygon(1, 2, farDepth, 1, 0);
			DepthTest_Draw(2);
			DepthTest_Pixel(160, 100, 0, 124, 124);

			// A nearer transparent surface must not write depth and reject the
			// farther transparent polygon submitted after it in retail OT order.
			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 0, nearDepth, 1, 1);
			DepthTest_Polygon(1, 1, middle, 1, 1);
			DepthTest_Polygon(2, 2, farDepth, 1, 0);
			DepthTest_Draw(3);
			DepthTest_Pixel(160, 100, 62, 124, 62);

			const u16 texels[] = {0, 0x03e0, 0x83e0};
			const int expected[][3] = {{0, 0, 248}, {0, 248, 0}, {0, 124, 124}};
			for (int texel = 0; texel < 3; texel++)
			{
				DepthTest_Begin(1, mode);
				DepthTest_Polygon(0, 2, farDepth, 1, 0);
				DepthTest_TexturedForeground(texels[texel]);
				DepthTest_Pixel(160, 100, expected[texel][0], expected[texel][1], expected[texel][2]);
			}

			// A HUD projection can carry PGXP precision without carrying world
			// depth. It must remain on top, including when it shares draw state.
			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 0, nearDepth, 1, 0);
			DepthTest_Polygon(1, 1, farDepth, 0, 0);
			DepthTest_Draw(2);
			DepthTest_Pixel(160, 100, 0, 248, 0);

			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 0, nearDepth, 1, 0);
			DepthTest_Polygon(1, 1, farDepth, 1, 0);
			NativePgxp_CopyXY(&depthTestPolys[1].x2, NULL, (u16)depthTestPolys[1].x2 | ((u32)(u16)depthTestPolys[1].y2 << 16));
			DepthTest_Draw(2);
			DepthTest_Pixel(160, 100, 0, 248, 0);

			// Unknown/partially recovered polygons safely retain painter order.
			DepthTest_Begin(1, mode);
			DepthTest_Polygon(0, 0, nearDepth, 1, 0);
			DepthTest_Polygon(1, 1, NULL, 1, 0);
			DepthTest_Draw(2);
			DepthTest_Pixel(160, 100, 0, 248, 0);

			DepthTest_Native(mode);
			for (int offset = 0; offset >= -8; offset -= 4)
				DepthTest_TigerSkyOrder(mode, 1, offset);
		}
	}
	NativeRenderer_Shutdown();
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	puts("Native depth renderer checks passed");
	return 0;
}
