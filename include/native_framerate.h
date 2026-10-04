#ifndef NATIVE_FRAMERATE_H
#define NATIVE_FRAMERATE_H

#include <stdint.h>

/* The option index keeps the old 30/60 setting and ghost override compatible. */
#define NATIVE_FRAME_RATE_COUNT 6
static inline int NativeFrameRate_FromIndex(int index)
{
	static const int rates[NATIVE_FRAME_RATE_COUNT] = {30, 60, 90, 120, 144, 240};
	return rates[(index >= 0 && index < NATIVE_FRAME_RATE_COUNT) ? index : 0];
}

static inline int NativeFrameRate_Index(int rate)
{
	for (int i = 0; i < NATIVE_FRAME_RATE_COUNT; i++)
		if (NativeFrameRate_FromIndex(i) == rate)
			return i;
	return -1;
}

/* Difference of cumulative fractions preserves small signed steps at 144 Hz. */
static inline int NativeFrameRate_Step(int value, unsigned int frame, int rate)
{
	unsigned int phase = frame % (unsigned int)rate;
	return (int)(((int64_t)value * (phase + 1) * 30) / rate - ((int64_t)value * phase * 30) / rate);
}

static inline int NativeFrameRate_Tick(unsigned int frame, int rate)
{
	return NativeFrameRate_Step(1, frame, rate) != 0;
}

#endif
