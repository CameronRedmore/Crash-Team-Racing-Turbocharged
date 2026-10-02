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

int main(void)
{
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
		}
	}
	NativeRenderer_Shutdown();
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	puts("Native depth renderer checks passed");
	return 0;
}
