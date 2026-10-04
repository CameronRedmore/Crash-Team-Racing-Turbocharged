// Included by the renderer integration harness after the real game unity.
// These checks need neither an OpenGL context nor game assets.
static void AspectTest_GamePaths(void)
{
	static const double scales[] = {1.0, 0.75, 5.0 / 6.0, 4.0 / 7.0, 0.375};
	static const int widths[] = {1440, 1920, 1728, 1920, 1920};
	static const int heights[] = {1080, 1080, 1080, 822, 540};
	struct GameTracker tracker = {0};
	const int previousAspect = gNativeAspectRatio;
	const int previousFov = gNativeFovDegrees;
	const int previousProjection = gNativeProjectionMode;
	const int previousProjectionStrength = gNativeProjectionStrength;
	struct Level level = {0};
	struct Instance instance = {0};
	struct InstDef *allInstances[] = {(struct InstDef *)&instance, NULL};
	struct Instance *pvsInstances[] = {NULL};
	tracker.levelID = MAIN_MENU_LEVEL;
	tracker.level1 = &level;
	level.ptrInstDefPtrArray = allInstances;
	tracker.cameraDC[0].visInstSrc = pvsInstances;
	sdata->gGT = &tracker;
	gNativePresetPending = 1; // Menu input must not write the user's config.
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	gNativeProjectionMode = NATIVE_PROJECTION_PERSPECTIVE;
	gNativeProjectionStrength = 50;
	gNativeFovDegrees = 0;
	Platform_InitScratchpad();
	g_windowWidth = 1920;
	g_windowHeight = 1080;
	NativeRenderer_EnableGamePresentation(1);

	for (int ratio = 0; ratio < NATIVE_ASPECT_COUNT; ratio++)
	{
		gNativeAspectRatio = ratio;
		NativeRenderer_UpdateGamePresentationAspect();
		assert(s_presentViewport.w == widths[ratio]);
		assert(s_presentViewport.h == heights[ratio]);
		assert(s_presentViewport.x == (1920 - widths[ratio]) / 2);
		assert(s_presentViewport.y == (1080 - heights[ratio]) / 2);
		int videoX, videoY, videoW, videoH;
		NativeRenderer_GetStreamingViewport(240, 240, &videoX, &videoY, &videoW, &videoH);
		assert(abs(videoW * 3 - videoH * 4) <= 3);
		assert(videoX == (1920 - videoW) / 2 && videoY == (1080 - videoH) / 2);
		int croppedH;
		NativeRenderer_GetStreamingViewport(180, 240, NULL, NULL, NULL, &croppedH);
		assert(croppedH == (videoH * 180 + 120) / 240);

		for (int players = 1; players <= 4; players++)
		{
			struct PushBuffer pb = {0};
			pb.rect.w = players >= 3 ? 256 : 512;
			pb.rect.h = players >= 2 ? 108 : 216;
			pb.distanceToScreen_PREV = 256;
			PushBuffer_UpdateFrustum(&pb);
			assert(pb.matrix_ViewProj.m[0][0] == (s16)(4096 * scales[ratio]));
			assert(pb.matrix_ViewProj.m[1][1] == 2304);
			NativeDraw3DView view;
			NativeDrawLevel_BuildView(&pb, &view);
			assert(fabs(view.rotation[0] - scales[ratio]) < 1e-10);
			assert(fabs(view.rotation[4] - 0.5625) < 1e-10);
			assert(view.width == pb.rect.w && view.height == pb.rect.h);
			// A triangle near the widened right edge remains submitted; a
			// triangle entirely beyond the edge is rejected before rasterisation.
			const float right = (float)(view.width * 0.5 / scales[ratio]);
			NativeDraw3DVertex a = {0}, b = {0}, c = {0};
			a.x = right - 8; b.x = right - 4; c.x = right - 8;
			a.z = b.z = c.z = 256; c.y = 2;
			NativeDraw3DMaterial material = {0};
			material.flags = NATIVE_DRAW3D_DOUBLE_SIDED;
			NativeDraw3D_BeginFrame();
			int layerIndex = NativeDraw3D_BeginLayer(&view);
			assert(NativeDraw3D_AddTriangle(layerIndex, &a, &b, &c, &material));
			a.x = right + 4; b.x = right + 8; c.x = right + 4;
			assert(!NativeDraw3D_AddTriangle(layerIndex, &a, &b, &c, &material));
			NativeDraw3D_EndLayer(layerIndex);
		}

		const int invisibleLeaves[] = {0};
		const int expanded = ratio == NATIVE_ASPECT_21_9 || ratio == NATIVE_ASPECT_32_9;
		assert(RenderLists_IsVisible(invisibleLeaves, 0) == expanded);
		assert((void *)RenderBucket_GetVisibleLevelInstances(&tracker.cameraDC[0]) ==
		       (expanded ? (void *)allInstances : (void *)pvsInstances));
		assert(fabs(AH_Map_MarkerAspectX() - (512.0 / 216.0 / (4.0 / 3.0)) * scales[ratio]) < 1e-6);

		// Map origin, artwork and markers move together toward the screen
		// edge; their physical inset remains the same at every ratio.
		struct UIMap map = {2000, 4000, 0, 0, 100, 80, 400, 180, 0};
		const s32 origin[3] = {0, 0, 0};
		tracker.numPlyrCurrGame = 1;
		float markerX, markerY;
		UI_Map_GetIconPosPrecise(&map, origin, &markerX, &markerY);
		const double expectedOffset = 112 * (1.0 - scales[ratio]);
		assert(fabs(NativeMinimap_GetAnchorOffsetX(&map) - expectedOffset) < 1e-5);
		assert(fabs(markerX - (400 + expectedOffset)) < 2e-5);
		assert(markerY == 164);
		const double mapRight = 400 + (500 - 400) * scales[ratio] + expectedOffset;
		assert(fabs((512 - mapRight) / scales[ratio] - 12) < 1e-10);
		// Rotated arrows/monitor geometry and their outlines use physical
		// pixel aspect, rather than the old fixed 8/5 horizontal multiplier.
		const SVec2 square[4] = {{.x = -4, .y = -4}, {.x = 4, .y = -4}, {.x = 4, .y = 4}, {.x = -4, .y = 4}};
		float x[4], y[4], outlineX[4], outlineY[4];
		const float pixelAspect = (float)NativeAspect_GetHudPixelAspectX();
		for (int angle = 0; angle < 4096; angle += 1024)
		{
			AH_Map_PreciseShape(100, 80, square, 4, 4096, 1, angle, x, y);
			for (int i = 0; i < 4; i++)
			{
				const int next = (i + 1) % 4;
				assert(fabs(hypot((x[next] - x[i]) / pixelAspect, y[next] - y[i]) - 8) < 2e-5);
			}
			AH_Map_PreciseOutline(x, y, 4, outlineX, outlineY);
			assert(fabs(hypot((outlineX[0] - x[0]) / pixelAspect, outlineY[0] - y[0]) - sqrt(2)) < 2e-5);
		}
		tracker.gameMode1 = MAIN_MENU;
		assert(NativeMinimap_GetAnchorOffsetX(&map) == 0);
		tracker.gameMode1 = 0;
	}

	// FOV scales the real world cameras, including the precise transform,
	// while UI and detached preview cameras retain their existing framing.
	gNativeAspectRatio = NATIVE_ASPECT_16_9;
	for (int degrees = 45; degrees <= 100; degrees += 5)
	{
		gNativeFovDegrees = degrees;
		const double focal = 0.75 / tan(degrees * 3.14159265358979323846 / 360.0);
		for (int camera = 0; camera < 4; camera++)
		{
			struct PushBuffer *pb = &tracker.pushBuffer[camera];
			memset(pb, 0, sizeof(*pb));
			pb->rect.w = 512; pb->rect.h = 216;
			pb->distanceToScreen_PREV = 256;
			PushBuffer_UpdateFrustum(pb);
			assert(pb->matrix_ViewProj.m[0][0] == (s16)(3072 * focal));
			assert(pb->matrix_ViewProj.m[1][1] == (s16)(2304 * focal));
			NativeDraw3DView view;
			NativeDrawLevel_BuildView(pb, &view);
			assert(fabs(view.rotation[0] - 0.75 * focal) < 1e-10);
			assert(fabs(view.rotation[4] - 0.5625 * focal) < 1e-10);
			assert(fabs(2 * atan(view.height / (2 * view.projection * view.rotation[4])) *
			            180 / 3.14159265358979323846 - degrees) < 1e-8);
		}
		struct PushBuffer *ui = &tracker.pushBuffer_UI;
		memset(ui, 0, sizeof(*ui));
		ui->rect.w = 512; ui->rect.h = 216;
		ui->distanceToScreen_PREV = 256;
		PushBuffer_SetMatrixVP(ui);
		assert(ui->matrix_ViewProj.m[0][0] == 3072);
		assert(ui->matrix_ViewProj.m[1][1] == 2304);
		assert(NativeAspect_UsesExpandedVisibility() == (degrees >= 75));
	}
	gNativeFovDegrees = 0;

	// Resizing a matching ultrawide display fills it without bars.
	gNativeAspectRatio = NATIVE_ASPECT_32_9;
	g_windowWidth = 3840;
	NativeRenderer_UpdatePresentationViewport();
	assert(s_presentViewport.w == 3840 && s_presentViewport.h == 1080);
	assert(s_presentViewport.x == 0 && s_presentViewport.y == 0);

	struct MenuRow rows[] = {{.stringIndex = NATIVE_MENU_STRING_ASPECT_RATIO}, {.stringIndex = RECTMENU_STRING_NONE}};
	struct RectMenu menu = s_nativeDisplayMenu;
	menu.rows = rows;
	menu.rowSelected = 0;
	menu.funcState = RECTMENU_FUNC_STATE_INPUT;
	assert(RECTMENU_NativeOptionsHorizontalInput(&menu));
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeAspectRatio == NATIVE_ASPECT_4_3);
	sdata->buttonTapPerPlayer[0] = BTN_LEFT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeAspectRatio == NATIVE_ASPECT_32_9);
	tracker.levelID = CRASH_COVE;
	tracker.gameMode1 = PAUSE_ALL;
	assert(tracker.levelID == CRASH_COVE && tracker.gameMode1 == PAUSE_ALL);
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeAspectRatio == NATIVE_ASPECT_4_3);
	assert((rows[0].stringIndex & MENU_ROW_LOCKED) == 0);
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_ASPECT_RATIO));
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_RENDERER));
	rows[0].stringIndex = NATIVE_MENU_STRING_FIELD_OF_VIEW;
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_FIELD_OF_VIEW));
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeFovDegrees == 45);
	sdata->buttonTapPerPlayer[0] = BTN_LEFT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeFovDegrees == 0);
	sdata->buttonTapPerPlayer[0] = BTN_LEFT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeFovDegrees == 100);
	gNativeFovDegrees = 47;
	sdata->buttonTapPerPlayer[0] = BTN_LEFT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeFovDegrees == 0);
	gNativeFovDegrees = 98;
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeFovDegrees == 0);
	rows[0].stringIndex = NATIVE_MENU_STRING_RENDERER;
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeRendererMode == NATIVE_RENDERER_CLASSIC);
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeRendererMode == NATIVE_RENDERER_NATIVE);
	tracker.levelID = MAIN_MENU_LEVEL;
	tracker.gameMode1 = 0;
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
	rows[0].stringIndex = NATIVE_MENU_STRING_ASPECT_RATIO;
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeAspectRatio == NATIVE_ASPECT_16_9);
	assert((rows[0].stringIndex & MENU_ROW_LOCKED) == 0);
	assert(strstr(RECTMENU_GetString(NATIVE_MENU_STRING_ASPECT_RATIO), "NATIVE"));
	assert(strstr(RECTMENU_GetString(NATIVE_MENU_STRING_ASPECT_RATIO), "16:9"));
	rows[0].stringIndex = NATIVE_MENU_STRING_FIELD_OF_VIEW;
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeFovDegrees == 45);
	assert((rows[0].stringIndex & MENU_ROW_LOCKED) == 0);
	assert(strstr(RECTMENU_GetString(NATIVE_MENU_STRING_FIELD_OF_VIEW), "(NATIVE)"));
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION;
	const int classicProjectionBefore = gNativeProjectionMode;
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_PROJECTION));
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionMode == (classicProjectionBefore + 1) % NATIVE_PROJECTION_MODE_COUNT);
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION_STRENGTH;
	const int classicStrengthBefore = gNativeProjectionStrength;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionStrength == (classicStrengthBefore > 90 ? 0 : classicStrengthBefore + 10));
	assert((rows[0].stringIndex & MENU_ROW_LOCKED) == 0);
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_PROJECTION_STRENGTH));
	gNativeFovDegrees = 0;
	NativeRenderer_UpdateGamePresentationAspect();
	assert(s_presentAspectW == 16 && s_presentAspectH == 9);
	int classicVideoW, classicVideoH;
	NativeRenderer_GetStreamingViewport(240, 240, NULL, NULL, &classicVideoW, &classicVideoH);
	assert(classicVideoW == s_presentViewport.w && classicVideoH == s_presentViewport.h);
	assert(RenderBucket_GetVisibleLevelInstances(&tracker.cameraDC[0]) == pvsInstances);
	assert(!RenderLists_IsVisible((const int[]){0}, 0));
	assert(CTR_WIDESCREEN_SCALE_X(4096) == 3094);
	assert(CTR_WIDESCREEN_SCALE_X(1) == 0);
	NativeRenderer_EnableGamePresentation(0);
	assert(s_presentAspectW == 4 && s_presentAspectH == 3);
	gNativeAspectRatio = previousAspect;
	gNativeFovDegrees = previousFov;
	gNativeProjectionMode = previousProjection;
	gNativeProjectionStrength = previousProjectionStrength;
	sdata->gGT = NULL;
}

// A world-space square must still occupy a square on the physical framebuffer
// after projection and the renderer's logical-to-physical target mapping.
static void AspectTest_RenderTargets(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
	gNativeDitheringEnabled = 0;
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	Platform_InitScratchpad();
	NativeRenderer_EnableGamePresentation(1);
	for (int ratio = 0; ratio < NATIVE_ASPECT_COUNT; ratio++)
	{
		gNativeAspectRatio = ratio;
		NativePgxp_EndFrame();
		ClearSplits();
		SetDefDrawEnv(&activeDrawEnv, 0, 0, 512, 216);
		SetDefDispEnv(&activeDispEnv, 0, 0, 512, 216);
		activeDrawEnv.isbg = 1;
		activeDrawEnv.dfe = 1;
		NativeRenderer_BeginScene();
		NativeRenderer_Clear(0, 0, 512, 216, 0, 0, 0);
		struct PushBuffer pb = {0};
		pb.rect.w = 512; pb.rect.h = 216;
		pb.distanceToScreen_PREV = 256;
		PushBuffer_SetMatrixVP(&pb);
		NativeDraw3DView view;
		NativeDrawLevel_BuildView(&pb, &view);
		NativeDraw3D_BeginFrame();
		int layer = NativeDraw3D_BeginLayer(&view);
		NativeDraw3DVertex a = {.x = -50, .y = -50, .z = 256, .r = 255};
		NativeDraw3DVertex b = {.x = 50, .y = -50, .z = 256, .r = 255};
		NativeDraw3DVertex c = {.x = -50, .y = 50, .z = 256, .r = 255};
		NativeDraw3DVertex d = {.x = 50, .y = 50, .z = 256, .r = 255};
		NativeDraw3DMaterial material = {.flags = NATIVE_DRAW3D_DOUBLE_SIDED};
		assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &material));
		assert(NativeDraw3D_AddTriangle(layer, &b, &d, &c, &material));
		DepthTest_Marker(0, layer);
		void *packets[] = {&depthTestMarkers[0]};
		DepthTest_DrawPackets(packets, 1);
		const struct NativeRenderTarget *target = NativeRenderer_ResolveMainRenderTarget();
		assert(target->width == s_presentViewport.w && target->height == s_presentViewport.h);
		u8 *pixels = malloc((size_t)target->width * target->height * 4);
		assert(pixels);
		glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
		glReadPixels(0, 0, target->width, target->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		int left = target->width, right = -1, bottom = target->height, top = -1;
		for (int y = 0; y < target->height; y++) for (int x = 0; x < target->width; x++)
		{
			if (pixels[((size_t)y * target->width + x) * 4] < 100) continue;
			if (x < left) left = x;
			if (x > right) right = x;
			if (y < bottom) bottom = y;
			if (y > top) top = y;
		}
		free(pixels);
		assert(right >= left && top >= bottom);
		assert(abs((right - left) - (top - bottom)) <= 1);
		assert(glGetError() == GL_NO_ERROR);
		NativeRenderer_EndScene();
	}
}
