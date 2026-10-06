// Exercise pause-menu camera changes through the real controller input path.
// CTest runs this in its own directory because the menu persists the setting.
static void CameraOptionsTest(void)
{
	struct GameTracker tracker = {0};
	struct GamepadSystem pads = {0};
	sdata->gGT = &tracker;
	sdata->gGamepads = &pads;
	gNativePresetPending = 0;
	const u32 buttons[] = {BTN_RIGHT, BTN_LEFT, BTN_CROSS_one};
	for (int players = 1; players <= 4; players++)
	{
		tracker.levelID = 0;
		tracker.gameMode1 = PAUSE_ALL;
		tracker.numPlyrCurrGame = tracker.numPlyrNextGame = players;
		MM_NativeOptionsConfigureRows(1);
		struct RectMenu menu = s_nativeUiMenu;
		menu.state |= MUTE_SOUND_OF_MOVING_CURSOR;
		for (int row = 0; menu.rows[row].stringIndex != RECTMENU_STRING_NONE; row++)
			if ((menu.rows[row].stringIndex & MENU_ROW_LNG_MASK) == NATIVE_MENU_STRING_DEFAULT_CAMERA)
				menu.rowSelected = row;
		sdata->activeSubMenu = &menu;
		gNativeDefaultCameraFar = 0;
		for (unsigned int player = 0; player < len(tracker.cameraDC); player++)
		{
			tracker.cameraDC[player].nearOrFar = player < (unsigned int)players ? 0 : 7;
			tracker.cameraDC[player].zoomToggleState = tracker.cameraDC[player].nearOrFar;
			tracker.cameraDC[player].cameraMode = CAMERA_MODE_FREECAM;
			tracker.cameraDC[player].flags = CAMERA_FLAG_TRANSITION_BACK;
		}
		for (unsigned int press = 0; press < len(buttons); press++)
		{
			pads.gamepad[0].buttonsTapped = buttons[press];
			RECTMENU_CollectInput();
			RECTMENU_ProcessInput(&menu);
			const int expected = (press + 1) & 1;
			assert(gNativeDefaultCameraFar == expected);
			for (unsigned int player = 0; player < len(tracker.cameraDC); player++)
			{
				assert(tracker.cameraDC[player].nearOrFar == (player < (unsigned int)players ? expected : 7));
				assert(tracker.cameraDC[player].zoomToggleState == (player < (unsigned int)players ? expected : 7));
				assert(tracker.cameraDC[player].cameraMode == CAMERA_MODE_FREECAM);
				assert(tracker.cameraDC[player].flags == CAMERA_FLAG_TRANSITION_BACK);
			}
		}

		// The main-menu setting remains a default for future races.
		tracker.levelID = MAIN_MENU_LEVEL;
		tracker.gameMode1 = MAIN_MENU;
		pads.gamepad[0].buttonsTapped = BTN_RIGHT;
		RECTMENU_CollectInput();
		RECTMENU_ProcessInput(&menu);
		assert(gNativeDefaultCameraFar == 0);
		for (int player = 0; player < players; player++)
			assert(tracker.cameraDC[player].nearOrFar == 1);
	}
	FILE *config = fopen(NativeConfig_GetPath(), "r");
	assert(config != NULL);
	char line[256];
	int persisted = -1;
	while (fgets(line, sizeof(line), config) != NULL)
		sscanf(line, "default_camera_far=%d", &persisted);
	fclose(config);
	assert(persisted == gNativeDefaultCameraFar);
	sdata->activeSubMenu = NULL;
	sdata->gGT = NULL;
	sdata->gGamepads = NULL;
}
