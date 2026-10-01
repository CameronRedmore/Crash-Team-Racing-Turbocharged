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

static void DepthTest_Pixel(int x, int y, int r, int g, int b)
{
	u8 pixel[4];
	// The render target is scaled to the host window. Sample using the same
	// logical coordinates as the PS1 packets, away from triangle boundaries.
	x = x * s_mainRenderTarget.width / 320;
	y = (240 - y) * s_mainRenderTarget.height / 240;
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	if (abs(pixel[0] - r) > 10 || abs(pixel[1] - g) > 10 || abs(pixel[2] - b) > 10)
	{
		fprintf(stderr, "Unexpected pixel %d,%d: %u,%u,%u; expected %d,%d,%d\n", x, y, pixel[0], pixel[1], pixel[2], r, g, b);
		abort();
	}
	assert(glGetError() == GL_NO_ERROR);
}

int main(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
	gNativeAntiAliasingEnabled = 0;
	gNativeDitheringEnabled = 0;
	gNativeBorderlessEnabled = 0;
	if (!NativeRenderer_InitialiseRender("CTR depth test", 320, 240, 0)) return 77;
	SDL_HideWindow(g_window);
	assert(NativeRenderer_InitialisePSX());
	const float nearDepth[3] = {200, 200, 200};
	const float farDepth[3] = {800, 800, 800};
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
	}
	NativeRenderer_Shutdown();
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	puts("Native depth renderer checks passed");
	return 0;
}
