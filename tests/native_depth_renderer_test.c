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
	if (semiTrans)
		setSemiTrans(p, 1);
	p->x0 = 20;
	p->y0 = 20;
	p->x1 = 300;
	p->y1 = 20;
	p->x2 = 160;
	p->y2 = 220;
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
		depthTestPolys[i].tag = CtrGpu_PackOTTag(i + 1 < count ? CtrGpu_PrimToOTLink24(&depthTestPolys[i + 1]) : 0xffffff, 6u << 24);
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
	p->x0 = 20;
	p->y0 = 20;
	p->x1 = 300;
	p->y1 = 20;
	p->x2 = 160;
	p->y2 = 220;
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

// A multi-material HUD model must keep its private depth on every split,
// regardless of world depth behind it, and leave scene depth intact afterwards.
static void DepthTest_HudDepth(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
	gNativeDitheringEnabled = 0;
	const int modes[] = {NATIVE_AA_OFF, NATIVE_AA_MSAA_4X, NATIVE_AA_SSAA_4X};
	u16 blue = 31 << 10;
	RECT16 tex = {640, 256, 1, 1};
	LoadImage(&tex, &blue);
	for (int aa = 0; aa < 3; aa++)
		for (int gpu = 0; gpu < 2; gpu++)
		{
			gNativeGpuTransformEnabled = gpu;
			gNativeAntiAliasingMode = modes[aa];
			DepthTest_Begin(1, NATIVE_PGXP_MODE_PERSPECTIVE);
			activeDispEnv.disp.y = gpu ? 296 : 0;
			activeDrawEnv.clip.y = activeDrawEnv.ofs[1] = activeDispEnv.disp.y;
			int world = DepthTest_BeginNativeLayer(0);
			DepthTest_NativeTriangle(world, 0, 200, NATIVE_DRAW3D_DOUBLE_SIDED, 0);
			DepthTest_Marker(0, world);
			int hud = DepthTest_BeginNativeLayer(0);
			NativeDraw3DMaterial m = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED | NATIVE_DRAW3D_OVERLAY};
			NativeDraw3DVertex a = DepthTest_NativeVertex(80, 60, 800, 1);
			NativeDraw3DVertex b = DepthTest_NativeVertex(140, 60, 800, 1);
			NativeDraw3DVertex c = DepthTest_NativeVertex(110, 140, 800, 1);
			assert(NativeDraw3D_AddTriangle(hud, &a, &b, &c, &m));
			m.flags |= NATIVE_DRAW3D_TEXTURED;
			m.tpage = getTPage(2, 0, 640, 256);
			a = DepthTest_NativeVertex(180, 60, 700, 0);
			b = DepthTest_NativeVertex(240, 60, 700, 0);
			c = DepthTest_NativeVertex(210, 140, 700, 0);
			a.r = a.g = a.b = b.r = b.g = b.b = c.r = c.g = c.b = 128;
			assert(NativeDraw3D_AddTriangle(hud, &a, &b, &c, &m));
			DepthTest_Marker(1, hud);
			world = DepthTest_BeginNativeLayer(0);
			DepthTest_NativeTriangle(world, 1, 400, NATIVE_DRAW3D_DOUBLE_SIDED, 0);
			DepthTest_Marker(2, world);
			void *packets[] = {&depthTestMarkers[0], &depthTestMarkers[1], &depthTestMarkers[2]};
			DepthTest_DrawPackets(packets, 3);
			DepthTest_Pixel(110, 90, 0, 248, 0);
			DepthTest_Pixel(210, 90, 0, 0, 248);
			DepthTest_Pixel(160, 180, 248, 0, 0);
			// HUD work covers under a quarter of this target, even with SSAA.
			assert((s_isolatedDepth.right - s_isolatedDepth.x) * (s_isolatedDepth.top - s_isolatedDepth.y) <
			       s_mainRenderTarget.width * s_mainRenderTarget.height / 4);
			NativeRenderer_EndScene();
		}
	gNativeGpuTransformEnabled = 1;
}

// Compare the entire resolved image through the actual split scheduler,
// including CPU/GPU interleaving, mutable transforms, culling and near clipping.
static void DepthTest_GpuParity(void)
{
	u8 *images[2] = {NULL, NULL};
	u32 pixelCount = 0;
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	u16 texels[64];
	for (int i = 0; i < 64; i++)
		texels[i] = (u16)(1 + i % 31) | (u16)(1 + i / 8) << 5 | (u16)(i % 8) << 10;
	RECT16 rect = {640, 256, 8, 8};
	LoadImage(&rect, texels);
	for (int mirror = 0; mirror < 2; mirror++)
		for (int variant = 0; variant < 9; variant++)
		{
			for (int gpu = 0; gpu < 2; gpu++)
			{
				gNativeGpuTransformEnabled = gpu;
				DepthTest_Begin(1, NATIVE_PGXP_MODE_PERSPECTIVE);
				const int layer = DepthTest_BeginNativeLayer(mirror);
				NativeDraw3DMaterial m = {0};
				m.flags = variant == 1 ? NATIVE_DRAW3D_DOUBLE_SIDED : variant == 2 ? NATIVE_DRAW3D_REVERSE_WINDING : 0;
				m.depthBias = variant == 3 ? -10 : 0;
				m.screenOffsetX = variant == 3 ? 7 : 0;
				g_cfg_bilinearFiltering = variant == 7;
				gNativeDitheringEnabled = variant == 8;
				gNativeColorDepth = variant == 8 ? NATIVE_COLOR_DEPTH_15BIT : NATIVE_COLOR_DEPTH_TRUE;
				if (variant >= 6)
				{
					m.flags = NATIVE_DRAW3D_TEXTURED | NATIVE_DRAW3D_DITHER;
					m.tpage = getTPage(2, 0, 640, 256);
				}
				NativeDraw3DVertex a = DepthTest_NativeVertex(30, 40, 256, 0);
				NativeDraw3DVertex b = DepthTest_NativeVertex(140, 40, 256, 0);
				NativeDraw3DVertex c = DepthTest_NativeVertex(80, 180, variant == 4 ? 16 : 256, 0);
				if (variant >= 6)
				{
					a.r = a.g = a.b = b.r = b.g = b.b = c.r = c.g = c.b = 128;
					a.u = a.v = b.v = c.u = 0;
					b.u = c.v = 7;
				}
				// Reverse front convention, or exercise reflected object transforms.
				if (variant == 1 || variant == 2)
				{
					NativeDraw3DVertex tmp = b;
					b = c;
					c = tmp;
				}
				double rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
				double translation[3] = {0, 0, 0};
				if (variant == 5)
				{
					rotation[0] = -1;
					m.flags = NATIVE_DRAW3D_REVERSE_WINDING;
				}
				NativeDraw3D_SetObjectTransform(layer, rotation, translation);
				NativeDraw3D_AddTriangle(layer, &a, &b, &c, &m);
				translation[0] = variant == 5 ? -160 : 160;
				NativeDraw3D_SetObjectTransform(layer, rotation, translation);
				a.r = b.r = c.r = 0;
				a.g = b.g = c.g = 255;
				NativeDraw3D_AddTriangle(layer, &a, &b, &c, &m);
				// A transparent layer and a legacy HUD packet follow GPU draws.
				DepthTest_Marker(0, layer);
				int blend = DepthTest_BeginNativeLayer(mirror);
				DepthTest_NativeTriangle(blend, 2, 512, NATIVE_DRAW3D_SEMI_TRANS, 0);
				DepthTest_Marker(1, blend);
				DepthTest_Polygon(0, 0, NULL, 0, 0);
				depthTestPolys[0].x0 = depthTestPolys[0].x1 = 4;
				depthTestPolys[0].y0 = depthTestPolys[0].y2 = 4;
				depthTestPolys[0].x2 = 14;
				depthTestPolys[0].y1 = 14;
				void *packets[] = {&depthTestMarkers[0], &depthTestMarkers[1], &depthTestPolys[0]};
				DepthTest_DrawPackets(packets, 3);
				const struct NativeRenderTarget *resolved = NativeRenderer_ResolveMainRenderTarget();
				pixelCount = (u32)resolved->width * resolved->height;
				images[gpu] = realloc(images[gpu], pixelCount * 4);
				assert(images[gpu]);
				glBindFramebuffer(GL_FRAMEBUFFER, resolved->framebuffer);
				glReadPixels(0, 0, resolved->width, resolved->height, GL_RGBA, GL_UNSIGNED_BYTE, images[gpu]);
				NativeRenderer_BindMainRenderTarget();
				assert(!glIsEnabled(GL_CULL_FACE));
				assert(glGetError() == GL_NO_ERROR);
			}
			u32 differing = 0, lit = 0;
			for (u32 i = 0; i < pixelCount; i++)
			{
				lit += images[0][4 * i] || images[0][4 * i + 1] || images[0][4 * i + 2];
				differing += memcmp(images[0] + 4 * i, images[1] + 4 * i, 4) != 0;
			}
			if (differing > pixelCount / 5000 + 8)
				fprintf(stderr, "GPU parity mirror=%d variant=%d: %u differing pixels\n", mirror, variant, differing);
			assert(lit > 100 && differing <= pixelCount / 5000 + 8);
		}
	free(images[0]);
	free(images[1]);
	g_cfg_bilinearFiltering = gNativeDitheringEnabled = 0;
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
	gNativeGpuTransformEnabled = 1;
}

// Static geometry must render exactly like the same triangles submitted through
// the dynamic GPU path, with the layer's object transform, every cull class,
// baked depth bias, textures, dithering and bilinear offsets, and must rebuild
// its vertices when the bilinear state they bake changes between frames.
static void DepthTest_StaticTriangle(NativeDraw3DTriangle *t, const NativeDraw3DVertex *a, const NativeDraw3DVertex *b, const NativeDraw3DVertex *c,
                                     const NativeDraw3DMaterial *m)
{
	const NativeDraw3DVertex *v[3] = {a, b, c};
	memset(t, 0, sizeof(*t));
	for (int i = 0; i < 3; i++)
	{
		t->position[i][0] = v[i]->x;
		t->position[i][1] = v[i]->y;
		t->position[i][2] = v[i]->z;
		t->uv[i][0] = v[i]->u;
		t->uv[i][1] = v[i]->v;
		t->color[i][0] = v[i]->r;
		t->color[i][1] = v[i]->g;
		t->color[i][2] = v[i]->b;
	}
	t->material = *m;
	t->transformIndex = 1;
}

static void DepthTest_StaticParity(void)
{
	u8 *images[2] = {NULL, NULL};
	u32 pixelCount = 0;
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	gNativeGpuTransformEnabled = 1;
	u16 texels[64];
	for (int i = 0; i < 64; i++)
		texels[i] = (u16)(1 + i % 31) | (u16)(1 + i / 8) << 5 | (u16)(i % 8) << 10;
	RECT16 rect = {640, 256, 8, 8};
	LoadImage(&rect, texels);
	for (int mirror = 0; mirror < 2; mirror++)
		for (int variant = 0; variant < 8; variant++)
		{
			NativeDraw3DMaterial m = {0};
			m.flags = variant == 1 ? NATIVE_DRAW3D_DOUBLE_SIDED : variant == 2 ? NATIVE_DRAW3D_REVERSE_WINDING : 0;
			m.depthBias = variant == 3 ? -10 : 0;
			if (variant >= 4)
			{
				m.flags = NATIVE_DRAW3D_TEXTURED | (variant == 7 ? NATIVE_DRAW3D_SUPER_TURBO_TINT : 0);
				m.tpage = getTPage(2, 0, 640, 256);
			}
			NativeDraw3DVertex a = DepthTest_NativeVertex(30, 40, 256, 0);
			NativeDraw3DVertex b = DepthTest_NativeVertex(140, 40, 256, 0);
			NativeDraw3DVertex c = DepthTest_NativeVertex(80, 180, 300, 0);
			if (variant >= 4)
			{
				a.r = a.g = a.b = b.r = b.g = b.b = c.r = c.g = c.b = 128;
				a.u = a.v = b.v = c.u = 0;
				b.u = c.v = 7;
			}
			if (variant == 1 || variant == 2)
			{
				NativeDraw3DVertex tmp = b;
				b = c;
				c = tmp;
			}
			// A back face that only double-sided geometry keeps.
			NativeDraw3DVertex d = DepthTest_NativeVertex(200, 40, 256, 1), e = DepthTest_NativeVertex(250, 180, 256, 1),
			                   f = DepthTest_NativeVertex(300, 40, 256, 1);
			NativeDraw3DMaterial back = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED};
			const double rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
			const double translation[3] = {12, -8, 0};
			const int passes = variant == 5 ? 3 : 2;
			for (int pass = 0; pass < passes; pass++)
			{
				const int useStatic = pass > 0;
				// Variant 5 draws a static frame first with the opposite bilinear state.
				g_cfg_bilinearFiltering = (variant == 5) ? (pass != 1) : 0;
				gNativeDitheringEnabled = variant == 6;
				gNativeColorDepth = variant == 6 ? NATIVE_COLOR_DEPTH_15BIT : NATIVE_COLOR_DEPTH_TRUE;
				if (variant == 6)
					m.flags |= NATIVE_DRAW3D_DITHER;
				DepthTest_Begin(1, NATIVE_PGXP_MODE_PERSPECTIVE);
				activeDrawEnv.dtd = variant == 6;
				const int layer = DepthTest_BeginNativeLayer(mirror);
				NativeDraw3D_SetObjectTransform(layer, rotation, translation);
				if (useStatic)
				{
					// Later passes reuse the uploaded geometry.
					if (pass == 1)
					{
						NativeDraw3DTriangle *triangles = malloc(sizeof(*triangles) * 2);
						assert(triangles);
						DepthTest_StaticTriangle(&triangles[0], &d, &e, &f, &back);
						DepthTest_StaticTriangle(&triangles[1], &a, &b, &c, &m);
						NativeGpu_SetStaticGeometry(triangles, 2);
					}
					const NativeDraw3DStaticRange ranges[2] = {{0, 1, NativeDraw3D_StaticBucket(&back)}, {1, 1, NativeDraw3D_StaticBucket(&m)}};
					assert(ranges[1].bucket != NATIVE_DRAW3D_STATIC_NONE);
					assert(NativeDraw3D_AddStaticRanges(layer, ranges, 2));
				}
				else
				{
					NativeDraw3D_AddTriangle(layer, &d, &e, &f, &back);
					NativeDraw3D_AddTriangle(layer, &a, &b, &c, &m);
				}
				// CPU and transparent geometry share the frame.
				NativeDraw3DMaterial pushed = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED, .depthSlots = 1};
				NativeDraw3DVertex g = DepthTest_NativeVertex(20, 200, 600, 2), h = DepthTest_NativeVertex(60, 200, 600, 2),
				                   k = DepthTest_NativeVertex(40, 230, 600, 2);
				NativeDraw3D_AddTriangle(layer, &g, &h, &k, &pushed);
				DepthTest_Marker(0, layer);
				int blend = DepthTest_BeginNativeLayer(mirror);
				DepthTest_NativeTriangle(blend, 2, 512, NATIVE_DRAW3D_SEMI_TRANS, 0);
				DepthTest_Marker(1, blend);
				void *packets[] = {&depthTestMarkers[0], &depthTestMarkers[1]};
				DepthTest_DrawPackets(packets, 2);
				if (useStatic && pass + 1 < passes)
				{
					NativeRenderer_EndScene();
					continue;
				}
				const struct NativeRenderTarget *resolved = NativeRenderer_ResolveMainRenderTarget();
				pixelCount = (u32)resolved->width * resolved->height;
				images[useStatic] = realloc(images[useStatic], pixelCount * 4);
				assert(images[useStatic]);
				glBindFramebuffer(GL_FRAMEBUFFER, resolved->framebuffer);
				glReadPixels(0, 0, resolved->width, resolved->height, GL_RGBA, GL_UNSIGNED_BYTE, images[useStatic]);
				NativeRenderer_BindMainRenderTarget();
				assert(!glIsEnabled(GL_CULL_FACE));
				assert(glGetError() == GL_NO_ERROR);
				NativeRenderer_EndScene();
			}
			u32 differing = 0, lit = 0;
			for (u32 i = 0; i < pixelCount; i++)
			{
				lit += images[0][4 * i] || images[0][4 * i + 1] || images[0][4 * i + 2];
				differing += memcmp(images[0] + 4 * i, images[1] + 4 * i, 4) != 0;
			}
			if (differing != 0)
				fprintf(stderr, "Static parity mirror=%d variant=%d: %u differing pixels\n", mirror, variant, differing);
			assert(lit > 100 && differing == 0);
		}
	NativeGpu_SetStaticGeometry(NULL, 0);
	free(images[0]);
	free(images[1]);
	g_cfg_bilinearFiltering = gNativeDitheringEnabled = 0;
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
}

static void DepthTest_GpuBenchmark(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	for (int gpu = 0; gpu < 2; gpu++)
	{
		gNativeGpuTransformEnabled = gpu;
		u64 begin = 0;
		for (int frame = 0; frame < 240; frame++)
		{
			if (frame == 40)
				begin = SDL_GetPerformanceCounter();
			DepthTest_Begin(1, NATIVE_PGXP_MODE_PERSPECTIVE);
			const int layer = DepthTest_BeginNativeLayer(0);
			NativeDraw3DMaterial m = {0};
			for (int i = 0; i < 30000; i++)
			{
				const float x = (float)(i % 100) * 3 + 4, y = (float)((i / 100) % 70) * 3 + 4;
				NativeDraw3DVertex a = DepthTest_NativeVertex(x, y, 256, 0);
				NativeDraw3DVertex b = DepthTest_NativeVertex(x + 2, y, 256, 0);
				NativeDraw3DVertex c = DepthTest_NativeVertex(x, y + 2, 256, 0);
				assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &m));
			}
			DepthTest_Marker(0, layer);
			void *packets[] = {&depthTestMarkers[0]};
			DepthTest_DrawPackets(packets, 1);
			glFinish();
			u32 vertices, uploads, gt, ct;
			NativeRenderer_GetUploadCounts(&vertices, &uploads);
			NativeDraw3D_GetGeometryCounts(&gt, &ct);
			assert(vertices == 90000 && uploads == 1);
			assert(gt == (gpu ? 30000u : 0u) && gt + ct == 30000);
			assert(glGetError() == GL_NO_ERROR);
		}
		const double ms = (SDL_GetPerformanceCounter() - begin) * 1000.0 / SDL_GetPerformanceFrequency() / 200;
		printf("30000 opaque triangles: %s %.3f ms/frame (including GPU completion)\n", gpu ? "GPU" : "CPU", ms);
	}
	gNativeGpuTransformEnabled = 1;
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
		NativeDraw3DMaterial material = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED};
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
		NativeDraw3DMaterial material = {0};
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

	// Pushed-back OT slots hide a surface behind one up to that distance
	// further away (hub tunnel mouths poking through walls), and not beyond.
	const float slotDepth = NativeDraw3D_GetDrawOrderSlotDepth();
	for (int slots = 0; slots < 3; slots++)
	{
		DepthTest_Begin(1, mode);
		layer = DepthTest_BeginNativeLayer(0);
		DepthTest_NativeTriangle(layer, 2, 400 + 1.5f * slotDepth, 0, 0);
		{
			NativeDraw3DVertex a = DepthTest_NativeVertex(20, 20, 400, 0);
			NativeDraw3DVertex b = DepthTest_NativeVertex(300, 20, 400, 0);
			NativeDraw3DVertex c = DepthTest_NativeVertex(160, 220, 400, 0);
			NativeDraw3DMaterial material = {.depthSlots = (u8)slots};
			assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material) == 1);
		}
		DepthTest_Marker(0, layer);
		void *packets[] = {&depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 1);
		// Two slots move the near surface behind the far one; one does not.
		DepthTest_Pixel(160, 100, slots == 2 ? 0 : 248, 0, slots == 2 ? 248 : 0);
	}

	// Push-back keeps clip depth affine, so a pushed triangle reaching behind
	// the camera still clips at the near plane and stays in front of depth 800.
	DepthTest_Begin(1, mode);
	DepthTest_Polygon(0, 2, farDepth, 1, 0);
	layer = DepthTest_BeginNativeLayer(0);
	{
		NativeDraw3DVertex a = {-500, 300, 500, 0, 0, 255, 0, 0};
		NativeDraw3DVertex b = {500, 300, 500, 0, 0, 255, 0, 0};
		NativeDraw3DVertex c = {0, -300, -100, 0, 0, 255, 0, 0};
		NativeDraw3DMaterial material = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED, .depthSlots = 1};
		assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material) == 1);
	}
	DepthTest_Marker(0, layer);
	{
		void *packets[] = {&depthTestPolys[0], &depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 2);
	}
	DepthTest_Pixel(160, 120, 248, 0, 0);

	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
}

// Tiger Temple's black lower sky uses a -8 byte offset from OT slot 0x3ff.
// With the native level marker at 0x3fe, the sky paints over the level. Exercise
// the actual level-marker helper and DrawSky_Full, rather than a hand-made
// packet order that would silently miss that regression.
static void DepthTest_TigerSkyOrder(int mode, int draw, int skyOffset)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	if (draw)
		DepthTest_Begin(1, mode);
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
		if (NativeGpuLinks_IsTerminator(tag & 0xffffff))
			break;
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
	for (int i = 1; i <= 3; i++)
		ctx.tempPacked[i].z = 1600;
	inst.depthBiasNormal = (u8)-2;
	POLY_G3 flat = {0};
	setPolyG3(&flat);
	flat.r0 = 10;
	flat.g0 = 20;
	flat.b0 = 30;
	flat.r1 = 40;
	flat.g1 = 50;
	flat.b1 = 60;
	flat.r2 = 70;
	flat.g2 = 80;
	flat.b2 = 90;
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
	textured.u0 = 2;
	textured.v0 = 3;
	textured.u1 = 4;
	textured.v1 = 5;
	textured.u2 = 6;
	textured.v2 = 7;
	textured.r0 = 11;
	textured.r1 = 22;
	textured.r2 = 33;
	RenderBucket_SubmitNativePrim(&ctx, 0, &textured);
	assert(layer->triangleCount == 4);
	assert(triangles[3].material.tpage == 0x20 && triangles[3].material.clut == 0x123);
	assert(triangles[3].material.flags == (NATIVE_DRAW3D_TEXTURED | NATIVE_DRAW3D_SEMI_TRANS | NATIVE_DRAW3D_DOUBLE_SIDED));
	assert(triangles[3].uv[1][0] == 4 && triangles[3].uv[2][1] == 7);
	assert(triangles[3].color[1][0] == 22 && triangles[3].color[2][0] == 33);

	// Lit collectible writers emit one colour for all three vertices (FT3).
	POLY_FT3 lit = {0};
	setPolyFT3(&lit);
	lit.r0 = 77;
	lit.g0 = 88;
	lit.b0 = 99;
	lit.tpage = 0x60;
	lit.clut = 0x456;
	lit.u2 = 42;
	lit.v2 = 43;
	RenderBucket_SubmitNativePrim(&ctx, 0, &lit);
	assert(layer->triangleCount == 5);
	for (int i = 0; i < 3; i++)
		assert(triangles[4].color[i][0] == 77 && triangles[4].color[i][2] == 99);
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
	for (int i = 0; i < 3; i++)
		split[i].z = 1600;
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
	for (int i = 0; i < 2052; i++)
		assert(DepthTest_BeginNativeLayer(i & 1) == i);
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
	p->x0 = 20;
	p->y0 = 20;
	p->r0 = 0;
	p->x1 = 300;
	p->y1 = 20;
	p->r1 = 255;
	p->x2 = 160;
	p->y2 = 220;
	p->r2 = 128;
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
	char colors[16] = {0};
	tracker.backBuffer->primMem.end = packets + 5;
	AH_Map_HubArrowPrecise(100.25f, 80.5f, points, colors, 0x800, 0, 1.0f);
	// One fill triangle, then a one-pixel mitred outline submitted behind it.
	assert(tracker.backBuffer->primMem.cursor == packets + 2);
	const float fillX[3] = {106.25f, 102.25f, 110.25f};
	const float fillY[3] = {80.5f, 86.5f, 86.5f};
	VERTTYPE *fillCorners[3] = {&packets[0].x0, &packets[0].x1, &packets[0].x3};
	VERTTYPE *outlineCorners[3] = {&packets[1].x0, &packets[1].x1, &packets[1].x3};
	for (int i = 0; i < 3; i++)
	{
		NativePgxpVertex precise;
		u32 packed = (u16)fillCorners[i][0] | ((u32)(u16)fillCorners[i][1] << 16);
		assert(NativePgxp_Lookup(fillCorners[i], packed, &precise));
		assert(fabs(precise.x - fillX[i]) < 0.0001 && fabs(precise.y - fillY[i]) < 0.0001);
		assert(precise.w == NATIVE_PGXP_SCREEN_W && precise.depth == 0);

		NativePgxpVertex outline;
		packed = (u16)outlineCorners[i][0] | ((u32)(u16)outlineCorners[i][1] << 16);
		assert(NativePgxp_Lookup(outlineCorners[i], packed, &outline));
		const float centreX = (fillX[0] + fillX[1] + fillX[2]) / 3.0f;
		const float centreY = (fillY[0] + fillY[1] + fillY[2]) / 3.0f;
		const float fillDistance = hypotf(precise.x - centreX, precise.y - centreY);
		assert(hypotf(outline.x - centreX, outline.y - centreY) > fillDistance + 0.99f);
	}
	GrVertex vertices[4];
	MakeVertexQuad(vertices, &packets[0].x0, &packets[0].x1, &packets[0].x2, &packets[0].x3);
	assert(fabs(vertices[0].x - 106.25f) < 0.0001 && fabs(vertices[3].y - 86.5f) < 0.0001);
	// The player arrow keeps its smaller size.
	NativePgxp_EndFrame();
	tracker.backBuffer->primMem.cursor = packets;
	AH_Map_HubArrowPrecise(100.25f, 80.5f, points, colors, 0x800, 0, 0.6f);
	{
		NativePgxpVertex precise;
		const u32 packed = (u16)packets[0].x1 | ((u32)(u16)packets[0].y1 << 16);
		assert(NativePgxp_Lookup(&packets[0].x1, packed, &precise));
		assert(fabs(precise.x - (106.25f - 4.0f * 0.6f)) < 0.0001);
	}
	// The save/load marker quad keeps fractions and gets square mitred corners.
	NativePgxp_EndFrame();
	tracker.backBuffer->primMem.cursor = packets;
	AH_Map_LoadSavePrecise(50.5f, 40.25f, &D232.loadSavePos[0], colors, 0x800, 0);
	assert(tracker.backBuffer->primMem.cursor == packets + 2);
	{
		const float quadX[4] = {53.3f, 59.7f, 53.3f, 59.7f};
		const float quadY[4] = {43.25f, 43.25f, 45.25f, 45.25f};
		const float outward[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
		for (int q = 0; q < 2; q++)
		{
			VERTTYPE *corners[4] = {&packets[q].x0, &packets[q].x1, &packets[q].x2, &packets[q].x3};
			for (int i = 0; i < 4; i++)
			{
				NativePgxpVertex precise;
				const u32 packed = (u16)corners[i][0] | ((u32)(u16)corners[i][1] << 16);
				assert(NativePgxp_Lookup(corners[i], packed, &precise));
				assert(fabs(precise.x - (quadX[i] + q * outward[i][0])) < 0.0001);
				assert(fabs(precise.y - (quadY[i] + q * outward[i][1])) < 0.0001);
			}
		}
	}
	// The minimap toggle works with 3D PGXP and world depth both off.
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	gNativeDepthBufferEnabled = 0;
	// Race dots use the same projection, with the existing sprite shape.
	struct
	{
		struct IconGroup group;
		struct Icon *icons[1];
	} group = {0};
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
	// Modern Map replaces the sprites with vector shapes centred on
	// the world position. They keep fractions with Precise Minimap Off.
	gNativeModernMapEnabled = 1;
	gNativePreciseMinimapEnabled = 0;
	assert(NATIVE_VERTEX_TRACKING_ACTIVE());
	static POLY_G3 shapes[96];
	NativeGpuLinks_RegisterRangeChecked("marker shapes", shapes, sizeof(shapes));
	tracker.backBuffer->primMem.guardEnd = shapes + 95;
	const float aspectX = (512.0f / 216.0f) / (4.0f / 3.0f) * 34.0f / 45.0f;
	const u32 markerColors[4] = {0xff, 0xff, 0x80, 0x80};
	for (int star = 0; star < 2; star++)
	{
		NativePgxp_EndFrame();
		tracker.backBuffer->primMem.cursor = shapes;
		AH_Map_MarkerShape(100.25f, 80.5f, star ? 3.7f : 2.1f, star, markerColors);
		const int rim = star ? 10 : 20;
		// A fill fan, then a black outline fan behind it.
		assert(tracker.backBuffer->primMem.cursor == shapes + 2 * rim);
		const float radius = star ? 3.7f : 2.1f;
		const float outline = radius + (star ? 1.7f : 1.0f);
		for (int i = 0; i < 2 * rim; i++)
		{
			GrVertex tri[3];
			MakeVertexTriangle(tri, &shapes[i].x0, &shapes[i].x1, &shapes[i].x2);
			assert(fabsf(tri[0].x - 100.25f) < 0.0001f && fabsf(tri[0].y - 80.5f) < 0.0001f);
			if ((i % rim) == 0)
			{
				// The first rim point is straight up.
				assert(fabsf(tri[1].x - 100.25f) < 0.0001f);
				assert(fabsf(tri[1].y - (80.5f - ((i < rim) ? radius : outline))) < 0.0001f);
			}
			if ((i % rim) == rim / 4 && !star)
			{
				// A quarter turn is scaled for the HUD pixel aspect.
				assert(fabsf(tri[1].x - (100.25f + ((i < rim) ? radius : outline) * aspectX)) < 0.001f);
			}
		}
		// The fill centre is lightened towards white; the outline is black.
		assert(shapes[0].r0 == 217 && shapes[0].g0 == 102 && shapes[0].b0 == 102);
		assert(shapes[rim].r0 == 0 && shapes[rim].g1 == 0 && shapes[rim].b2 == 0);
	}
	// Modern hub pulses are smaller rings, centred on the marker.
	for (int frame = 0; frame < 64; frame++)
	{
		NativePgxp_EndFrame();
		tracker.backBuffer->primMem.cursor = shapes;
		tracker.timer = frame;
		AH_Map_MarkerPulse(0, 100.25f, 80.5f, false);
		for (POLY_G3 *tri = shapes; tri < (POLY_G3 *)tracker.backBuffer->primMem.cursor; tri++)
		{
			GrVertex v[3];
			MakeVertexTriangle(v, &tri->x0, &tri->x1, &tri->x2);
			for (int i = 0; i < 3; i++)
			{
				const float distance = hypotf((v[i].x - 100.25f) / aspectX, v[i].y - 80.5f);
				assert(distance > 3.4f - 0.61f && distance < 10.2f + 0.61f);
			}
		}
	}
	// Race target rings sit between radius +- half width.
	NativePgxp_EndFrame();
	tracker.backBuffer->primMem.cursor = shapes;
	AH_Map_MarkerOutline(100.25f, 80.5f, 4.6f, 0.5f, false, 0xff);
	assert(tracker.backBuffer->primMem.cursor == shapes + 40);
	for (int i = 0; i < 40; i++)
	{
		GrVertex v[3];
		MakeVertexTriangle(v, &shapes[i].x0, &shapes[i].x1, &shapes[i].x2);
		for (int k = 0; k < 3; k++)
		{
			const float distance = hypotf((v[k].x - 100.25f) / aspectX, v[k].y - 80.5f);
			assert(fabsf(distance - 4.1f) < 0.0001f || fabsf(distance - 5.1f) < 0.0001f);
		}
	}
	// Flicker blends start on the first colour and reach the second half a period later.
	tracker.timer = 0;
	u32 flash[4];
	AH_Map_MarkerFlash(flash, CRASH_BLUE, CORTEX_RED, 2.0f);
	assert(flash[0] == data.ptrColor[CRASH_BLUE][0]);
	tracker.timer = 1;
	AH_Map_MarkerFlash(flash, CRASH_BLUE, CORTEX_RED, 2.0f);
	assert(flash[0] == data.ptrColor[CORTEX_RED][0]);
	tracker.timer = 0;
	// The Adventure arrow pivots on the marker itself, as a vector arrow.
	NativePgxp_EndFrame();
	tracker.backBuffer->primMem.cursor = packets;
	tracker.backBuffer->primMem.guardEnd = packets + 4;
	UI_Map_DrawAdvPlayer(&map, world, 0, 0, 0, 0x800);
	{
		float markerX, markerY;
		UI_Map_GetIconPosPrecise(&map, world, &markerX, &markerY);
		NativePgxpVertex tip;
		const u32 packed = (u16)packets[0].x0 | ((u32)(u16)packets[0].y0 << 16);
		assert(NativePgxp_Lookup(&packets[0].x0, packed, &tip));
		const float tipY = data.playerIconAdvMap.pos[0].y * 0x800 * 0.6f / 4096.0f;
		assert(fabsf(tip.x - markerX) < 0.0001f && fabsf(tip.y - (markerY + tipY)) < 0.0001f);
	}
	gNativeModernMapEnabled = 0;

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

// Validate ground coverage separately from GL: a nine-vertex patch, the
// triangular-block special case, collision exclusions, and invalid indices.
static void DepthTest_CollisionMinimap(void)
{
	struct LevVertex vertices[9] = {0};
	const s16 x[9] = {0, 100, 0, 100, 50, 0, 50, 100, 50};
	const s16 z[9] = {0, 0, 100, 100, 0, 50, 50, 50, 100};
	for (int i = 0; i < 9; i++)
	{
		vertices[i].pos.x = x[i];
		vertices[i].pos.z = z[i];
	}
	struct QuadBlock blocks[2] = {0};
	for (int i = 0; i < 9; i++)
		blocks[0].index[i] = blocks[1].index[i] = i;
	blocks[0].quadFlags = QUADBLOCK_FLAG_GROUND;
	blocks[1].quadFlags = QUADBLOCK_FLAG_GROUND | QUADBLOCK_FLAG_KILL_PLANE;
	struct mesh_info mesh = {.numQuadBlock = 2, .numVertex = 9, .ptrQuadBlockArray = blocks, .ptrVertexArray = vertices};
	struct UIMap map = {100, 100, 0, 0, 20, 10, 400, 180, 0};
	struct NativeMinimapImage image = {0};
	for (int mode = 0; mode < 4; mode++)
	{
		map.mode = mode;
		u8 *pixels = NativeMinimap_BuildPixels(&mesh, &map, &image);
		assert(pixels && image.pixelWidth == 208 && image.pixelHeight == 208);
		int ground = 0;
		for (int i = 0; i < image.pixelWidth * image.pixelHeight; i++)
			ground += pixels[4 * i] != 0;
		assert(ground == 160 * 160);
		assert(pixels[3] == 0); // Padding stays transparent.
		free(pixels);
	}
	map.mode = 0;
	blocks[0].index[3] = blocks[0].index[2];
	u8 *pixels = NativeMinimap_BuildPixels(&mesh, &map, &image);
	assert(pixels);
	int ground = 0;
	for (int i = 0; i < image.pixelWidth * image.pixelHeight; i++)
		ground += pixels[4 * i] != 0;
	assert(ground >= 12700 && ground <= 12900); // Only the first half of the grid.
	free(pixels);
	blocks[0].quadFlags |= QUADBLOCK_FLAG_NO_COLLISION_RESPONSE;
	assert(!NativeMinimap_BuildPixels(&mesh, &map, &image));
	blocks[0].quadFlags = QUADBLOCK_FLAG_GROUND | QUADBLOCK_FLAG_TRIGGER;
	assert(!NativeMinimap_BuildPixels(&mesh, &map, &image));
	blocks[0].quadFlags = QUADBLOCK_FLAG_GROUND;
	blocks[0].index[6] = 9;
	assert(!NativeMinimap_BuildPixels(&mesh, &map, &image));
	blocks[0].index[6] = 6;
	map.worldEndX = map.worldStartX;
	assert(!NativeMinimap_BuildPixels(&mesh, &map, &image));
	// The switch must bypass generation and disk reads entirely when disabled.
	gNativeModernMapEnabled = 0;
	assert(!NativeMinimap_DrawLive(NULL, NULL, 1));
	assert(!NativeMinimap_DrawPreview(CRASH_COVE, 0, 0, 100, 100, NULL, NULL, 1));
}

static const u8 *DepthTest_MapSample(const u8 *pixels, const struct NativeMinimapImage *image, float x, float y)
{
	const int sx = (int)((x - image->left) * image->pixelWidth / image->width);
	const int sy = (int)((y - image->top) * image->pixelHeight / image->height);
	assert(sx >= 0 && sy >= 0 && sx < image->pixelWidth && sy < image->pixelHeight);
	return pixels + 4 * (sy * image->pixelWidth + sx);
}

static void DepthTest_MinimapCrossingsAndCache(void)
{
	struct LevVertex vertices[18] = {0};
	const s16 x[9] = {0, 100, 0, 100, 50, 0, 50, 100, 50};
	const s16 z[9] = {0, 0, 100, 100, 0, 50, 50, 50, 100};
	struct QuadBlock blocks[2] = {0};
	for (int i = 0; i < 9; i++)
	{
		vertices[i].pos.x = x[i];
		vertices[i].pos.z = z[i];
		vertices[i + 9].pos.x = 40 + x[i] / 5;
		vertices[i + 9].pos.z = z[i];
		vertices[i + 9].pos.y = 200;
		blocks[0].index[i] = i;
		blocks[1].index[i] = i + 9;
	}
	blocks[0].quadFlags = blocks[1].quadFlags = QUADBLOCK_FLAG_GROUND;
	struct mesh_info mesh = {.numQuadBlock = 2, .numVertex = 18, .ptrQuadBlockArray = blocks, .ptrVertexArray = vertices};
	struct UIMap map = {100, 100, 0, 0, 20, 10, 0, 0, 0};
	struct NativeMinimapImage image = {0};
	u8 *pixels = NativeMinimap_BuildPixels(&mesh, &map, &image);
	assert(pixels);
	// Upper road at X=40..60 is continuous; its two side edges survive the
	// overlapping lower floor. The lower route remains visible on both sides.
	assert(DepthTest_MapSample(pixels, &image, 10, 10)[0] == 255);
	assert(DepthTest_MapSample(pixels, &image, 8.1f, 10)[0] < 32);
	assert(DepthTest_MapSample(pixels, &image, 11.9f, 10)[0] < 32);
	assert(DepthTest_MapSample(pixels, &image, 7, 10)[0] >= 192);
	assert(DepthTest_MapSample(pixels, &image, 13, 10)[0] >= 192);
	int fractional = 0;
	for (int i = 0; i < image.pixelWidth * image.pixelHeight; i++)
		fractional += pixels[4 * i + 3] > 0 && pixels[4 * i + 3] < 255;
	assert(fractional > 0);

	char oldBase[1024];
	snprintf(oldBase, sizeof(oldBase), "%s", NativeAssets_GetBaseDir());
	assert(SDL_CreateDirectory("/tmp/ctr-minimap-cache-fixture"));
	assert(NativeAssets_Init("/tmp/ctr-minimap-cache-fixture"));
	const u64 key = NativeMinimap_GeometryKey(&mesh, &map);
	char name[48], path[1024];
	snprintf(name, sizeof(name), "%016llx.map", (unsigned long long)key);
	assert(NativeMinimap_CachePath(name, path, 0));
	SDL_RemovePath(path);
	struct NativeMinimapImage loaded = {0};
	// A drawing-time cache miss returns immediately without generating.
	assert(!NativeMinimap_GetPixels(&mesh, &map, &loaded, 0));
	assert(NativeMinimap_SaveCache(key, &image, pixels));
	u8 *copy = NativeMinimap_GetPixels(&mesh, &map, &loaded, 0);
	assert(copy && loaded.pixelWidth == image.pixelWidth && loaded.pixelHeight == image.pixelHeight);
	assert(memcmp(copy, pixels, (size_t)image.pixelWidth * image.pixelHeight * 4) == 0);
	free(copy);
	NativeMinimap_WriteIndex(0, 1, 42, key);
	assert(NativeMinimap_ReadIndex(0, 1, 42) == key && NativeMinimap_ReadIndex(0, 1, 43) == 0);
	map.iconStartX = 100;
	map.iconStartY = 200;
	assert(NativeMinimap_GeometryKey(&mesh, &map) == key);
	vertices[9].pos.y++;
	assert(NativeMinimap_GeometryKey(&mesh, &map) != key);
	vertices[9].pos.y--;
	FILE *file = fopen(path, "wb"); // Interrupted/corrupt cache must be rejected.
	assert(file);
	fputs("bad", file);
	fclose(file);
	assert(!NativeMinimap_LoadCache(key, &loaded));
	SDL_RemovePath(path);
	assert(NativeAssets_Init(oldBase));
	free(pixels);

	// A steep but continuous slope has no bridge outlines.
	mesh.numQuadBlock = 1;
	for (int i = 0; i < 9; i++)
		vertices[i].pos.y = vertices[i].pos.x * 3;
	pixels = NativeMinimap_BuildPixels(&mesh, &map, &image);
	assert(pixels);
	assert(DepthTest_MapSample(pixels, &image, 10, 10)[0] >= 192);
	free(pixels);
}

static void DepthTest_MinimapAssets(const char *baseDir)
{
	assert(NativeAssets_Init(baseDir));
	for (int level = 0; level <= CITADEL_CITY; level++)
	{
		struct NativeMinimapImage image = {0};
		u8 *pixels = NativeMinimap_ReadLevelPixels(level, LOAD_LEVEL_LOD_1P, &image, 1);
		// Battle arenas have no retail map metadata; their absence is supported.
		if (level >= NITRO_COURT && level <= LAB_BASEMENT && !pixels)
			continue;
		assert(pixels && image.pixelWidth <= 1024 && image.pixelHeight <= 1024);
		printf("Collision map %d: %dx%d, HUD %.0fx%.0f\n", level, image.pixelWidth, image.pixelHeight, image.width, image.height);
		char path[100];
		snprintf(path, sizeof(path), "/tmp/ctr-minimap-%02d.ppm", level);
		FILE *file = fopen(path, "wb");
		assert(file);
		fprintf(file, "P6\n%d %d\n255\n", image.pixelWidth, image.pixelHeight);
		for (int i = 0; i < image.pixelWidth * image.pixelHeight; i++)
		{
			if (pixels[4 * i + 3])
				fwrite(pixels + 4 * i, 1, 3, file);
			else
			{
				const u8 background[3] = {40, 60, 80};
				fwrite(background, 1, 3, file);
			}
		}
		fclose(file);
		free(pixels);
	}
}

static void DepthTest_SaveHudPreview(const char *path)
{
	const struct NativeRenderTarget *resolved = NativeRenderer_ResolveMainRenderTarget();
	u8 *pixels = malloc((size_t)resolved->width * resolved->height * 4);
	assert(pixels);
	glBindFramebuffer(GL_FRAMEBUFFER, resolved->framebuffer);
	glReadPixels(0, 0, resolved->width, resolved->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	FILE *preview = fopen(path, "wb");
	assert(preview);
	fprintf(preview, "P6\n%d %d\n255\n", resolved->width, resolved->height);
	for (int y = resolved->height - 1; y >= 0; y--)
		for (int x = 0; x < resolved->width; x++)
			fwrite(pixels + (y * resolved->width + x) * 4, 1, 3, preview);
	fclose(preview);
	free(pixels);
	NativeRenderer_BindMainRenderTarget();
}

static void DepthTest_HudIcons(void)
{
	static POLY_G4 packets[HUD_LIGHT_MAX_QUADS * 12];
	struct Icon icon = {0};
	icon.texLayout.u1 = 56;
	icon.texLayout.v2 = 56;
	struct PrimMem memory = {0};
	memory.cursor = packets;
	memory.end = packets + HUD_LIGHT_MAX_QUADS * 12;
	u32 ot = 0xffffff;
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("HUD icons", packets, sizeof(packets));
	NativePgxp_SetPrimRegion(packets, sizeof(packets));
	gNativeModernMapEnabled = gNativePreciseMinimapEnabled = 0;
	gNativeModernHudIconsEnabled = 0;
	assert(!NativeHudIcons_DrawButton('*', &icon, 20, 20, FP(1.0), &memory, &ot));
	assert(memory.cursor == packets && ot == 0xffffff);
	gNativeModernHudIconsEnabled = 1;
	memory.end = packets + 1;
	assert(!NativeHudIcons_DrawLight(&icon, 20, 20, FP(1.0), 0, 1, &memory, &ot));
	assert(memory.cursor == packets && ot == 0xffffff);
	memory.end = packets + HUD_LIGHT_MAX_QUADS * 12;
	assert(!NativeHudIcons_DrawButton('A', &icon, 20, 20, FP(1.0), &memory, &ot));
	DepthTest_Begin(0, NATIVE_PGXP_MODE_OFF);
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
	const char buttons[4] = {'*', '@', '[', '^'};
	for (int i = 0; i < 4; i++)
	{
		assert(NativeHudIcons_DrawButton(buttons[i], &icon, 20 + 72 * i, 20, FP(1.0), &memory, &ot));
		assert(NativeHudIcons_DrawLight(&icon, 20 + 72 * i, 94, FP(1.0), i == 3, 0, &memory, &ot));
		assert(NativeHudIcons_DrawLight(&icon, 20 + 72 * i, 168, FP(1.0), i == 3, 1, &memory, &ot));
	}
	ParsePrimitivesLinkedList(NativeGpuLinks_ToHostPointer(ot), 0);
	DrawAllSplits();
	u8 pixel[4];
	DepthTest_ReadPixel(48, 48, pixel);
	assert(pixel[2] > pixel[0] + 70); // Blue cross is in front of the grey face.
	DepthTest_ReadPixel(48, 196, pixel);
	assert(pixel[0] > pixel[1] + 70); // Lit red lens.
	DepthTest_ReadPixel(264, 196, pixel);
	assert(pixel[1] > pixel[0] + 70); // Lit green lens.
	// The housing dressing occupies space outside the circular lens.
	DepthTest_ReadPixel(28, 122, pixel);
	assert(pixel[2] > pixel[0] + 20); // Left blue steel bracket.
	DepthTest_ReadPixel(68, 130, pixel);
	assert(pixel[2] > pixel[0] + 20); // Right bracket.
	DepthTest_ReadPixel(48, 145, pixel);
	assert(pixel[2] > pixel[0] + 20); // Bottom mounting foot.
	DepthTest_ReadPixel(48, 122, pixel);
	const int unlitRed = pixel[0];
	DepthTest_ReadPixel(48, 196, pixel);
	assert(pixel[0] > unlitRed + 40); // Countdown state changes the lens glow.
	DepthTest_SaveHudPreview("/tmp/ctr-hud-icons.ppm");
	gNativeModernHudIconsEnabled = 0;
	NativePgxp_SetPrimRegion(NULL, 0);
}

static void DepthTest_MenuArrows(void)
{
	static struct GameTracker tracker;
	static struct
	{
		struct IconGroup group;
		struct Icon *icons[0x39];
	} font;
	static POLY_G4 packets[64];
	struct Icon icon = {0};
	icon.texLayout.u1 = 56;
	icon.texLayout.v2 = 40;
	font.group.numIcons = 0x39;
	font.icons[0x38] = &icon;
	tracker.iconGroup[4] = &font.group;
	struct GameTracker *previous = sdata->gGT;
	sdata->gGT = &tracker;
	struct PrimMem memory = {0};
	memory.cursor = packets;
	memory.end = packets + 64;
	u32 ot = 0xffffff;
	NativeGpuLinks_Reset();
	NativeGpuLinks_RegisterRangeChecked("menu arrow packets", packets, sizeof(packets));
	NativePgxp_SetPrimRegion(packets, sizeof(packets));
	gNativeModernHudIconsEnabled = 0;
	DecalHUD_Arrow2D(&icon, 70, 70, &memory, &ot, 0x00b0ff, 0x00b0ff, 0x0018ff, 0x0018ff, 0, FP(1.0), 0);
	assert((u8 *)memory.cursor == (u8 *)packets + sizeof(POLY_GT4)); // Toggle Off keeps the bitmap.
	memory.cursor = packets;
	ot = 0xffffff;
	gNativeModernHudIconsEnabled = 1;
	struct Icon otherIcon = icon;
	DecalHUD_Arrow2D(&otherIcon, 70, 70, &memory, &ot, 0, 0, 0, 0, 0, FP(1.0), 0);
	assert((u8 *)memory.cursor == (u8 *)packets + sizeof(POLY_GT4)); // Other decals stay untouched.
	memory.cursor = packets;
	ot = 0xffffff;
	DepthTest_Begin(0, NATIVE_PGXP_MODE_OFF);
	const int cx[4] = {70, 240, 70, 240};
	const int cy[4] = {70, 70, 170, 170};
	const int rotations[4] = {0x800, 0, 0x400, 0xc00};
	for (int i = 0; i < 4; i++)
	{
		DecalHUD_Arrow2D(&icon, cx[i], cy[i], &memory, &ot, 0x00b0ff, 0x00b0ff, 0x0018ff, 0x0018ff, 0, FP(1.0), rotations[i]);
		assert(memory.cursor == packets + 14 * (i + 1));
	}
	ParsePrimitivesLinkedList(NativeGpuLinks_ToHostPointer(ot), 0);
	DrawAllSplits();
	for (int i = 0; i < 4; i++)
	{
		u8 pixel[4];
		DepthTest_ReadPixel(cx[i], cy[i], pixel);
		assert(pixel[0] > 200 && pixel[1] > 50 && pixel[2] < 10);
	}
	DepthTest_SaveHudPreview("/tmp/ctr-menu-arrows.ppm");
	gNativeModernHudIconsEnabled = 0;
	NativePgxp_SetPrimRegion(NULL, 0);
	sdata->gGT = previous;
}

static void EngineTest_Physics(void)
{
	struct GameTracker tracker = {0};
	tracker.numPlyrCurrGame = 1;
	sdata->gGT = &tracker;
	sdata->gameProgress.unlockFlags |= UNLOCK_PENTA;
	NativeEngine_ClearReplayOverrides();
	const int donors[NATIVE_ENGINE_COUNT] = {CRASH_BANDICOOT, COCO_BANDICOOT, TINY_TIGER, POLAR, PENTA_PENGUIN};
	struct Driver reference[NATIVE_ENGINE_COUNT];
	for (int profile = 0; profile < NATIVE_ENGINE_COUNT; profile++)
	{
		memset(&reference[profile], 0, sizeof(struct Driver));
		gNativeEngineSelectionEnabled = 0;
		data.characterIDs[0] = donors[profile];
		VehBirth_SetConsts(&reference[profile]);
		gNativeEngineSelectionEnabled = 1;
		data.characterIDs[0] = CRASH_BANDICOOT;
		NativeEngine_SetSelectedProfile(0, profile);
		struct Driver selected = {0};
		VehBirth_SetConsts(&selected);
		assert(data.characterIDs[0] == CRASH_BANDICOOT);
		for (u32 i = 0; i < len(data.metaPhys); i++)
		{
			struct MetaPhys *entry = &data.metaPhys[i];
			assert(memcmp((u8 *)&selected + entry->offset, (u8 *)&reference[profile] + entry->offset, entry->size) == 0);
		}
	}
	// Balanced must remain distinct from Penta's separate PAL profile.
	b32 balancedDiffersFromPenta = false;
	for (u32 i = 0; i < len(data.metaPhys); i++)
	{
		struct MetaPhys *entry = &data.metaPhys[i];
		if (memcmp((u8 *)&reference[NATIVE_ENGINE_BALANCED] + entry->offset, (u8 *)&reference[NATIVE_ENGINE_PENTA] + entry->offset, entry->size) != 0)
			balancedDiffersFromPenta = true;
	}
	assert(balancedDiffersFromPenta);
	for (int stat = 0; stat < 3; stat++)
	{
		assert(s_nativeCharacterSelectStatTargets[NATIVE_ENGINE_BALANCED][stat] == 0x37);
		assert(s_nativeCharacterSelectStatTargets[NATIVE_ENGINE_PENTA][stat] == 0x50);
	}
	assert(*(s16 *)((u8 *)&reference[NATIVE_ENGINE_BALANCED] + ACCEL_CLASS_STAT_OFFSET) == 480);
	assert(*(s16 *)((u8 *)&reference[NATIVE_ENGINE_BALANCED] + SPEED_CLASS_STAT_OFFSET) == 13140);
	assert(*((u8 *)&reference[NATIVE_ENGINE_BALANCED] + TURN_RATE_OFFSET) == 28);
	assert(*(s16 *)((u8 *)&reference[NATIVE_ENGINE_PENTA] + ACCEL_CLASS_STAT_OFFSET) == 544);
	assert(*(s16 *)((u8 *)&reference[NATIVE_ENGINE_PENTA] + SPEED_CLASS_STAT_OFFSET) == 13900);
	assert(*((u8 *)&reference[NATIVE_ENGINE_PENTA] + TURN_RATE_OFFSET) == 30);
	data.characterIDs[0] = PENTA_PENGUIN;
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_SPEED);
	struct Driver pentaWithSpeed = {0};
	VehBirth_SetConsts(&pentaWithSpeed);
	for (u32 i = 0; i < len(data.metaPhys); i++)
	{
		struct MetaPhys *entry = &data.metaPhys[i];
		assert(memcmp((u8 *)&pentaWithSpeed + entry->offset, (u8 *)&reference[NATIVE_ENGINE_SPEED] + entry->offset, entry->size) == 0);
	}
	gNativeEngineSelectionEnabled = 0;
	VehBirth_SetConsts(&pentaWithSpeed);
	for (u32 i = 0; i < len(data.metaPhys); i++)
	{
		struct MetaPhys *entry = &data.metaPhys[i];
		assert(memcmp((u8 *)&pentaWithSpeed + entry->offset, (u8 *)&reference[NATIVE_ENGINE_PENTA] + entry->offset, entry->size) == 0);
	}
}

// Exercise the menu's real input transitions without rendering or controllers.
static void EngineTest_Lifecycle(void)
{
	struct GameTracker tracker = {0};
	sdata->gGT = &tracker;
	gNativeEngineSelectionEnabled = 1;
	sdata->gameProgress.unlockFlags &= ~UNLOCK_PENTA;
	assert(MM_Characters_NativeStepEngineProfile(NATIVE_ENGINE_TURN, 1) == NATIVE_ENGINE_BALANCED);
	assert(MM_Characters_NativeStepEngineProfile(NATIVE_ENGINE_BALANCED, -1) == NATIVE_ENGINE_TURN);
	sdata->gameProgress.unlockFlags |= UNLOCK_PENTA;
	assert(MM_Characters_NativeStepEngineProfile(NATIVE_ENGINE_TURN, 1) == NATIVE_ENGINE_PENTA);
	sdata->gameProgress.unlockFlags &= ~UNLOCK_PENTA;
	for (int players = 2; players <= 4; players++)
	{
		tracker.numPlyrNextGame = tracker.numPlyrCurrGame = players;
		D230.characterSelectMenuState = IN_MENU;
		sdata->characterSelectFlags = (1 << players) - 1;
		for (int player = 0; player < players; player++)
		{
			data.characterIDs[player] = CRASH_BANDICOOT;
			NativeEngine_SetSelectedProfile(player, NATIVE_ENGINE_BALANCED);
			s_nativeEngineSelectStage[player] = NATIVE_ENGINE_SELECT_ENGINE;
			s_nativeEngineSelectProfile[player] = NATIVE_ENGINE_SPEED;
		}
		// Ready -> engine -> character; one player's Back leaves others alone.
		MM_Characters_NativeEngineInput(0, MM_CHARACTER_SELECT_INPUT_CONFIRM);
		assert(D230.characterSelectMenuState == IN_MENU);
		MM_Characters_NativeEngineInput(0, MM_CHARACTER_SELECT_INPUT_BACK);
		assert(s_nativeEngineSelectStage[0] == NATIVE_ENGINE_SELECT_ENGINE);
		MM_Characters_NativeEngineInput(0, MM_CHARACTER_SELECT_INPUT_BACK);
		assert(s_nativeEngineSelectStage[0] == NATIVE_ENGINE_SELECT_CHARACTER);
		assert(sdata->characterSelectFlags == (u32)((1 << players) - 2));
		assert(s_nativeEngineSelectStage[1] == NATIVE_ENGINE_SELECT_ENGINE);
		s_nativeEngineSelectStage[0] = NATIVE_ENGINE_SELECT_ENGINE;
		sdata->characterSelectFlags |= 1;
		MM_Characters_NativeEngineInput(0, BTN_RIGHT);
		assert(s_nativeEngineSelectProfile[0] == NATIVE_ENGINE_TURN);
		assert(s_nativeEngineSelectProfile[1] == NATIVE_ENGINE_SPEED);
		assert(NativeEngine_GetSelectedProfile(0) == NATIVE_ENGINE_BALANCED);
		for (int player = 0; player < players; player++)
		{
			MM_Characters_NativeEngineInput(player, MM_CHARACTER_SELECT_INPUT_CONFIRM);
			assert(D230.characterSelectMenuState == (player == players - 1 ? EXITING_MENU : IN_MENU));
		}
		assert(D230.characterSelectExitsForward == 1);
		for (int player = 0; player < players; player++)
			assert(NativeEngine_GetEffectiveProfile(player) == (player == 0 ? NATIVE_ENGINE_TURN : NATIVE_ENGINE_SPEED));
	}

	struct GhostHeader ghost = {0};
	ghost.characterID = PENTA_PENGUIN;
	assert(GhostReplay_GetEngineProfile(&ghost) == NATIVE_ENGINE_PENTA);
	NativeEngineMetadata_StoreRetail(ghost.emptyPadding, NATIVE_ENGINE_SPEED);
	assert(GhostReplay_GetEngineProfile(&ghost) == NATIVE_ENGINE_SPEED);
	tracker.numPlyrCurrGame = 1;
	data.characterIDs[0] = CRASH_BANDICOOT;
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_TURN);
	NativeEngine_SetReplayOverride(0, NATIVE_ENGINE_PENTA);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_PENTA);
	NativeGhostInput_ClearSelection();
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_TURN);
	NativeEngine_SetReplayOverride(0, NATIVE_ENGINE_SPEED);
	GhostTape_Destroy();
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_TURN);
	NativeEngine_SetReplayOverride(0, NATIVE_ENGINE_SPEED);
	tracker.gameMode1 = MAIN_MENU;
	GhostReplay_Init1();
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_TURN);
}

// Use both actual preset menu definitions with the real controller input path.
static int presetTestConfirmed;
static void PresetTest_Callback(struct RectMenu *menu)
{
	assert(menu->funcState == RECTMENU_FUNC_STATE_INPUT);
	presetTestConfirmed = menu->rowSelected + 1;
}

static void PresetTest_Input(void)
{
	struct GameTracker tracker = {0};
	struct GamepadSystem pads = {0};
	sdata->gGT = &tracker;
	sdata->gGamepads = &pads;
	tracker.numPlyrNextGame = 1;
	struct RectMenu *definitions[] = {&s_nativePresetMenu, &s_nativePresetOptionsMenu};
	for (int i = 0; i < 2; i++)
	{
		struct RectMenu menu = *definitions[i];
		assert(menu.state & RECTMENU_NATIVE_DRAW_CALLBACK);
		assert(!(menu.state & RECTMENU_DRAW_CALLBACK_FLAGS));
		menu.state |= MUTE_SOUND_OF_MOVING_CURSOR;
		menu.rowSelected = 1;
		menu.funcPtr = PresetTest_Callback;
		sdata->activeSubMenu = &menu;
		pads.gamepad[0].buttonsTapped = BTN_DOWN;
		RECTMENU_CollectInput();
		assert(RECTMENU_ProcessInput(&menu) == 0);
		assert(menu.rowSelected == 2);
		pads.gamepad[0].buttonsTapped = BTN_UP;
		RECTMENU_CollectInput();
		RECTMENU_ProcessInput(&menu);
		assert(menu.rowSelected == 1);
		presetTestConfirmed = 0;
		pads.gamepad[0].buttonsTapped = BTN_CROSS_one;
		RECTMENU_CollectInput();
		assert(RECTMENU_ProcessInput(&menu) == 1);
		assert(presetTestConfirmed == 2);
	}
	sdata->activeSubMenu = NULL;
	sdata->gGT = NULL;
	sdata->gGamepads = NULL;
}

static void UnlockTest_OxideGhosts(void)
{
	static struct GameTracker tracker;
	static struct Driver driver;
	static struct Thread thread;
	sdata->gGT = &tracker;
	gNativeAdditionalUnlocksEnabled = 1;
	gNativeGhostReplayMode = 0;
	tracker.gameMode1 = TIME_TRIAL;
	tracker.drivers[0] = &driver;
	tracker.threadBuckets[PLAYER].thread = &thread;
	thread.object = &driver;
	data.bitIndex_timeTrialFlags_saveData.nTropyOpen = 1;
	data.bitIndex_timeTrialFlags_saveData.nOxideOpen = 2;

	// The former character requirements and Tropy ghosts alone no longer qualify.
	sdata->gameProgress.unlockFlags = UNLOCK_CHARACTERS;
	for (int i = 0; i < MEMCARD_HIGH_SCORE_TRACK_COUNT; i++)
		sdata->gameProgress.highScoreTracks[i].timeTrialFlags = 3;
	GAMEPROG_AdvPercent(&sdata->advProgress);
	assert(!CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_OXIDE));
	sdata->gameProgress.unlockFlags = 0;

	// Every original track is required, even with all the other ghosts beaten.
	for (int missing = 0; missing < MEMCARD_HIGH_SCORE_TRACK_COUNT; missing++)
	{
		for (int i = 0; i < MEMCARD_HIGH_SCORE_TRACK_COUNT; i++)
			sdata->gameProgress.highScoreTracks[i].timeTrialFlags = (i == missing) ? 3 : 7;
		GAMEPROG_AdvPercent(&sdata->advProgress);
		assert(!CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_OXIDE));
	}

	// Beating the final ghost grants Oxide immediately alongside the scrapbook.
	tracker.levelID = MEMCARD_HIGH_SCORE_TRACK_COUNT - 1;
	GAMEPROG_GetPtrHighScoreTrack();
	tracker.timeToBeatInTimeTrial_ForCurrentEvent = 100;
	driver.timeElapsedInRace = 99;
	MainGameEnd_SoloRaceGetReward(0);
	assert(CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_OXIDE));
	assert(CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_SCRAPBOOK));
	assert(tracker.levelID == MEMCARD_HIGH_SCORE_TRACK_COUNT - 1);

	// Completed saves qualify without Gem Cup characters, when the option is on.
	memset(sdata->gameProgress.unlocks, 0, sizeof(sdata->gameProgress.unlocks));
	gNativeAdditionalUnlocksEnabled = 0;
	tracker.gameModeEnd = 0;
	MainGameEnd_SoloRaceGetReward(0);
	GAMEPROG_AdvPercent(&sdata->advProgress);
	assert(!CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_OXIDE));
	gNativeAdditionalUnlocksEnabled = 1;
	GAMEPROG_AdvPercent(&sdata->advProgress);
	assert(CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_OXIDE));
}

// Exercise the actual browser navigation and measure its wrapped text without
// booting the game, reading saves or drawing into the user's active session.
static void UnlockMenuTest(void)
{
	struct RectMenu menu = s_nativeUnlocksMenu;
	gNativeFont = NATIVE_FONT_ORIGINAL;
	gNativeAdditionalUnlocksEnabled = 1;
	assert(MM_NativeUnlockEntryAt(5)->name == LNG_DR_N_TROPY);
	assert(MM_NativeUnlockEntryAt(6)->name == LNG_N_OXIDE_FULL);
	assert(MM_NativeUnlockEntryAt(7)->name == LNG_PENTA_PENGUIN);
	for (int additional = 0; additional <= 1; additional++)
	{
		gNativeAdditionalUnlocksEnabled = additional;
		int count = MM_NativeUnlockCount();
		assert(count == 13 + additional);
		menu.rowSelected = 0;
		s_nativeUnlockFirst = 0;
		for (int i = 1; i <= count * 2; i++)
		{
			MM_NativeUnlocksInput(&menu, BTN_DOWN);
			assert(menu.rowSelected == i % count);
			assert(s_nativeUnlockFirst >= 0);
			assert(s_nativeUnlockFirst + MM_NATIVE_UNLOCK_VISIBLE_ROWS <= count);
			assert(menu.rowSelected >= s_nativeUnlockFirst);
			assert(menu.rowSelected < s_nativeUnlockFirst + MM_NATIVE_UNLOCK_VISIBLE_ROWS);
		}
		MM_NativeUnlocksInput(&menu, BTN_UP);
		assert(menu.rowSelected == count - 1);
		MM_NativeUnlocksInput(&menu, BTN_RIGHT);
		assert(menu.rowSelected == 0);
		MM_NativeUnlocksInput(&menu, BTN_LEFT);
		assert(menu.rowSelected == count - 1);
		for (int i = 0; i < count; i++)
		{
			const struct MMNativeUnlockEntry *entry = MM_NativeUnlockEntryAt(i);
			char requirement[160];
			snprintf(requirement, sizeof(requirement), "%s %s", entry->requirement[0], entry->requirement[1]);
			int bottom = 94 + DecalFont_DrawMultiLine(requirement, 250, 94, 234, FONT_SMALL, 0x800);
			if (entry->name == LNG_PENTA_PENGUIN && !additional)
				bottom += 10 + DecalFont_DrawMultiLine("ENABLE ADDITIONAL UNLOCKS IN OPTIONS > GAMEPLAY", 250, 0, 234, FONT_SMALL, 0x800);
			assert(bottom <= 178);
		}
	}
	// An options change must clamp both selection and scroll position.
	gNativeAdditionalUnlocksEnabled = 0;
	MM_NativeUnlocksInput(&menu, 0);
	assert(menu.rowSelected == 12 && s_nativeUnlockFirst == 6);
	MM_NativeUnlocksInput(&menu, BTN_TRIANGLE);
	assert(sdata->ptrDesiredMenu == &D230.menuMainMenu);
	sdata->ptrDesiredMenu = NULL;
	MM_NativeUnlocksInput(&menu, BTN_SQUARE_one);
	assert(sdata->ptrDesiredMenu == &D230.menuMainMenu);
	sdata->ptrDesiredMenu = NULL;
}

static void EngineTest_SavePersistence(const char *root)
{
	struct GameTracker tracker = {0};
	struct MemcardProfile card = {0};
	tracker.numPlyrCurrGame = 1;
	sdata->gGT = &tracker;
	sdata->ptrToMemcardBuffer2 = &card;
	sdata->gameProgress.unlockFlags |= UNLOCK_PENTA;
	gNativeEngineSelectionEnabled = 1;
	GAMEPROG_NewProfile_InsideAdv(&sdata->advProgress);
	sdata->advProgress.characterID = CRASH_BANDICOOT;
	strcpy(sdata->advProgress.name, "ENGINE TEST");
	data.characterIDs[0] = CRASH_BANDICOOT;
	assert(NativeMemcard_SetRoot(root) == NATIVE_MEMCARD_OK);

	for (int profile = NATIVE_ENGINE_DEFAULT; profile < NATIVE_ENGINE_COUNT; profile++)
	{
		NativeEngine_SetSelectedProfile(0, profile);
		SelectProfile_SaveAdvProfile(0);
		assert(NativeMemcard_WriteSaveData("ENGINE-TEST-SLOTS", "", 0, (const u8 *)&card, sizeof(card)) == NATIVE_MEMCARD_OK);
		memset(&card, 0, sizeof(card));
		assert(NativeMemcard_ReadSaveData("ENGINE-TEST-SLOTS", (u8 *)&card, sizeof(card), 0) == NATIVE_MEMCARD_OK);
		NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_SPEED);
		NativeEngine_SetReplayOverride(0, NATIVE_ENGINE_TURN);
		SelectProfile_LoadAdvProfile(0);
		assert(NativeEngine_GetSelectedProfile(0) == profile);
		const char markers[] = {'B', 'A', 'S', 'T', 'P'};
		assert(SelectProfile_AdventureEngineMarker(&card.advProgress[0])[0] == markers[profile == NATIVE_ENGINE_DEFAULT ? NATIVE_ENGINE_BALANCED : profile]);
		assert(NativeEngine_GetEffectiveProfile(0) == (profile == NATIVE_ENGINE_DEFAULT ? NATIVE_ENGINE_BALANCED : profile));

		NativeAutoSave_SetExitPortal(GEM_STONE_VALLEY, 3);
		assert(NativeAutoSave_Write());
		NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_SPEED);
		NativeAutoSave_Refresh();
		assert(NativeAutoSave_QuickLoad());
		assert(NativeEngine_GetSelectedProfile(0) == profile);
		assert(NativeAutoSave_GetExitPortal(GEM_STONE_VALLEY) == 3);
	}

	// Toggling the option off must not erase the selected engine.
	gNativeEngineSelectionEnabled = 0;
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_ACCEL);
	assert(NativeAutoSave_Write());
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_SPEED);
	NativeAutoSave_Refresh();
	assert(NativeAutoSave_Apply());
	assert(NativeEngine_GetSelectedProfile(0) == NATIVE_ENGINE_ACCEL);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	gNativeEngineSelectionEnabled = 1;
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_ACCEL);

	// Old data, invalid tags and locked Penta must reset a stale selection.
	const u32 words[] = {0, 0x12345678, NATIVE_ENGINE_METADATA_TAG | NATIVE_ENGINE_COUNT, NativeEngineMetadata_EncodeWord(NATIVE_ENGINE_PENTA)};
	sdata->gameProgress.unlockFlags &= ~UNLOCK_PENTA;
	card.gameProgress.unlockFlags &= ~UNLOCK_PENTA;
	for (u32 i = 0; i < len(words); i++)
	{
		card.advProgress[0] = sdata->advProgress;
		card.advProgress[0].reservedRewardFlags = words[i];
		NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_SPEED);
		SelectProfile_LoadAdvProfile(0);
		assert(NativeEngine_GetSelectedProfile(0) == NATIVE_ENGINE_DEFAULT);
		assert(SelectProfile_AdventureEngineMarker(&card.advProgress[0])[0] == 'B');
	}

	// Both existing autosave versions retain their checksum and portal behavior.
	for (int version = 1; version <= 2; version++)
	{
		struct NativeAutoSaveFile legacy = s_nativeAutoSave;
		legacy.version = version;
		legacy.size = version == 1 ? OFFSETOF(struct NativeAutoSaveFile, exitPortalHub) : sizeof(legacy);
		legacy.adv.reservedRewardFlags = 0;
		legacy.exitPortalHub = GEM_STONE_VALLEY;
		legacy.exitPortalID = 3;
		legacy.checksum = NativeAutoSave_Checksum(&legacy);
		assert(NativeMemcard_WriteSaveData(NATIVE_AUTOSAVE_NAME, "", 0, (const u8 *)&legacy, legacy.size) == NATIVE_MEMCARD_OK);
		NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_SPEED);
		NativeAutoSave_Refresh();
		assert(NativeAutoSave_Apply());
		assert(NativeEngine_GetSelectedProfile(0) == NATIVE_ENGINE_DEFAULT);
		assert(NativeAutoSave_GetExitPortal(GEM_STONE_VALLEY) == (version == 1 ? -1 : 3));
		legacy.adv.reservedRewardFlags ^= 1;
		// Start without a backup so the bad file has nothing valid to fall back to.
		assert(NativeMemcard_RemoveFile(NATIVE_AUTOSAVE_NAME) == NATIVE_MEMCARD_OK);
		assert(NativeMemcard_WriteSaveData(NATIVE_AUTOSAVE_NAME, "", 0, (const u8 *)&legacy, legacy.size) == NATIVE_MEMCARD_OK);
		NativeAutoSave_Refresh();
		assert(!NativeAutoSave_Exists());
	}
	// A damaged autosave must be quarantined and the previous generation restored.
	{
		char autoPath[512];
		char corruptPath[512];
		unsigned char probe[8];
		snprintf(autoPath, sizeof(autoPath), "%s/slot0/%s", root, NATIVE_AUTOSAVE_NAME);
		snprintf(corruptPath, sizeof(corruptPath), "%s/slot0/.%s.corrupt", root, NATIVE_AUTOSAVE_NAME);
		assert(NativeAutoSave_Write());
		assert(NativeAutoSave_Write());
		FILE *damaged = fopen(autoPath, "wb");
		assert(damaged != NULL);
		fwrite("garbage", 1, 7, damaged);
		fclose(damaged);
		NativeAutoSave_Refresh();
		assert(NativeAutoSave_Exists());
		FILE *quarantined = fopen(corruptPath, "rb");
		assert(quarantined != NULL);
		assert(fread(probe, 1, 7, quarantined) == 7);
		fclose(quarantined);
		assert(memcmp(probe, "garbage", 7) == 0);
		// Primary was restored, and no temp file is left behind.
		assert(NativeMemcard_ReadSaveData(NATIVE_AUTOSAVE_NAME, probe, 4, 0) == NATIVE_MEMCARD_OK);
	}
	assert(NativeMemcard_RemoveRoot(root) == NATIVE_MEMCARD_OK);
	NativeMemcard_ClearRoot();
	sdata->ptrToMemcardBuffer2 = 0;
	sdata->gGT = NULL;
}

#include "native_visibility_checks.h"
#include "native_aspect_game_checks.h"
#include "native_projection_game_checks.h"
#include "native_projection_renderer_checks.h"

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--hud-aspect-only") == 0)
	{
		AspectTest_HudTransitions();
		puts("HUD proportions refresh across in-level aspect changes passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--visibility-only") == 0)
	{
		VisibilityTest_Camera();
		VisibilityTest_Expanded();
		puts("Expanded visibility camera, PVS recovery and distant-leaf checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--projection-only") == 0)
	{
		ProjectionTest_GamePaths();
		puts("Projection camera, OT boundaries and menu checks passed");
		return 0;
	}
	if (argc == 4 && strcmp(argv[1], "--projection-config-check") == 0)
	{
		load_config();
		assert(gNativeProjectionMode == atoi(argv[2]));
		assert(gNativeProjectionStrength == atoi(argv[3]));
		gNativePresetPending = 0;
		save_config();
		gNativeProjectionMode = gNativeProjectionStrength = -1;
		load_config();
		assert(gNativeProjectionMode == atoi(argv[2]));
		assert(gNativeProjectionStrength == atoi(argv[3]));
		puts("Projection config persistence passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--aspect-only") == 0)
	{
		AspectTest_GamePaths();
		puts("Aspect projection, culling, presentation and menu checks passed");
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "--aspect-config-check") == 0)
	{
		load_config();
		assert(gNativeAspectRatio == atoi(argv[2]));
		gNativePresetPending = 0;
		save_config();
		gNativeAspectRatio = -1;
		load_config();
		assert(gNativeAspectRatio == atoi(argv[2]));
		puts("Aspect setting persistence passed");
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "--fov-config-check") == 0)
	{
		load_config();
		assert(gNativeFovDegrees == atoi(argv[2]));
		gNativePresetPending = 0;
		save_config();
		gNativeFovDegrees = -1;
		load_config();
		assert(gNativeFovDegrees == atoi(argv[2]));
		puts("FOV setting persistence passed");
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "--engine-save-only") == 0)
	{
		EngineTest_SavePersistence(argv[2]);
		puts("Engine manual save and autosave persistence checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--unlock-menu-only") == 0)
	{
		UnlockMenuTest();
		puts("Unlock browser navigation and text bounds checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--oxide-unlock-only") == 0)
	{
		UnlockTest_OxideGhosts();
		puts("Oxide ghost unlock checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--preset-input-only") == 0)
	{
		PresetTest_Input();
		puts("Preset controller navigation and confirmation checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--engine-lifecycle-only") == 0)
	{
		EngineTest_Lifecycle();
		puts("Engine multiplayer and replay lifecycle checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--engine-physics-only") == 0)
	{
		EngineTest_Physics();
		puts("Independent engine physics checks passed");
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "--engine-config-check") == 0)
	{
		load_config();
		assert(gNativeEngineSelectionEnabled == atoi(argv[2]));
		save_config();
		gNativeEngineSelectionEnabled = !atoi(argv[2]);
		load_config();
		assert(gNativeEngineSelectionEnabled == atoi(argv[2]));
		puts("Engine setting persistence passed");
		return 0;
	}
	if (argc == 4 && strcmp(argv[1], "--config-check") == 0)
	{
		load_config();
		assert(gNativeModernMapEnabled == atoi(argv[2]));
		assert(gNativePreciseMinimapEnabled == 0);
		assert(gNativeModernHudIconsEnabled == atoi(argv[3]));
		save_config();
		puts("HUD settings migration and save passed");
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "--minimap-only") == 0)
	{
		DepthTest_MinimapPrecision();
		DepthTest_CollisionMinimap();
		DepthTest_MinimapCrossingsAndCache();
		puts("Minimap precision and collision checks passed");
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
	if (argc == 3 && strcmp(argv[1], "--minimap-assets") == 0)
	{
		DepthTest_MinimapAssets(argv[2]);
		return 0;
	}
	// Headless snapshot of every options-menu row's rendered text, for each
	// language and each accepted value. Guards the options menu against label
	// and formatting regressions without needing a GL context, and lets a
	// refactor of the row rendering be checked by diffing the output.
	if (argc == 2 && strcmp(argv[1], "--option-strings-only") == 0)
	{
		struct OptionStringSweep
		{
			const char *name;
			s16 stringIndex;
			int *storage;
			int minValue;
			int maxValue;
		};
		static const struct OptionStringSweep sweeps[] = {
		    {"aspect_ratio", NATIVE_MENU_STRING_ASPECT_RATIO, &gNativeAspectRatio, 0, NATIVE_ASPECT_COUNT - 1},
		    {"fov_degrees", NATIVE_MENU_STRING_FIELD_OF_VIEW, &gNativeFovDegrees, 0, 100},
		    {"projection_mode", NATIVE_MENU_STRING_PROJECTION, &gNativeProjectionMode, 0, NATIVE_PROJECTION_MODE_COUNT - 1},
		    {"projection_strength", NATIVE_MENU_STRING_PROJECTION_STRENGTH, &gNativeProjectionStrength, 0, 100},
		    {"mirror_mode", NATIVE_MENU_STRING_MIRROR_MODE, &gNativeMirrorModeEnabled, 0, 1},
		    {"frame_rate", NATIVE_MENU_STRING_FRAME_RATE, &gNative60FpsEnabled, 0, NATIVE_FRAME_RATE_COUNT - 1},
		    {"default_camera_far", NATIVE_MENU_STRING_DEFAULT_CAMERA, &gNativeDefaultCameraFar, 0, 1},
		    {"default_hud_speedometer", NATIVE_MENU_STRING_DEFAULT_HUD, &gNativeDefaultHudSpeedometer, 0, 1},
		    {"ai_racers", NATIVE_MENU_STRING_AI_RACERS, &gNativeAIRacersMode, 0, NATIVE_AI_RACERS_MODE_COUNT - 1},
		    {"skip_mask_hints", NATIVE_MENU_STRING_SKIP_MASK_HINTS, &gNativeSkipMaskHints, 0, 1},
		    {"engine_selection", NATIVE_MENU_STRING_ENGINE_SELECTION, &gNativeEngineSelectionEnabled, 0, 1},
		    {"additional_unlocks", NATIVE_MENU_STRING_ADDITIONAL_UNLOCKS, &gNativeAdditionalUnlocksEnabled, 0, 1},
		    {"anti_aliasing", NATIVE_MENU_STRING_ANTI_ALIASING, &gNativeAntiAliasingMode, 0, NATIVE_AA_MODE_COUNT - 1},
		    {"dithering", NATIVE_MENU_STRING_DITHERING, &gNativeDitheringEnabled, 0, 1},
		    {"borderless", NATIVE_MENU_STRING_BORDERLESS, &gNativeBorderlessEnabled, 0, 1},
		    {"pgxp", NATIVE_MENU_STRING_PGXP, &gNativePgxpMode, 0, NATIVE_PGXP_MODE_COUNT - 1},
		    {"renderer", NATIVE_MENU_STRING_RENDERER, &gNativeRendererMode, 0, NATIVE_RENDERER_MODE_COUNT - 1},
		    {"color_depth", NATIVE_MENU_STRING_COLOR_DEPTH, &gNativeColorDepth, 0, NATIVE_COLOR_DEPTH_COUNT - 1},
		    {"ps1_resolution", NATIVE_MENU_STRING_PS1_RESOLUTION, &gNativePs1ResolutionEnabled, 0, 1},
		    {"texture_filter", NATIVE_MENU_STRING_TEXTURE_FILTER, &g_cfg_bilinearFiltering, 0, 1},
		    {"modern_minimap", NATIVE_MENU_STRING_MODERN_MAP, &gNativeModernMapEnabled, 0, 1},
		    {"modern_hud_icons", NATIVE_MENU_STRING_MODERN_HUD_ICONS, &gNativeModernHudIconsEnabled, 0, 1},
		    {"font", NATIVE_MENU_STRING_FONT, &gNativeFont, 0, NATIVE_FONT_COUNT - 1},
		    {"kart_hue", NATIVE_MENU_STRING_KART_HUE, &gNativeKartHue, 0, NATIVE_KART_HUE_STEPS - 1},
		    {"max_lod", NATIVE_MENU_STRING_MAX_LOD, &gNativeMaxLodEnabled, 0, 1},
		    {"depth_buffer", NATIVE_MENU_STRING_DEPTH_BUFFER, &gNativeDepthBufferEnabled, 0, 1},
		    {"hd_pause_screen", NATIVE_MENU_STRING_HD_PAUSE, &gNativeHdPauseMode, 0, 2},
		    {"smoothed_physics", NATIVE_MENU_STRING_PHYSICS, &gNativeSmoothedPhysicsEnabled, 0, 1},
		    {"smoothed_ai", NATIVE_MENU_STRING_AI_PHYSICS, &gNativeSmoothedAIEnabled, 0, 1},
		    {"smoothed_collisions", NATIVE_MENU_STRING_COLLISION_PHYSICS, &gNativeSmoothedCollisionEnabled, 0, 1},
		    {"smoothed_steering", NATIVE_MENU_STRING_STEERING_PHYSICS, &gNativeSmoothedSteeringEnabled, 0, 1},
		};

		for (unsigned int sweep = 0; sweep < sizeof(sweeps) / sizeof(sweeps[0]); sweep++)
		{
			for (int language = 0; language < 6; language++)
			{
				cfg_language = language;
				for (int value = sweeps[sweep].minValue; value <= sweeps[sweep].maxValue; value++)
				{
					*sweeps[sweep].storage = value;
					printf("%s|lang%d|value%d|%s\n", sweeps[sweep].name, language, value, RECTMENU_GetString(sweeps[sweep].stringIndex));
				}
			}
		}
		// Rows that read the cheat bitmask rather than an option global.
		// Audio rows are excluded: they read sdata->lngStrings, which this mode
		// does not initialise.
		for (int language = 0; language < 6; language++)
		{
			cfg_language = language;
			for (int value = 0; value <= 1; value++)
			{
				if (value)
				{
					gNativeCheatConfigMask |= CHEAT_TURBOCOUNT;
				}
				else
				{
					gNativeCheatConfigMask &= ~CHEAT_TURBOCOUNT;
				}
				printf("boost_counter|lang%d|value%d|%s\n", language, value, RECTMENU_GetString(NATIVE_MENU_STRING_BOOST_COUNTER));
			}
		}
		puts("option string snapshot complete");
		return 0;
	}

	if (!SDL_Init(SDL_INIT_VIDEO))
		return 77;
	gNativeDitheringEnabled = 0;
	gNativeBorderlessEnabled = 0;
	if (!NativeRenderer_InitialiseRender("CTR depth test", 320, 240, 0))
		return 77;
	SDL_HideWindow(g_window);
	assert(NativeRenderer_InitialisePSX());
	if (argc == 2 && strcmp(argv[1], "--hud-depth-only") == 0)
	{
		DepthTest_HudDepth();
		NativeRenderer_Shutdown();
		SDL_DestroyWindow(g_window);
		SDL_Quit();
		puts("Multi-material HUD depth and bounded SSAA work passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--gpu-transform-only") == 0)
	{
		DepthTest_GpuParity();
		DepthTest_StaticParity();
		DepthTest_GpuBenchmark();
		NativeRenderer_Shutdown();
		SDL_DestroyWindow(g_window);
		SDL_Quit();
		puts("GPU transform parity, frame lifetime and benchmark checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--projection-renderer-only") == 0)
	{
		ProjectionTest_Resolve();
		ProjectionTest_Barriers();
		ProjectionTest_PauseTransitions();
		NativeRenderer_Shutdown();
		SDL_DestroyWindow(g_window);
		SDL_Quit();
		puts("Projection framebuffer sampling, AA and HUD checks passed");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--aspect-renderer-only") == 0)
	{
		AspectTest_RenderTargets();
		NativeRenderer_Shutdown();
		SDL_DestroyWindow(g_window);
		SDL_Quit();
		puts("Aspect framebuffer proportions passed for all five ratios");
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "--hud-icons-only") == 0)
	{
		DepthTest_HudIcons();
		DepthTest_MenuArrows();
		NativeRenderer_Shutdown();
		SDL_DestroyWindow(g_window);
		SDL_Quit();
		puts("Native HUD icon checks passed");
		return 0;
	}

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
