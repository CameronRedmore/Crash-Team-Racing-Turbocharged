#include "platform/native_aspect.h"

#include <limits.h>
#include <math.h>

#include <macros.h>
#include "platform/native_draw3d.h"
#include "platform/native_projection.h"

int gNativeAspectRatio = NATIVE_ASPECT_16_9;
int gNativeFovDegrees = 0;

#define NATIVE_ASPECT_PI 3.14159265358979323846

typedef struct NativeAspectFraction
{
	int32_t numerator;
	int32_t denominator;
} NativeAspectFraction;

static const NativeAspectFraction s_nativeAspectScale[NATIVE_ASPECT_COUNT] = {{1, 1}, {3, 4}, {5, 6}, {4, 7}, {3, 8}};

static const int s_nativeAspectWidth[NATIVE_ASPECT_COUNT] = {4, 16, 16, 21, 32};
static const int s_nativeAspectHeight[NATIVE_ASPECT_COUNT] = {3, 9, 10, 9, 9};
static const char *s_nativeAspectLabels[NATIVE_ASPECT_COUNT] = {"4:3", "16:9", "16:10", "21:9", "32:9"};

static int NativeAspect_ValidRatio(int ratio)
{
	return ratio >= 0 && ratio < NATIVE_ASPECT_COUNT;
}

static NativeAspectFraction NativeAspect_GetScaleFraction(void)
{
	if (!NativeAspect_IsActive())
		return (NativeAspectFraction){34, 45};
	if (!NativeAspect_ValidRatio(gNativeAspectRatio))
		return s_nativeAspectScale[NATIVE_ASPECT_16_9];
	return s_nativeAspectScale[gNativeAspectRatio];
}

int NativeAspect_IsActive(void)
{
#if defined(CTR_NATIVE) && NATIVE_DRAW3D_SUPPORTED
	return NATIVE_DRAW3D_ACTIVE();
#else
	return 0;
#endif
}

double NativeAspect_GetScaleX(void)
{
	NativeAspectFraction fraction = NativeAspect_GetScaleFraction();
	return (double)fraction.numerator / (double)fraction.denominator;
}

int NativeAspect_UsesExpandedVisibility(void)
{
	return NativeAspect_IsActive() && (NativeAspect_GetScaleX() < 0.75 || NativeAspect_GetFocalScale() < 1.0 ||
	                                   (gNativeProjectionMode == NATIVE_PROJECTION_EDGE && gNativeProjectionStrength > 0));
}

double NativeAspect_GetFocalScale(void)
{
	if (!NativeAspect_IsActive() || gNativeFovDegrees == 0 || gNativeFovDegrees < 45 || gNativeFovDegrees > 100)
	{
		return 1.0;
	}
	// Retail has tan(verticalFov / 2) = 0.75: the NTSC projection uses
	// 9/16 vertical scaling over 216 pixels at a focal distance of 256.
	// PAL's 59/96 over 236 pixels gives the same value.
	return 0.75 / tan((double)gNativeFovDegrees * NATIVE_ASPECT_PI / 360.0);
}

int32_t NativeAspect_ScaleX(int32_t x)
{
	NativeAspectFraction fraction = NativeAspect_GetScaleFraction();
	int64_t result = ((int64_t)x * fraction.numerator) / fraction.denominator;
	if (result > INT32_MAX)
		return INT32_MAX;
	if (result < INT32_MIN)
		return INT32_MIN;
	return (int32_t)result;
}

int32_t NativeAspect_ExpandX(int32_t x)
{
	NativeAspectFraction fraction = NativeAspect_GetScaleFraction();
	int64_t result = ((int64_t)x * fraction.denominator) / fraction.numerator;
	if (result > INT32_MAX)
		return INT32_MAX;
	if (result < INT32_MIN)
		return INT32_MIN;
	return (int32_t)result;
}

int32_t NativeAspect_ScaleXCeil(int32_t x)
{
	NativeAspectFraction fraction = NativeAspect_GetScaleFraction();
	int64_t product = (int64_t)x * fraction.numerator;
	int64_t result = product / fraction.denominator;
	if (product > 0 && product % fraction.denominator != 0)
		++result;
	if (result > INT32_MAX)
		return INT32_MAX;
	if (result < INT32_MIN)
		return INT32_MIN;
	return (int32_t)result;
}

void NativeAspect_GetPresentation(int *w, int *h)
{
	int ratio = NativeAspect_IsActive() && NativeAspect_ValidRatio(gNativeAspectRatio) ? gNativeAspectRatio : NATIVE_ASPECT_16_9;
#ifdef __vita__
	if (!NativeAspect_IsActive())
	{
		if (w)
			*w = 30;
		if (h)
			*h = 17;
		return;
	}
#endif
	if (w)
		*w = s_nativeAspectWidth[ratio];
	if (h)
		*h = s_nativeAspectHeight[ratio];
}

const char *NativeAspect_GetLabel(int ratio)
{
	return s_nativeAspectLabels[NativeAspect_ValidRatio(ratio) ? ratio : NATIVE_ASPECT_16_9];
}

double NativeAspect_GetHudPixelAspectX(void)
{
	return (512.0 / (double)SCREEN_HEIGHT) / (4.0 / 3.0) * NativeAspect_GetScaleX();
}
