// Headless checks of the same camera-yaw update used during and after a drift.
static void DriftCameraTest(void)
{
	struct GameTracker tracker = {0};
	struct GameTracker *previous = sdata->gGT;
	const int previousRate = gNative60FpsEnabled;
	const int previousSteering = gNativeSmoothedSteeringEnabled;
	const int previousForce30 = gNativeForce30Fps;
	const int previousOverride = gNativeGhostReplayFpsOverride;
	int failures = 0;
	sdata->gGT = &tracker;
	gNativeForce30Fps = 0;
	gNativeGhostReplayFpsOverride = -1;
	const int offsets[] = {-512, -128, -1, 1, 128, 512};
	const int targets[] = {-256, 0, 256};
	for (int smoothed = 0; smoothed <= 1; smoothed++)
	{
		gNativeSmoothedSteeringEnabled = smoothed;
		for (int index = 0; index < NATIVE_FRAME_RATE_COUNT; index++)
		{
			gNative60FpsEnabled = index;
			const int rate = CTR_FRAMES_PER_SECOND;
			for (unsigned int t = 0; t < len(targets); t++)
			{
				for (unsigned int i = 0; i < len(offsets); i++)
				{
					struct Driver driver = {0};
					driver.const_DriftCameraLerpStep = 32;
					driver.rotCurr.w = targets[t] + offsets[i];
					NativePhysics_Reset();
					tracker.timer = 0;
					tracker.elapsedTimeMS = 0;
					PhysLerpRot(&driver, targets[t]);
					assert(driver.rotCurr.w == targets[t] + offsets[i]);
					for (int frame = 0; frame < rate * 8; frame++)
					{
						tracker.timer = frame;
						tracker.elapsedTimeMS = CTR_FRAME_STEP(32, frame);
						PhysLerpRot(&driver, targets[t]);
					}
					if (driver.rotCurr.w != targets[t])
					{
						fprintf(stderr, "Camera stuck: %d FPS, smoothed=%d, offset=%d, target=%d, yaw=%d\n", rate, smoothed, offsets[i], targets[t],
						        driver.rotCurr.w);
						failures++;
					}
				}
			}
		}
	}
	gNative60FpsEnabled = previousRate;
	gNativeSmoothedSteeringEnabled = previousSteering;
	gNativeForce30Fps = previousForce30;
	gNativeGhostReplayFpsOverride = previousOverride;
	NativePhysics_Reset();
	sdata->gGT = previous;
	assert(failures == 0);
}
