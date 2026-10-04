#include "platform/native_projection.h"

#include <math.h>

#define NATIVE_PROJECTION_EDGE_CENTRE 0.35
#define NATIVE_PROJECTION_PI 3.14159265358979323846

int gNativeProjectionMode = NATIVE_PROJECTION_PERSPECTIVE;
int gNativeProjectionStrength = 50;

int NativeProjection_BuildParams(enum NativeProjectionMode mode, int strength,
	double horizontalTanHalf, NativeProjectionParams *out)
{
	if (out == 0) return 0;
	*out = (NativeProjectionParams){NATIVE_PROJECTION_PERSPECTIVE, 0, 1.0, 1.0, 1.0};
	if (!isfinite(horizontalTanHalf) || horizontalTanHalf <= 0.0) return 0;
	if (mode < NATIVE_PROJECTION_PERSPECTIVE || mode >= NATIVE_PROJECTION_MODE_COUNT)
	{
		return 1;
	}
	strength = (strength < 0) ? 0 : ((strength > 100) ? 100 : strength);
	out->mode = mode;
	out->strength = strength;
	out->horizontalTanHalf = horizontalTanHalf;
	if (mode == NATIVE_PROJECTION_PERSPECTIVE || strength == 0)
	{
		out->mode = NATIVE_PROJECTION_PERSPECTIVE;
		out->strength = 0;
		return 1;
	}
	if (mode == NATIVE_PROJECTION_EDGE)
	{
		const double k = (double)strength / 100.0;
		out->overscan = NATIVE_PROJECTION_EDGE_CENTRE +
			expm1(k * (1.0 - NATIVE_PROJECTION_EDGE_CENTRE)) / k;
		return isfinite(out->overscan) && out->overscan >= 1.0;
	}
	{
		const double d = (double)strength / 100.0;
		out->paniniEndpoint = horizontalTanHalf * (d + 1.0) /
			(1.0 + d * sqrt(1.0 + horizontalTanHalf * horizontalTanHalf));
		return isfinite(out->paniniEndpoint) && out->paniniEndpoint > 0.0;
	}
}

static double NativeProjection_EdgeSourceX(const NativeProjectionParams *params, double outputX)
{
	const double c = NATIVE_PROJECTION_EDGE_CENTRE;
	const double k = (double)params->strength / 100.0;
	const double absX = fabs(outputX);
	const double warped = absX <= c ? absX : c + expm1(k * (absX - c)) / k;
	return copysign(warped / params->overscan, outputX);
}

static int NativeProjection_PaniniSource(const NativeProjectionParams *params,
	double outputX, double outputY, double *sourceX, double *sourceY)
{
	const double d = (double)params->strength / 100.0;
	const double a = d + 1.0;
	const double p = outputX * params->paniniEndpoint;
	const double denominator = a * a - p * p * d * d;
	if (denominator <= 0.0) return 0;
	const double radicand = a * a + (1.0 - d * d) * p * p;
	if (radicand < 0.0) return 0;
	const double rayX = p * (a + d * sqrt(radicand)) / denominator;
	const double factor = a / (1.0 + d * sqrt(1.0 + rayX * rayX));
	if (!isfinite(rayX) || !isfinite(factor) || factor <= 0.0) return 0;
	*sourceX = rayX / params->horizontalTanHalf;
	const double endpointFactor = params->paniniEndpoint / params->horizontalTanHalf;
	*sourceY = outputY * endpointFactor / factor;
	return isfinite(*sourceX) && isfinite(*sourceY);
}

int NativeProjection_MapOutputToSource(const NativeProjectionParams *params,
	double outputNdcX, double outputNdcY, double *sourceNdcX, double *sourceNdcY)
{
	if (params == 0 || sourceNdcX == 0 || sourceNdcY == 0 ||
		!isfinite(outputNdcX) || !isfinite(outputNdcY))
	{
		return 0;
	}
	if (params->mode == NATIVE_PROJECTION_PERSPECTIVE || params->strength <= 0)
	{
		*sourceNdcX = outputNdcX;
		*sourceNdcY = outputNdcY;
		return 1;
	}
	// NDC is the display rectangle; clamp round-off at the edge to the fitted endpoint.
	outputNdcX = fmax(-1.0, fmin(1.0, outputNdcX));
	if (params->mode == NATIVE_PROJECTION_EDGE)
	{
		*sourceNdcX = NativeProjection_EdgeSourceX(params, outputNdcX);
		*sourceNdcY = outputNdcY;
		return isfinite(*sourceNdcX);
	}
	if (params->mode == NATIVE_PROJECTION_PANINI)
	{
		return NativeProjection_PaniniSource(params, outputNdcX, outputNdcY,
			sourceNdcX, sourceNdcY);
	}
	*sourceNdcX = outputNdcX;
	*sourceNdcY = outputNdcY;
	return 1;
}
