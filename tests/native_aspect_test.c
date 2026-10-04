#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "platform/native_draw3d.h"
#include "platform/native_aspect.h"

int gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
#include "../platform/native_aspect.c"

int main(void)
{
	static const int32_t num[] = {1, 3, 5, 4, 3};
	static const int32_t den[] = {1, 4, 6, 7, 8};
	static const int width[] = {4, 16, 16, 21, 32};
	static const int height[] = {3, 9, 10, 9, 9};
	static const char *labels[] = {"4:3", "16:9", "16:10", "21:9", "32:9"};
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	for (int ratio = 0; ratio < NATIVE_ASPECT_COUNT; ++ratio)
	{
		gNativeAspectRatio = ratio;
		assert(fabs(NativeAspect_GetScaleX() - (double)num[ratio] / den[ratio]) < 1e-12);
		assert(NativeAspect_ScaleX(den[ratio] * 12) == num[ratio] * 12);
		assert(NativeAspect_ExpandX(num[ratio] * 12) == den[ratio] * 12);
		assert(strcmp(NativeAspect_GetLabel(ratio), labels[ratio]) == 0);
		{
			int w = 0, h = 0;
			NativeAspect_GetPresentation(&w, &h);
			assert(w == width[ratio] && h == height[ratio]);
		}
		assert(NativeAspect_ScaleX(1) == num[ratio] / den[ratio]);
		assert(NativeAspect_ScaleX(-1) == (-num[ratio]) / den[ratio]);
		assert(NativeAspect_ScaleX(8) == (8 * num[ratio]) / den[ratio]);
		assert(NativeAspect_ScaleX(-8) == (-8 * num[ratio]) / den[ratio]);
	}
	gNativeAspectRatio = NATIVE_ASPECT_16_9;
	gNativeFovDegrees = 0;
	assert(fabs(NativeAspect_GetFocalScale() - 1.0) < 1e-12);
	assert(!NativeAspect_UsesExpandedVisibility());
	gNativeFovDegrees = 90;
	assert(fabs(NativeAspect_GetFocalScale() - 0.75) < 1e-12);
	assert(NativeAspect_UsesExpandedVisibility());
	gNativeFovDegrees = 45;
	assert(fabs(NativeAspect_GetFocalScale() - (0.75 / tan(45.0 * 3.14159265358979323846 / 360.0))) < 1e-12);
	gNativeFovDegrees = 100;
	assert(fabs(NativeAspect_GetFocalScale() - (0.75 / tan(100.0 * 3.14159265358979323846 / 360.0))) < 1e-12);
	gNativeFovDegrees = 60;
	assert(NativeAspect_GetFocalScale() > 1.0);
	assert(!NativeAspect_UsesExpandedVisibility());
	gNativeFovDegrees = 101;
	assert(fabs(NativeAspect_GetFocalScale() - 1.0) < 1e-12);
	gNativeFovDegrees = 44;
	assert(fabs(NativeAspect_GetFocalScale() - 1.0) < 1e-12);
	gNativeAspectRatio = NATIVE_ASPECT_4_3;
	gNativeFovDegrees = 90;
	assert(NativeAspect_UsesExpandedVisibility());
	gNativeFovDegrees = 0;
	gNativeAspectRatio = NATIVE_ASPECT_21_9;
	assert(NativeAspect_ScaleXCeil(1) == 1);
	assert(NativeAspect_ScaleXCeil(-1) == 0);
	assert(NativeAspect_ScaleXCeil(-8) == -4);
	assert(NativeAspect_ScaleX(1) == 0);
	assert(NativeAspect_ScaleX(-1) == 0);
	gNativeAspectRatio = 100;
	assert(fabs(NativeAspect_GetScaleX() - 0.75) < 1e-12);
	assert(strcmp(NativeAspect_GetLabel(-1), "16:9") == 0);
	{
		int w = 0, h = 0;
		NativeAspect_GetPresentation(&w, &h);
		assert(w == 16 && h == 9);
	}
	gNativeAspectRatio = NATIVE_ASPECT_21_9;
	assert(NativeAspect_ExpandX(INT32_MAX) == INT32_MAX);
	assert(NativeAspect_ExpandX(INT32_MIN) == INT32_MIN);
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
	assert(fabs(NativeAspect_GetFocalScale() - 1.0) < 1e-12);
	assert(!NativeAspect_UsesExpandedVisibility());
	assert(!NativeAspect_IsActive());
	assert(fabs(NativeAspect_GetScaleX() - (34.0 / 45.0)) < 1e-12);
	assert(NativeAspect_ScaleX(45) == 34);
	assert(NativeAspect_ScaleX(1) == 0);
	assert(NativeAspect_ScaleX(-1) == 0);
	assert(fabs(NativeAspect_GetHudPixelAspectX() - (512.0 / 216.0 / (4.0 / 3.0) * (34.0 / 45.0))) < 1e-12);
	{
		int w = 0, h = 0;
		NativeAspect_GetPresentation(&w, &h);
		assert(w == 16 && h == 9);
	}
	return 0;
}
