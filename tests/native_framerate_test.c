#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <macros.h>

int gNative60FpsEnabled;
int gNativeForce30Fps;
int gNativeGhostReplayFpsOverride = -1;

int main(void)
{
	const int rates[] = {30, 60, 90, 120, 144, 240};
	const int steps[] = {0, 1, -1, 7, -7, 32, 4096, -4096};
	for (int option = 0; option < NATIVE_FRAME_RATE_COUNT; option++)
	{
		int rate = rates[option];
		gNative60FpsEnabled = option;
		assert(CTR_FRAMES_PER_SECOND == rate);
		assert(NativeFrameRate_Index(rate) == option);
		assert(FPS_DOUBLE(30) == rate);
		assert(FPS_HALF(rate) == 30);
		int ticks = 0, elapsed = 0;
		for (unsigned int frame = 0; frame < (unsigned int)rate; frame++)
		{
			ticks += CTR_RETAIL_FRAME_TICK(frame);
			elapsed += CTR_FRAME_STEP(32, frame);
		}
		assert(ticks == 30);
		assert(elapsed == 960);
		for (unsigned int s = 0; s < sizeof(steps) / sizeof(steps[0]); s++)
		{
			int total = 0;
			for (unsigned int frame = 0; frame < (unsigned int)rate; frame++)
				total += CTR_FRAME_STEP(steps[s], frame);
			assert(total == steps[s] * 30);
		}
		gNativeForce30Fps = 1;
		assert(CTR_FRAMES_PER_SECOND == 30);
		assert(FPS_DOUBLE(30) == 30);
		gNativeForce30Fps = 0;
		gNativeGhostReplayFpsOverride = 1;
		assert(CTR_FRAMES_PER_SECOND == 60);
		gNativeGhostReplayFpsOverride = -1;
	}
	assert(NativeFrameRate_Index(100) == -1);
	assert(NativeFrameRate_FromIndex(-1) == 30);
	assert(NativeFrameRate_FromIndex(99) == 30);
	for (unsigned int frame = 0; frame < 120; frame++)
	{
		assert(NativeFrameRate_Tick(frame, 60) == ((frame & 1) != 0));
		assert(NativeFrameRate_Step(-7, frame, 60) == ((frame & 1) ? -4 : -3));
	}
	/* No overflow when a long-lived frame counter or signed step is large. */
	assert(NativeFrameRate_Step(INT_MAX, UINT_MAX, 240) > 0);
	assert(NativeFrameRate_Step(INT_MIN, UINT_MAX, 240) < 0);
	return 0;
}
