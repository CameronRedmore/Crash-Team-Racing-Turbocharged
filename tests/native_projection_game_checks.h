// Included from the no-GL renderer integration executable after main.c.
// Exercises the real game camera, runtime gates, options handler and OT barriers.
#include <assert.h>

static int ProjectionTest_IsMarker(const void *node, int *cameraId)
{
	if (getlen(node) != sizeof(NativeProjectionMarker) / sizeof(u32) - P_LEN) return 0;
	u32 code;
	memcpy(&code, (const u8 *)node + sizeof(u32), sizeof(code));
	if ((code & 0xff000000u) != 0xB4000000u) return 0;
	*cameraId = (int)(code & 0x00ffffffu);
	return 1;
}

static void ProjectionTest_CheckBarriers(void)
{
	static u32 tempOT[4102];
	static POLY_F3 worldPrimitive[4];
	static POLY_F3 uiPrimitive;
	struct GameTracker tracker = {0};
	struct GameTracker *previousGT = sdata->gGT;
	int previousMode = gNativeProjectionMode;
	int previousStrength = gNativeProjectionStrength;
	int previousRenderer = gNativeRendererMode;
	int previousPresetPending = gNativePresetPending;
	gNativePresetPending = 1;
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeProjectionMode = NATIVE_PROJECTION_PANINI;
	gNativeProjectionStrength = 50;
	sdata->gGT = &tracker;
	for (int count = 1; count <= 4; ++count)
	{
		tracker.numPlyrCurrGame = count;
		tracker.gameMode1 = 0;
		NativeGpuLinks_Reset();
		NativeGpuLinks_RegisterRangeChecked("projection test OT", tempOT, sizeof(tempOT));
		NativeGpuLinks_RegisterRangeChecked("projection test primitives", worldPrimitive, sizeof(worldPrimitive));
		NativeGpuLinks_RegisterRangeChecked("projection test UI primitive", &uiPrimitive, sizeof(uiPrimitive));
		ClearOTagR(tempOT, (count << 10) | 6);
		for (int camera = 0; camera < count; ++camera)
		{
			PushBuffer_Init(&tracker.pushBuffer[camera], camera, count);
			tracker.pushBuffer[camera].ptrOT = tempOT + (count - camera - 1) * 1024 + 6;
		}
		tracker.pushBuffer_UI.ptrOT = tempOT + 1;
		MainFrame_AppendProjectionMarkers(&tracker);
		for (int camera = 0; camera < count; ++camera)
		{
			setPolyF3(&worldPrimitive[camera]);
			AddPrim(&tracker.pushBuffer[camera].ptrOT[0], &worldPrimitive[camera]);
		}
		setPolyF3(&uiPrimitive);
		AddPrim(&tracker.pushBuffer_UI.ptrOT[3], &uiPrimitive);

		int expectedCamera = 1;
		int activeCamera = -1;
		int sawCameraPrimitive[4] = {0};
		int sawEnd = 0;
		int sawUI = 0;
		int steps = 0;
		void *node = &tracker.pushBuffer[0].ptrOT[1023];
		while (node != NULL && steps++ < 10000)
		{
			int cameraId = -1;
			if (ProjectionTest_IsMarker(node, &cameraId))
			{
				if (activeCamera >= 0) assert(sawCameraPrimitive[activeCamera]);
				if (cameraId == 0)
				{
					assert(expectedCamera == count + 1);
					sawEnd = 1;
					activeCamera = -1;
				}
				else
				{
					assert(cameraId == expectedCamera);
					assert(cameraId >= 1 && cameraId <= count);
					activeCamera = cameraId - 1;
					++expectedCamera;
				}
			}
			else if (node == &uiPrimitive)
			{
				assert(sawEnd);
				sawUI = 1;
			}
			else
			{
				for (int camera = 0; camera < count; ++camera)
				{
					if (node == &worldPrimitive[camera])
					{
						assert(!sawEnd && activeCamera == camera);
						sawCameraPrimitive[camera] = 1;
					}
				}
				if (sawEnd)
				{
					const uintptr_t p = (uintptr_t)node;
					const uintptr_t begin = (uintptr_t)tempOT;
					const uintptr_t end = begin + sizeof(tempOT);
					assert((p >= begin && p < end) || node == &uiPrimitive);
				}
			}
			node = nextPrim(node);
		}
		assert(node == NULL && steps < 10000);
		assert(expectedCamera == count + 1);
		assert(sawEnd && sawUI);
		for (int camera = 0; camera < count; ++camera) assert(sawCameraPrimitive[camera]);
	}
	NativeGpuLinks_Reset();
	sdata->gGT = previousGT;
	gNativeRendererMode = previousRenderer;
	gNativeProjectionMode = previousMode;
	gNativeProjectionStrength = previousStrength;
	gNativePresetPending = previousPresetPending;
}

static void ProjectionTest_GamePaths(void)
{
	static const double aspectScale[] = {1.0, 0.75, 5.0 / 6.0, 4.0 / 7.0, 3.0 / 8.0};
	struct GameTracker tracker = {0};
	struct GameTracker *previousGT = sdata->gGT;
	const int previousRenderer = gNativeRendererMode;
	const int previousAspect = gNativeAspectRatio;
	const int previousFov = gNativeFovDegrees;
	const int previousMode = gNativeProjectionMode;
	const int previousStrength = gNativeProjectionStrength;
	const int previousPgxp = gNativePgxpMode;
	const int previousDepth = gNativeDepthBufferEnabled;
	const int previousPresetPending = gNativePresetPending;
	const u32 previousButtons = sdata->buttonTapPerPlayer[0];
	const int previousWindowW = g_windowWidth;
	const int previousWindowH = g_windowHeight;
	sdata->gGT = &tracker;
	tracker.levelID = MAIN_MENU_LEVEL;
	tracker.numPlyrCurrGame = 4;
	gNativePresetPending = 1;
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeProjectionStrength = 50;
	gNativeProjectionMode = NATIVE_PROJECTION_PANINI;
	assert(NativeProjection_IsGameplayActive());
	assert(NativeProjection_GetWorldOverscan() == 1.0);
	tracker.gameMode1 = MAIN_MENU;
	assert(!NativeProjection_IsGameplayActive());
	tracker.gameMode1 = LOADING;
	assert(!NativeProjection_IsGameplayActive());
	tracker.gameMode1 = GAME_CUTSCENE;
	assert(!NativeProjection_IsGameplayActive());
	tracker.gameMode1 = PAUSE_ALL;
	assert(!NativeProjection_IsGameplayActive());
	tracker.gameMode1 = 0;
	gNativeProjectionMode = NATIVE_PROJECTION_EDGE;
	assert(NativeProjection_IsGameplayActive());
	NativeProjectionParams edgeParams;
	assert(NativeProjection_BuildParams(NATIVE_PROJECTION_EDGE, 50, 1.0, &edgeParams));
	assert(fabs(NativeProjection_GetWorldOverscan() - edgeParams.overscan) < 1e-12);
	assert(NativeAspect_UsesExpandedVisibility());
	gNativeProjectionMode = NATIVE_PROJECTION_PERSPECTIVE;
	assert(!NativeProjection_IsGameplayActive());
	assert(NativeProjection_GetWorldOverscan() == 1.0);
	gNativeProjectionMode = NATIVE_PROJECTION_EDGE;
	gNativeProjectionStrength = 0;
	assert(!NativeProjection_IsGameplayActive());
	gNativeProjectionStrength = 50;
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
	assert(!NativeProjection_IsGameplayActive());
	assert(NativeProjection_GetWorldOverscan() == 1.0);
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;

	Platform_InitScratchpad();
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	for (int mode = NATIVE_PROJECTION_PERSPECTIVE; mode < NATIVE_PROJECTION_MODE_COUNT; ++mode)
	{
		gNativeProjectionMode = mode;
		gNativeProjectionStrength = mode == NATIVE_PROJECTION_PERSPECTIVE ? 0 : 50;
		for (int ratio = 0; ratio < NATIVE_ASPECT_COUNT; ++ratio)
		{
			gNativeAspectRatio = ratio;
			for (int fovIndex = 0; fovIndex < 3; ++fovIndex)
			{
				const int degrees[] = {0, 45, 100};
				gNativeFovDegrees = degrees[fovIndex];
				const double focal = degrees[fovIndex] == 0 ? 1.0 :
					0.75 / tan(degrees[fovIndex] * 3.14159265358979323846 / 360.0);
				for (int camera = 0; camera < 4; ++camera)
				{
					struct PushBuffer *pb = &tracker.pushBuffer[camera];
					memset(pb, 0, sizeof(*pb));
					pb->rect.w = 512;
					pb->rect.h = 216;
					pb->distanceToScreen_PREV = 256;
					NativePgxp_EndFrame();
					PushBuffer_SetMatrixVP(pb);
					const double overscan = (mode == NATIVE_PROJECTION_EDGE) ?
						NativeProjection_GetWorldOverscan() : 1.0;
					const int32_t integerAspectX = NativeAspect_ScaleX(4096);
					assert(pb->matrix_ViewProj.m[0][0] == (s16)(integerAspectX * focal / overscan));
					assert(pb->matrix_ViewProj.m[1][1] == (s16)(2304.0 * focal));
					double preciseRotation[9], preciseTranslation[3];
					NativePgxp_GetTransform(&pb->matrix_ViewProj, &pb->matrix_ViewProj.m[0][0],
						pb->matrix_ViewProj.t, preciseRotation, preciseTranslation);
					assert(fabs(preciseRotation[0] / 4096.0 - aspectScale[ratio] * focal / overscan) < 1e-10);
					assert(fabs(preciseRotation[4] / 4096.0 - 0.5625 * focal) < 1e-10);
				}
				struct PushBuffer *ui = &tracker.pushBuffer_UI;
				memset(ui, 0, sizeof(*ui));
				ui->rect.w = 512;
				ui->rect.h = 216;
				ui->distanceToScreen_PREV = 256;
				PushBuffer_SetMatrixVP(ui);
				assert(ui->matrix_ViewProj.m[0][0] == (s16)NativeAspect_ScaleX(4096));
				assert(ui->matrix_ViewProj.m[1][1] == 2304);
			}
		}
	}

	// Projection controls are editable in a paused race. Changes stay stored
	// under Classic and are displayed as Native preferences.
	struct MenuRow rows[2] = {
		{.stringIndex = NATIVE_MENU_STRING_PROJECTION},
		{.stringIndex = RECTMENU_STRING_NONE}
	};
	struct RectMenu menu = {0};
	menu.rows = rows;
	menu.rowSelected = 0;
	menu.funcState = RECTMENU_FUNC_STATE_INPUT;
	gNativeProjectionMode = NATIVE_PROJECTION_PERSPECTIVE;
	gNativeProjectionStrength = 50;
	sdata->buttonTapPerPlayer[0] = BTN_LEFT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionMode == NATIVE_PROJECTION_EDGE);
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionMode == NATIVE_PROJECTION_PERSPECTIVE);
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionMode == NATIVE_PROJECTION_PANINI);
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION_STRENGTH;
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionStrength == 60);
	sdata->buttonTapPerPlayer[0] = BTN_LEFT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionStrength == 50);
	tracker.levelID = CRASH_COVE;
	tracker.gameMode1 = PAUSE_ALL;
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION;
	const int pausedMode = gNativeProjectionMode;
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_PROJECTION));
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionMode == (pausedMode + 1) % NATIVE_PROJECTION_MODE_COUNT);
	assert((rows[0].stringIndex & MENU_ROW_LOCKED) == 0);
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION_STRENGTH;
	const int pausedStrength = gNativeProjectionStrength;
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_PROJECTION_STRENGTH));
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionStrength == (pausedStrength > 90 ? 0 : pausedStrength + 10));
	assert((rows[0].stringIndex & MENU_ROW_LOCKED) == 0);
	rows[0].stringIndex = NATIVE_MENU_STRING_DEPTH_BUFFER;
	const int depthPreference = gNativeDepthBufferEnabled;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeDepthBufferEnabled == !depthPreference);
	assert(NATIVE_DEPTH_BUFFER_ACTIVE());
	rows[0].stringIndex = NATIVE_MENU_STRING_PGXP;
	const int pausedPgxp = gNativePgxpMode;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativePgxpMode != pausedPgxp);
	tracker.gameMode1 = 0;
	tracker.levelID = MAIN_MENU_LEVEL;
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION;
	const int classicMode = gNativeProjectionMode;
	const int classicStrength = gNativeProjectionStrength;
	assert(!MM_NativeOptionsRowLockedInRace(NATIVE_MENU_STRING_PROJECTION));
	sdata->buttonTapPerPlayer[0] = BTN_RIGHT;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionMode == (classicMode + 1) % NATIVE_PROJECTION_MODE_COUNT);
	assert(strstr(RECTMENU_GetString(NATIVE_MENU_STRING_PROJECTION), "(NATIVE)"));
	rows[0].stringIndex = NATIVE_MENU_STRING_PROJECTION_STRENGTH;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeProjectionStrength == (classicStrength > 90 ? 0 : classicStrength + 10));
	assert(strstr(RECTMENU_GetString(NATIVE_MENU_STRING_PROJECTION_STRENGTH), "(NATIVE)"));
	rows[0].stringIndex = NATIVE_MENU_STRING_DEPTH_BUFFER;
	const int classicDepth = gNativeDepthBufferEnabled;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativeDepthBufferEnabled == !classicDepth);
	rows[0].stringIndex = NATIVE_MENU_STRING_PGXP;
	const int classicPgxp = gNativePgxpMode;
	MM_NativeOptionsMenuProc(&menu);
	assert(gNativePgxpMode != classicPgxp);
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;

	// Pausing disables world-projection overscan; resuming applies the chosen
	// Edge setting and FOV to world cameras while leaving UI projection alone.
	tracker.levelID = CRASH_COVE;
	gNativeAspectRatio = NATIVE_ASPECT_16_9;
	gNativeFovDegrees = 100;
	gNativeProjectionMode = NATIVE_PROJECTION_EDGE;
	gNativeProjectionStrength = 50;
	tracker.gameMode1 = PAUSE_ALL;
	assert(!NativeProjection_IsGameplayActive());
	assert(NativeProjection_GetWorldOverscan() == 1.0);
	struct PushBuffer *world = &tracker.pushBuffer[0];
	memset(world, 0, sizeof(*world));
	world->rect.w = 512; world->rect.h = 216; world->distanceToScreen_PREV = 256;
	NativePgxp_EndFrame(); PushBuffer_SetMatrixVP(world);
	const double focal = 0.75 / tan(100 * 3.14159265358979323846 / 360.0);
	assert(world->matrix_ViewProj.m[0][0] == (s16)(NativeAspect_ScaleX(4096) * focal));
	double preciseRotation[9], preciseTranslation[3];
	NativePgxp_GetTransform(&world->matrix_ViewProj, &world->matrix_ViewProj.m[0][0],
	                        world->matrix_ViewProj.t, preciseRotation, preciseTranslation);
	assert(fabs(preciseRotation[0] / 4096.0 - 0.75 * focal) < 1e-10);
	tracker.gameMode1 = 0;
	assert(NativeProjection_IsGameplayActive());
	const double resumedOverscan = NativeProjection_GetWorldOverscan();
	assert(resumedOverscan > 1.0);
	NativePgxp_EndFrame(); PushBuffer_SetMatrixVP(world);
	assert(world->matrix_ViewProj.m[0][0] == (s16)(NativeAspect_ScaleX(4096) * focal / resumedOverscan));
	NativePgxp_GetTransform(&world->matrix_ViewProj, &world->matrix_ViewProj.m[0][0],
	                        world->matrix_ViewProj.t, preciseRotation, preciseTranslation);
	assert(fabs(preciseRotation[0] / 4096.0 - 0.75 * focal / resumedOverscan) < 1e-10);

	ProjectionTest_CheckBarriers();

	sdata->gGT = previousGT;
	gNativeRendererMode = previousRenderer;
	gNativeAspectRatio = previousAspect;
	gNativeFovDegrees = previousFov;
	gNativeProjectionMode = previousMode;
	gNativeProjectionStrength = previousStrength;
	gNativePgxpMode = previousPgxp;
	gNativeDepthBufferEnabled = previousDepth;
	gNativePresetPending = previousPresetPending;
	sdata->buttonTapPerPlayer[0] = previousButtons;
	g_windowWidth = previousWindowW;
	g_windowHeight = previousWindowH;
}
