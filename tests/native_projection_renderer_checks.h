// Real framebuffer checks: source sampling, camera isolation, HUD and flushes.
static void ProjectionTest_PauseTransitions(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAspectRatio = NATIVE_ASPECT_4_3;
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	SetDefDrawEnv(&activeDrawEnv, 0, 0, 512, 216);
	SetDefDispEnv(&activeDispEnv, 0, 0, 512, 216);
	activeDrawEnv.dfe = 1;
	NativeRenderer_EnableGamePresentation(1);
	NativeRenderer_BeginScene();
	NativeRenderer_Clear(0, 0, 512, 216, 255, 255, 255);
	u16 palette[16];
	for (int i = 0; i < 16; i++) palette[i] = (u16)((i * 2) * (1 + 32 + 1024));
	assert(NativeRenderer_CapturePauseBackground(palette, 1));
	const TextureID capture = NativeRenderer_GetPauseBackgroundTexture();
	assert(capture != (TextureID)-1 && glIsTexture(capture));
	const int capturedW = s_pauseBackgroundTarget.width;
	NativeRenderer_EndScene();
	// A pause frame can change renderer and every aspect without discarding
	// the frozen screenshot or retaining an obsolete world target binding.
	for (int mode = 0; mode < NATIVE_RENDERER_MODE_COUNT; mode++)
		for (int ratio = 0; ratio < NATIVE_ASPECT_COUNT; ratio++)
		{
			gNativeRendererMode = mode;
			gNativeAspectRatio = ratio;
			NativeRenderer_BeginScene();
			assert(!s_projectedWorldBound && !s_projectedWorldSeeded);
			assert(NativeRenderer_GetPauseBackgroundTexture() == capture);
			assert(s_pauseBackgroundTarget.width == capturedW);
			int aw, ah;
			NativeAspect_GetPresentation(&aw, &ah);
			assert(s_presentAspectW * ah == s_presentAspectH * aw);
			NativeRenderer_Clear(0, 0, 512, 216, 0, 255, 0);
			assert(glGetError() == GL_NO_ERROR);
			NativeRenderer_EndScene();
		}
	// A subsequent capture uses the new size and still produces a valid image.
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAspectRatio = NATIVE_ASPECT_32_9;
	NativeRenderer_BeginScene();
	assert(NativeRenderer_CapturePauseBackground(palette, 1));
	assert(s_pauseBackgroundTarget.width == s_mainResolveWidth);
	assert(s_pauseBackgroundTarget.height == s_mainResolveHeight);
	assert(glGetError() == GL_NO_ERROR);
	NativeRenderer_EndScene();
	NativeRenderer_EnableGamePresentation(0);
}
static void ProjectionTest_FlushPacket(void *packet)
{
	ParsePrimitive(packet);
	GPUDrawSplit *last = &s_gpu.splits[s_gpu.splitIndex];
	last->numVerts = s_gpu.vertexIndex - last->startVertex;
}

static void ProjectionTest_Barriers(void)
{
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	for (int overlay = 0; overlay <= 1; overlay++)
	{
		DepthTest_Begin(1, NATIVE_PGXP_MODE_OFF);
		gNativeRendererMode = NATIVE_RENDERER_NATIVE;
		NativeProjectionMarker begin = {0}, end = {0};
		begin.code = 0xB4000001; end.code = 0xB4000000;
		setlen(&begin, sizeof(begin) / sizeof(u32) - P_LEN);
		setlen(&end, sizeof(end) / sizeof(u32) - P_LEN);
		begin.rect = (RECT16){0, 0, 320, 240};
		NativeProjection_BuildParams(NATIVE_PROJECTION_PERSPECTIVE, 0, 1, &begin.params);
		ProjectionTest_FlushPacket(&begin);
		assert(s_gpuProjectionCamera == 1);
		int layer = DepthTest_BeginNativeLayer(0);
		DepthTest_NativeTriangle(layer, 0, 200, NATIVE_DRAW3D_DOUBLE_SIDED, 0);
		DepthTest_Marker(0, layer);
		ProjectionTest_FlushPacket(&depthTestMarkers[0]);
		DrawAllSplits(); // Feedback/capacity flush while the camera is still open.
		layer = DepthTest_BeginNativeLayer(0);
		DepthTest_NativeTriangle(layer, 1, 400, NATIVE_DRAW3D_DOUBLE_SIDED, 0);
		DepthTest_Marker(1, layer);
		ProjectionTest_FlushPacket(&depthTestMarkers[1]);
		if (overlay)
		{
			layer = DepthTest_BeginNativeLayer(0);
			NativeDraw3DVertex a = DepthTest_NativeVertex(20, 20, 500, 2);
			NativeDraw3DVertex b = DepthTest_NativeVertex(300, 20, 500, 2);
			NativeDraw3DVertex c = DepthTest_NativeVertex(160, 220, 500, 2);
			NativeDraw3DMaterial material = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED | NATIVE_DRAW3D_OVERLAY};
			assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material));
			DepthTest_Marker(2, layer);
			ProjectionTest_FlushPacket(&depthTestMarkers[2]);
			assert(s_gpuDeferredOverlayCount == 1);
		}
		ProjectionTest_FlushPacket(&end);
		assert(!s_gpuProjectionCamera && !s_gpuDeferredOverlayCount);
		DepthTest_Pixel(160, 100, overlay ? 0 : 255, 0, overlay ? 255 : 0);
		// A normal HUD packet is drawn after the camera resolve.
		POLY_F4 hud = {0};
		setPolyF4(&hud); hud.g0 = 255;
		hud.x0 = hud.x2 = 5; hud.x1 = hud.x3 = 15;
		hud.y0 = hud.y1 = 5; hud.y2 = hud.y3 = 15;
		ProjectionTest_FlushPacket(&hud);
		DrawAllSplits();
		DepthTest_Pixel(10, 10, 0, 255, 0);
		NativeRenderer_EndScene();
	}
}

static void ProjectionTest_Resolve(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
	gNativeDitheringEnabled = 0;
	gNativeAspectRatio = NATIVE_ASPECT_4_3;
	NativeRenderer_EnableGamePresentation(0);
	const int aaModes[] = {NATIVE_AA_OFF, NATIVE_AA_MSAA_4X, NATIVE_AA_SSAA_2X};
	for (int aa = 0; aa < 3; aa++) for (int mode = 0; mode < NATIVE_PROJECTION_MODE_COUNT; mode++)
	{
		gNativeAntiAliasingMode = aaModes[aa];
		SetDefDrawEnv(&activeDrawEnv, 0, 0, 320, 240);
		SetDefDispEnv(&activeDispEnv, 0, 0, 320, 240);
		activeDrawEnv.dfe = 1;
		NativeRenderer_BeginScene();
		NativeRenderer_Clear(0, 0, 320, 240, 0, 0, 0);
		assert(NativeRenderer_BindProjectedWorld(1));
		const int w = s_mainRenderTarget.width, h = s_mainRenderTarget.height;
		glEnable(GL_SCISSOR_TEST);
		// A coloured grid exposes both horizontal and vertical inverse mapping.
		for (int y = 0; y < h; y += 8) for (int x = 0; x < w; x += 8)
		{
			glScissor(x, y, 8, 8);
			glClearColor((x + 4.0f) / w, (y + 4.0f) / h, 0.25f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		glDisable(GL_SCISSOR_TEST);
		NativeRenderer_BindProjectedWorld(0);
		NativeProjectionParams params;
		assert(NativeProjection_BuildParams(mode, 50, 2.0, &params));
		RECT16 rect = {0, 0, 320, 240};
		// A camera resolve must not overwrite the neighbouring camera or
		// sample its pixels, including with MSAA/SSAA targets.
		NativeRenderer_Clear(0, 0, 320, 240, 0, 0, 255);
		RECT16 leftRect = {0, 0, 160, 240};
		NativeRenderer_ResolveProjectedWorld(&leftRect, &params);
		DepthTest_Pixel(240, 100, 0, 0, 248);
		u8 left[4];
		DepthTest_ReadPixel(80, 120, left);
		assert(abs(left[0] - 64) <= 9);
		RECT16 rightRect = {160, 0, 160, 240};
		NativeRenderer_ResolveProjectedWorld(&rightRect, &params);
		u8 right[4];
		DepthTest_ReadPixel(240, 120, right);
		assert(abs(right[0] - 191) <= 9);
		NativeRenderer_ResolveProjectedWorld(&rect, &params);
		// HUD after the resolve must keep its position/colour across modes.
		NativeRenderer_Clear(5, 5, 12, 12, 0, 255, 0);
		const struct NativeRenderTarget *target = NativeRenderer_ResolveMainRenderTarget();
		glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
		for (int yi = 1; yi <= 3; yi++) for (int xi = 1; xi <= 7; xi++)
		{
			const int x = target->width * xi / 8, y = target->height * yi / 4;
			double sx, sy;
			assert(NativeProjection_MapOutputToSource(&params,
				2.0 * (x + 0.5) / target->width - 1,
				2.0 * (y + 0.5) / target->height - 1, &sx, &sy));
			u8 pixel[4];
			glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			assert(abs(pixel[0] - (int)((sx + 1) * 127.5)) <= 9);
			assert(abs(pixel[1] - (int)((sy + 1) * 127.5)) <= 12);
			assert(abs(pixel[2] - 64) <= 2);
		}
		u8 hud[4];
		glReadPixels(target->width * 10 / 320, target->height * 230 / 240,
			1, 1, GL_RGBA, GL_UNSIGNED_BYTE, hud);
		assert(hud[0] == 0 && hud[1] == 248 && hud[2] == 0);
		assert(glGetError() == GL_NO_ERROR);
		NativeRenderer_EndScene();
	}
}
