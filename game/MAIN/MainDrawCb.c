#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034a80-0x80034aa4.
void MainDrawCb_DrawSync()
{
	struct GameTracker *gGT;
	gGT = sdata->gGT;

	if (gGT->bool_DrawOTag_InProgress == 1)
	{
		gGT->bool_DrawOTag_InProgress = 0;
	}

	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034aa4-0x80034b48 for the retail body.
// NOTE(aalhendi): CTR_NATIVE keeps native input/audio/VRAM bridge work.
void MainDrawCb_Vsync()
{
	struct GameTracker *gGT;

	gGT = sdata->gGT;
	int audioTick = 1;
#ifdef CTR_NATIVE
	static unsigned int callbackFrame;
	int rate = CTR_NATIVE_60FPS_ACTIVE ? CTR_FRAMES_PER_SECOND : 60;
	audioTick = NativeFrameRate_Step(2, callbackFrame++, rate) != 0;
#endif
	gGT->frameTimer_VsyncCallback += audioTick;
	if (audioTick && ((gGT->gameMode1 & PAUSE_ALL) == 0))
	{
		gGT->frameTimer_Confetti++;
	}

	sdata->vsyncTillFlip--;
	gGT->vSync_between_drawSync++;

	// NOTE(aalhend): accumulate root-counter 1 units between VSync callbacks; Timer_GetTime_Total converts them through the retail 0x147e divisor.
	sdata->rcntTotalUnits += GetRCnt(0xf2000001);
	ResetRCnt(0xf2000001);

#ifdef CTR_NATIVE
	// NOTE(aalhendi): Retail calls HOWL unconditionally. Native skips only while
	// channel lists are mid-edit.
	if (audioTick && (sdata->criticalSectionCount == 0))
#endif
	{
		howl_PlayAudio_Update();
	}

#ifdef CTR_NATIVE
	// NOTE(aalhendi): Native owns host input and writes PSX-shaped pad
	// snapshots before retail GAMEPAD_PollVsync consumes them.
	Platform_PollInput();
#endif

	GAMEPAD_PollVsync(sdata->gGamepads);

	return;
}
