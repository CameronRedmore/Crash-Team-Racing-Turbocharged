#ifndef NATIVE_ASPECT_H
#define NATIVE_ASPECT_H

#include <stdint.h>

enum NativeAspectRatio
{
	NATIVE_ASPECT_4_3 = 0,
	NATIVE_ASPECT_16_9 = 1,
	NATIVE_ASPECT_16_10 = 2,
	NATIVE_ASPECT_21_9 = 3,
	NATIVE_ASPECT_32_9 = 4,
	NATIVE_ASPECT_COUNT
};

extern int gNativeAspectRatio;
extern int gNativeFovDegrees;

int NativeAspect_IsActive(void);
// Retail PVS masks can omit geometry with ultrawide aspect or wider FOV.
int NativeAspect_UsesExpandedVisibility(void);
// Multiplier applied to both camera projection axes, relative to the retail FOV.
double NativeAspect_GetFocalScale(void);
double NativeAspect_GetScaleX(void);
int32_t NativeAspect_ScaleX(int32_t x);
int32_t NativeAspect_ExpandX(int32_t x);
int32_t NativeAspect_ScaleXCeil(int32_t x);
void NativeAspect_GetPresentation(int *w, int *h);
const char *NativeAspect_GetLabel(int ratio);
double NativeAspect_GetHudPixelAspectX(void);

#endif
