// Run the source-owned boss scripts through the real interpreter and driving
// handoff, without requiring a display or racing through Adventure first.
static void BossCutsceneTest(void)
{
	struct GameTracker tracker = {0};
	struct Driver driver = {0};
	struct Thread driverThread = {0};
	struct Instance driverInst = {0};
	struct ModelAnim anim = {0};
	struct ModelAnim *anims[] = {&anim};
	struct ModelHeader header = {0};
	struct Model model = {0};
	struct GameTracker *previous = sdata->gGT;
	sdata->gGT = &tracker;
	tracker.drivers[0] = &driver;
	driver.instSelf = &driverInst;
	driverInst.thread = &driverThread;
	header.numAnimations = 1;
	header.ptrAnimations = anims;
	model.numHeaders = 1;
	model.headers = &header;
	anim.numFrames = 250;
	const int scripts[] = {ROO_START, ROO_BEAT, PAPU_START, PAPU_BEAT, KJOE_START, KJOE_BEAT, PINSTRIPE_START, PINSTRIPE_BEAT, OXIDE_TROPHIES};
	const int deltas[] = {32, 16, 8, 4};
	for (unsigned int d = 0; d < len(deltas); d++)
	{
		for (unsigned int script = 0; script < len(scripts); script++)
		{
			struct CutsceneObj cs = {0};
			struct Instance head = {0};
			head.model = &model;
			cs.metadataMeta = &cs.decodedOpcode;
			cs.flags = CS_FLAG_XA_SYNC_ANIMATION | CS_FLAG_XA_PLAYBACK_STARTED;
			// Begin immediately after PLAY_XA. Feed the same sample offset that
			// native audio supplies, then signal completion after ten seconds.
			CS_ScriptCmd_OpcodeAt(&cs, R233.bossCS[scripts[script]].opcode + 0x20);
			D233.isCutsceneOver = 0;
			D233.CutsceneManipulatesAudio = 0;
			D233.cutsceneState = CS_WAIT_END;
			tracker.levelID = N_SANITY_BEACH;
			tracker.overlayIndex_Threads = OVERLAY_INDEX_PODIUMS;
			tracker.overlayTransition = 0;
			tracker.gameMode2 = VEH_FREEZE_PODIUM;
			tracker.pushBuffer_UI.fadeFromBlack_currentValue = FP_ONE;
			tracker.pushBuffer_UI.fadeFromBlack_desiredResult = FP_ONE;
			tracker.pushBuffer_UI.fade_step = 0;
			driver.kartState = KS_FREEZE;
			driver.funcPtrs[DRIVER_FUNC_INIT] = NULL;
			driverInst.flags = HIDE_MODEL;
			driverThread.flags = THREAD_FLAG_DISABLE_COLLISION;
			int time = 0;
			for (; time < 15 * 960 && !D233.isCutsceneOver; time += deltas[d])
			{
				tracker.elapsedTimeMS = deltas[d];
				sdata->XA_State = time < 10 * 960 ? XA_PLAYING : XA_IDLE;
				sdata->XA_CurrOffset = (int)(((long long)time * 44100) / 960);
				CS_Thread_UseOpcode(&head, &cs);
				struct PushBuffer *pb = &tracker.pushBuffer_UI;
				int fade = pb->fadeFromBlack_currentValue + pb->fade_step;
				if (pb->fade_step < 0 && fade < pb->fadeFromBlack_desiredResult)
					fade = pb->fadeFromBlack_desiredResult;
				pb->fadeFromBlack_currentValue = fade;
			}
			if (scripts[script] == PINSTRIPE_BEAT)
			{
				assert(D233.cutsceneState == CS_WAIT_INPUT);
				assert(D233.bossCutsceneIndex == OXIDE_TROPHIES);
				continue;
			}
			assert(D233.isCutsceneOver);
			assert(!(tracker.gameMode2 & VEH_FREEZE_PODIUM));
			assert(!(driverInst.flags & HIDE_MODEL));
			assert(!(driverThread.flags & THREAD_FLAG_DISABLE_COLLISION));
			assert(driver.funcPtrs[DRIVER_FUNC_INIT] == VehPhysProc_Driving_Init);
			// Driving init must remain queued until the hub overlay is ready.
			VehPhysProc_Driving_Init(&driverThread, &driver);
			assert(driver.funcPtrs[DRIVER_FUNC_INIT] == VehPhysProc_Driving_Init);
			tracker.overlayIndex_Threads = OVERLAY_INDEX_ADV_HUB;
			VehPhysProc_Driving_Init(&driverThread, &driver);
			assert(driver.kartState == KS_NORMAL);
			assert(driver.funcPtrs[DRIVER_FUNC_PHYS_LINEAR] == VehPhysProc_Driving_PhysLinear);
		}
	}
	sdata->gGT = previous;
}

static void BossDoorControlTest(void)
{
	struct GameTracker tracker = {0};
	struct GameTracker *previous = sdata->gGT;
	struct AdvProgress previousProgress = sdata->advProgress;
	const int previousSkipHints = gNativeSkipMaskHints;
	const s16 previousRequestedHint = sdata->AkuHint_RequestedHint;
	const int previousHintState = sdata->AkuAkuHintState;
	struct Driver driver = {0};
	struct Instance driverInst = {0};
	struct Thread driverThread = {0};
	struct Instance doorInst = {0};
	struct InstDef doorDef = {0};
	struct Thread doorThread = {0};
	struct WoodDoor door = {0};
	tracker.drivers[0] = &driver;
	tracker.levelID = N_SANITY_BEACH;
	tracker.gameMode1 = ADVENTURE_MODE | ADVENTURE_ARENA;
	tracker.overlayIndex_Threads = OVERLAY_INDEX_ADV_HUB;
	tracker.currAdvProfile.numKeys = 1;
	driver.instSelf = &driverInst;
	driverInst.thread = &driverThread;
	doorInst.instDef = &doorDef;
	doorThread.inst = &doorInst;
	doorThread.object = &door;
	door.doorID = AH_DOOR_BEACH_TO_GEMSTONE_VALLEY;
	sdata->gGT = &tracker;
	for (int skipHints = 1; skipHints >= 0; skipHints--)
	{
		for (int hintUnlocked = 0; hintUnlocked <= 1; hintUnlocked++)
		{
			memset(&sdata->advProgress, 0, sizeof(sdata->advProgress));
			if (hintUnlocked)
				UNLOCK_ADV_BIT(sdata->advProgress.rewards, ADV_REWARD_HINT_NEW_WORLD_GREETING);
			gNativeSkipMaskHints = skipHints;
			sdata->AkuHint_RequestedHint = -1;
			sdata->AkuAkuHintState = 0;
			tracker.gameMode2 = VEH_FREEZE_DOOR;
			tracker.cameraDC[0].flags = CAMERA_FLAG_TRANSITION_AWAY | CAMERA_FLAG_TRANSITION_HOLD;
			door.camFlags = WdCam_CutscenePlaying | WdCam_FlyingOut | WdCam_FullyOut;
			door.doorRot.y = AH_DOOR_OPEN_ROTATION;
			door.hudFlags = HUD_FLAG_RACE_HUD;
			driver.kartState = KS_FREEZE;
			AH_Door_ThTick(&doorThread);
			assert(sdata->advProgress.storyFlags & ADV_REWARD_DOOR_BEACH_TO_GEMSTONE_VALLEY_MASK);
			// Finish the camera return and request the greeting, exactly as the
			// next door ticks do after receiving Ripper Roo's first key.
			tracker.cameraDC[0].flags = 0;
			AH_Door_ThTick(&doorThread);
			if (skipHints || hintUnlocked)
			{
				assert(sdata->AkuHint_RequestedHint == -1);
				assert(sdata->AkuAkuHintState == 0);
			}
			else
			{
				assert(sdata->AkuHint_RequestedHint == ADV_MASK_HINT_ID_NEW_WORLD_GREETING);
				assert(driver.funcPtrs[DRIVER_FUNC_INIT] == VehPhysProc_FreezeEndEvent_Init);
			}
			assert(!(tracker.gameMode2 & GAME_MODE2_VEH_FREEZE_MASK));
			if (skipHints || hintUnlocked)
			{
				VehPhysProc_Driving_Init(&driverThread, &driver);
				assert(driver.kartState == KS_NORMAL);
				assert(driver.funcPtrs[DRIVER_FUNC_PHYS_LINEAR] == VehPhysProc_Driving_PhysLinear);
			}
		}
	}
	gNativeSkipMaskHints = previousSkipHints;
	sdata->AkuHint_RequestedHint = previousRequestedHint;
	sdata->AkuAkuHintState = previousHintState;
	sdata->advProgress = previousProgress;
	sdata->gGT = previous;
}
