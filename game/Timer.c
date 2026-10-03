#include <common.h>

#define TIMER_RCNT RCntCNT1

enum TimerConstants
{
	TIMER_RCNT_TARGET = 0xffff,
	TIMER_RCNT_MODE = 0x2000,
	TIMER_RCNT_LOW_RECHECK_THRESHOLD = 100,
	TIMER_MILLISECONDS_PER_SECOND = 1000,
	TIMER_RCNT_UNITS_PER_SECOND = 0x147e,
	TIMER_WRAP_MILLISECONDS = 0xc7e18,
};

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004b31c-0x8004b370.
void Timer_Init()
{
	EnterCriticalSection();
	StopRCnt(TIMER_RCNT);
	SetRCnt(TIMER_RCNT, TIMER_RCNT_TARGET, TIMER_RCNT_MODE);
	StartRCnt(TIMER_RCNT);
	ExitCriticalSection();
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004b370-0x8004b3a4.
void Timer_Destroy()
{
	EnterCriticalSection();
	StopRCnt(TIMER_RCNT);
	ExitCriticalSection();
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004b3a4-0x8004b41c.
int Timer_GetTime_Total()
{
	s32 rcntTotal = sdata->rcntTotalUnits;
	s32 rcnt = GetRCnt(TIMER_RCNT);
	s32 sysClock = rcntTotal + rcnt;

	if (rcnt < TIMER_RCNT_LOW_RECHECK_THRESHOLD)
	{
		sysClock = sdata->rcntTotalUnits + rcnt;
	}

	return (sysClock * TIMER_MILLISECONDS_PER_SECOND) / TIMER_RCNT_UNITS_PER_SECOND;
}

// Usage: elapsed(frameStart, &frameStart)
// will overwrite new frameStart, and return
// elapsed time since previous frameStart
// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004b41c-0x8004b470.
int Timer_GetTime_Elapsed(int oldVal, int *retVal)
{
	s32 newVal = Timer_GetTime_Total();

	if (retVal != 0)
	{
		*retVal = newVal;
	}

	// impossible?
	if (newVal < oldVal)
	{
		newVal += TIMER_WRAP_MILLISECONDS;
	}

	return newVal - oldVal;
}

/// @brief Retail `(value * elapsedTimeMS) >> shift`, for per-frame motion and timers.
/// Above 30 FPS, frames are only a few ms long, so the retail shift rounds small
/// values down to 0 every frame (gravity 4 >> 5 at 240 FPS never applies). Taking
/// the difference of the scaled running level time keeps the sum exact instead.
int Timer_ScaleByElapsed(int value, int shift)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	int elapsed = gGT->elapsedTimeMS;

#if defined(CTR_NATIVE)
	if (CTR_FRAMES_PER_SECOND > FPS)
	{
		s64 now = (u32)gGT->msInThisLEV;
		return (int)((((s64)value * now) >> shift) - (((s64)value * (now - elapsed)) >> shift));
	}
#endif

	return (value * elapsed) >> shift;
}
