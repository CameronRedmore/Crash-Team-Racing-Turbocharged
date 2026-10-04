#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "platform/native_projection.h"

#include "../platform/native_projection.c"

static double EdgeForwardX(const NativeProjectionParams *params, double sourceX)
{
	const double c = 0.35;
	const double k = (double)params->strength / 100.0;
	const double target = fabs(sourceX) * params->overscan;
	double output = target <= c ? target : c + log1p(k * (target - c)) / k;
	return copysign(output, sourceX);
}

static void PaniniForward(const NativeProjectionParams *params, double sourceX, double sourceY, double *outputX, double *outputY)
{
	const double d = (double)params->strength / 100.0;
	const double rayX = sourceX * params->horizontalTanHalf;
	const double factor = (d + 1.0) / (1.0 + d * sqrt(1.0 + rayX * rayX));
	const double endpointFactor = params->paniniEndpoint / params->horizontalTanHalf;
	*outputX = rayX * factor / params->paniniEndpoint;
	*outputY = sourceY * factor / endpointFactor;
}

static void CheckRoundTripAndMonotonicity(enum NativeProjectionMode mode, int strength, double horizontalTanHalf)
{
	NativeProjectionParams params;
	assert(NativeProjection_BuildParams(mode, strength, horizontalTanHalf, &params));
	double previousX = -INFINITY;
	for (int i = 0; i <= 200; ++i)
	{
		const double sourceX = -1.0 + 2.0 * i / 200.0;
		const double sourceY = -0.9 + 1.8 * ((i * 37) % 200) / 199.0;
		double outputX, outputY;
		if (mode == NATIVE_PROJECTION_EDGE)
		{
			outputX = EdgeForwardX(&params, sourceX);
			outputY = sourceY;
		}
		else
		{
			PaniniForward(&params, sourceX, sourceY, &outputX, &outputY);
		}
		assert(isfinite(outputX) && isfinite(outputY));
		assert(outputX >= previousX);
		previousX = outputX;
		double roundTripX, roundTripY;
		assert(NativeProjection_MapOutputToSource(&params, outputX, outputY, &roundTripX, &roundTripY));
		assert(fabs(roundTripX - sourceX) < 1e-9);
		assert(fabs(roundTripY - sourceY) < 1e-9);
	}
	assert(fabs(previousX - 1.0) < 1e-9);
}

int main(void)
{
	static const double aspects[] = {4.0 / 3.0, 16.0 / 9.0, 16.0 / 10.0, 21.0 / 9.0, 32.0 / 9.0};
	NativeProjectionParams params;
	double sx, sy;
	assert(gNativeProjectionMode == NATIVE_PROJECTION_PERSPECTIVE);
	assert(gNativeProjectionStrength == 50);
	assert(NativeProjection_BuildParams(NATIVE_PROJECTION_PERSPECTIVE, 100, 1.0, &params));
	assert(params.mode == NATIVE_PROJECTION_PERSPECTIVE && params.overscan == 1.0);
	assert(NativeProjection_MapOutputToSource(&params, 0.37, -0.42, &sx, &sy));
	assert(fabs(sx - 0.37) < 1e-15 && fabs(sy + 0.42) < 1e-15);
	assert(NativeProjection_BuildParams(NATIVE_PROJECTION_EDGE, 0, 1.5, &params));
	assert(params.mode == NATIVE_PROJECTION_PERSPECTIVE && params.overscan == 1.0);
	assert(NativeProjection_BuildParams(NATIVE_PROJECTION_PANINI, 1, 1.5, &params));
	assert(fabs(params.paniniEndpoint - 1.5) < 0.02);
	assert(NativeProjection_MapOutputToSource(&params, 0.8, 0.2, &sx, &sy));
	assert(fabs(sx - 0.8) < 0.01 && fabs(sy - 0.2) < 0.01);
	assert(!NativeProjection_BuildParams(NATIVE_PROJECTION_PANINI, 50, 0.0, &params));
	assert(NativeProjection_BuildParams(NATIVE_PROJECTION_EDGE, 100, 1.0, &params));
	assert(NativeProjection_MapOutputToSource(&params, 0.0, 0.3, &sx, &sy));
	assert(fabs(sx) < 1e-15 && fabs(sy - 0.3) < 1e-15);
	assert(NativeProjection_MapOutputToSource(&params, 1.0, 0.3, &sx, &sy));
	assert(fabs(sx - 1.0) < 1e-12);
	assert(NativeProjection_MapOutputToSource(&params, -1.0, 0.3, &sx, &sy));
	assert(fabs(sx + 1.0) < 1e-12);
	for (int fov = 45; fov <= 100; fov += 5)
	{
		const double verticalTanHalf = tan((double)fov * 3.14159265358979323846 / 360.0);
		for (unsigned int aspect = 0; aspect < sizeof(aspects) / sizeof(aspects[0]); ++aspect)
		{
			const double hTan = verticalTanHalf * aspects[aspect];
			for (int strength = 20; strength <= 100; strength += 40)
			{
				CheckRoundTripAndMonotonicity(NATIVE_PROJECTION_EDGE, strength, hTan);
				CheckRoundTripAndMonotonicity(NATIVE_PROJECTION_PANINI, strength, hTan);
				assert(NativeProjection_BuildParams(NATIVE_PROJECTION_PANINI, strength, hTan, &params));
				assert(params.overscan == 1.0);
				for (int ix = 0; ix <= 20; ++ix)
				{
					for (int iy = 0; iy <= 20; ++iy)
					{
						const double x = -1.0 + ix / 10.0;
						const double y = -1.0 + iy / 10.0;
						assert(NativeProjection_MapOutputToSource(&params, x, y, &sx, &sy));
						assert(isfinite(sx) && isfinite(sy));
						assert(fabs(sx) <= 1.0 + 1e-12);
						assert(fabs(sy) <= 1.0 + 1e-12);
					}
				}
			}
		}
	}
	puts("native projection maths passed");
	return 0;
}
