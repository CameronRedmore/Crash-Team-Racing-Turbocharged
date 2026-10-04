#include "platform/native_visibility_stats.h"

#include "platform/native_log.h"

#include <stdio.h>
#include <time.h>

#define NATIVE_VIS_REPORT_INTERVAL_SECONDS 10.0

// Counts for the frame in flight, the frame just finished, and the session.
// NativeVisibility_ReportFrameBoundary runs at the top of a frame, so at that
// point s_nativeVisFrame holds the previous frame's totals and becomes
// s_nativeVisLastFrame.
global_variable u64 s_nativeVisFrame[NATIVE_VIS_COUNTER_COUNT];
global_variable u64 s_nativeVisLastFrame[NATIVE_VIS_COUNTER_COUNT];
global_variable u64 s_nativeVisSession[NATIVE_VIS_COUNTER_COUNT];
global_variable u64 s_nativeVisInterval[NATIVE_VIS_COUNTER_COUNT];
global_variable u32 s_nativeVisIntervalFrames;
global_variable double s_nativeVisReportTime;

global_variable const char *const s_nativeVisNames[NATIVE_VIS_COUNTER_COUNT] = {
    "bsp_children_tested", "bsp_children_bbox_rejected", "bsp_children_far_rejected", "bsp_children_pushed", "bsp_leaves_linked",
    "bsp_leaves_rejected", "quad_blocks_tested",         "quad_blocks_emitted",       "instances_queued",    "instances_dropped",
};

void NativeVisibilityCount(enum NativeVisibilityCounter counter, u32 amount)
{
	if (counter < 0 || counter >= NATIVE_VIS_COUNTER_COUNT || amount == 0)
	{
		return;
	}
	s_nativeVisFrame[counter] += amount;
	s_nativeVisSession[counter] += amount;
}

void NativeVisibilityCountOnce(enum NativeVisibilityCounter counter)
{
	NativeVisibilityCount(counter, 1);
}

void NativeVisibilityLastFrame(u64 *out)
{
	if (out == NULL)
	{
		return;
	}
	for (int i = 0; i < NATIVE_VIS_COUNTER_COUNT; i++)
	{
		out[i] = s_nativeVisLastFrame[i];
	}
}

u64 NativeVisibilitySessionTotal(enum NativeVisibilityCounter counter)
{
	if (counter < 0 || counter >= NATIVE_VIS_COUNTER_COUNT)
	{
		return 0;
	}
	return s_nativeVisSession[counter];
}

void NativeVisibility_ReportFrameBoundary(void)
{
	for (int i = 0; i < NATIVE_VIS_COUNTER_COUNT; i++)
	{
		s_nativeVisLastFrame[i] = s_nativeVisFrame[i];
		s_nativeVisFrame[i] = 0;
		s_nativeVisInterval[i] += s_nativeVisLastFrame[i];
	}
	s_nativeVisIntervalFrames++;

	// time() rather than timespec_get(): the i686 MinGW (msvcrt) runtime lacks TIME_UTC.
	const time_t now = time(NULL);
	if (now == (time_t)-1)
	{
		return;
	}
	const double seconds = (double)now;
	if (s_nativeVisReportTime == 0.0 || seconds < s_nativeVisReportTime)
	{
		s_nativeVisReportTime = seconds;
		return;
	}
	if (seconds - s_nativeVisReportTime < NATIVE_VIS_REPORT_INTERVAL_SECONDS)
	{
		return;
	}

	int active = 0;
	for (int i = 0; i < NATIVE_VIS_COUNTER_COUNT; i++)
	{
		if (s_nativeVisInterval[i] != 0)
		{
			active = 1;
			break;
		}
	}
	if (!active)
	{
		return;
	}

	// The interval is the useful window: a level's per-frame counts are steady,
	// so a jump between intervals is a camera or settings change rather than a
	// track change. Report the per-frame average as well, because the interval
	// length depends on how fast the machine is rendering.
	const u32 frames = (s_nativeVisIntervalFrames != 0) ? s_nativeVisIntervalFrames : 1;
	char line[768];
	int used = snprintf(line, sizeof(line), "[CTR Visibility] %u frames", frames);
	for (int i = 0; i < NATIVE_VIS_COUNTER_COUNT && used < (int)sizeof(line) - 1; i++)
	{
		used += snprintf(line + used, sizeof(line) - (size_t)used, " %s=%llu/%llu", s_nativeVisNames[i], (unsigned long long)(s_nativeVisInterval[i] / frames),
		                 (unsigned long long)s_nativeVisInterval[i]);
	}
	Platform_LogWarn("%s\n", line);

	for (int i = 0; i < NATIVE_VIS_COUNTER_COUNT; i++)
	{
		s_nativeVisInterval[i] = 0;
	}
	s_nativeVisIntervalFrames = 0;
	s_nativeVisReportTime = seconds;
}
