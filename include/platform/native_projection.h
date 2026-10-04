#ifndef NATIVE_PROJECTION_H
#define NATIVE_PROJECTION_H

enum NativeProjectionMode
{
	NATIVE_PROJECTION_PERSPECTIVE = 0,
	NATIVE_PROJECTION_PANINI = 1,
	NATIVE_PROJECTION_EDGE = 2,
	NATIVE_PROJECTION_MODE_COUNT
};

extern int gNativeProjectionMode;
extern int gNativeProjectionStrength;

// Runtime activation and conservative world-target overscan are provided by the game layer.
int NativeProjection_IsGameplayActive(void);
double NativeProjection_GetWorldOverscan(void);

typedef struct NativeProjectionParams
{
	enum NativeProjectionMode mode;
	int strength;
	double horizontalTanHalf;
	double overscan;
	double paniniEndpoint;
} NativeProjectionParams;

// Returns zero for invalid pointers or a non-finite/non-positive horizontal tangent.
int NativeProjection_BuildParams(enum NativeProjectionMode mode, int strength,
	double horizontalTanHalf, NativeProjectionParams *out);

// Inverse-map display NDC to source-camera NDC. Returns zero if inputs are invalid.
int NativeProjection_MapOutputToSource(const NativeProjectionParams *params,
	double outputNdcX, double outputNdcY, double *sourceNdcX, double *sourceNdcY);

#endif
