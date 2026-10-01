#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <macros.h>
#include <psx/libgte.h>
#include <psx/inline_c.h>
#include <psx/gtereg.h>
#include <platform/native_pgxp.h>

int gNativeMirrorModeRenderActive;
int gNativeMirrorModeDoubleFlipActive;
extern int GTE_operator(int op);

static void close_to(double actual, double expected)
{
	assert(fabs(actual - expected) < 0.0001);
}

int main(void)
{
	// Compound camera rotation order and fractional-angle orthonormality.
	const float angles[3] = {1024, 1024, 0};
	double camera[9];
	NativePgxp_CameraRotation(angles, camera);
	close_to(camera[1], 1);
	close_to(camera[5], -1);
	close_to(camera[6], -1);
	const float fractionalAngles[3] = {101.25f, 300.5f, -75.75f};
	NativePgxp_CameraRotation(fractionalAngles, camera);
	for (int a = 0; a < 3; a++)
		for (int b = 0; b < 3; b++)
		{
			double dot = 0;
			for (int k = 0; k < 3; k++) dot += camera[a*3+k] * camera[b*3+k];
			close_to(dot, a == b ? 1 : 0);
		}
	MATRIX matrix = {0};
	matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 4096;
	const double preciseRotation[9] = {4096, 0, 0, 0, 4096, 0, 0, 0, 4096};
	const double preciseTranslation[3] = {0.5, 0.25, 0.75};
	double rotation[9], translation[3], result[3];
	const double input[3] = {1, 2, 100};
	const double zero[3] = {0};
	NativePgxp_SetTransform(&matrix, &matrix.m[0][0], matrix.t, preciseRotation, preciseTranslation);
	gte_SetRotMatrix(&matrix);
	gte_SetTransMatrix(&matrix);
	NativePgxp_Transform(0, 0, preciseRotation, zero, input, result);
	close_to(result[0] / 4096.0, 1.5);
	close_to(result[2] / 4096.0, 100.75);

	// Projection retains fractional camera translation and actual depth.
	CTC2(0, 24);
	CTC2(0, 25);
	CTC2(50, 26);
	MTC2(1u | (2u << 16), 0);
	MTC2(100, 1);
	GTE_operator(0x80001); // RTPS, sf=1
	u32 packed = MFC2(14);
	NativePgxpVertex vertex;
	NativePgxp_StoreGteSXY(&packed, 14, packed);
	assert(NativePgxp_Lookup(&packed, packed, &vertex));
	close_to(vertex.x, 1.5 * 50.0 / 100.75);
	close_to(vertex.y, 2.25 * 50.0 / 100.75);
	close_to(vertex.w, 100.75);

	// A direct translation write removes the camera shadow even if the
	// integer value is unchanged. Rotation and translation are independent.
	CTC2(0, 5);
	NativePgxp_Transform(0, 0, preciseRotation, zero, input, result);
	close_to(result[0] / 4096.0, 1);

	// Changed integer matrices must not recover old fractional transforms.
	matrix.t[0] = 9;
	NativePgxp_GetTransform(&matrix, &matrix.m[0][0], matrix.t, rotation, translation);
	close_to(translation[0], 9);
	matrix.t[0] = 0;
	NativePgxp_EndFrame();
	NativePgxp_GetTransform(&matrix, &matrix.m[0][0], matrix.t, rotation, translation);
	close_to(translation[0], 0);

	// Fractional rotation survives projection and light-matrix transforms.
	double fractionalRotation[9] = {4096, 0.5, 0, 0, 4096, 0, 0, 0, 4096};
	NativePgxp_SetTransform(&matrix, &matrix.m[0][0], matrix.t, fractionalRotation, preciseTranslation);
	gte_SetRotMatrix(&matrix);
	gte_SetLightMatrix(&matrix);
	NativePgxp_Transform(0, 3, preciseRotation, zero, input, result);
	close_to(result[0], 4097.0);
	NativePgxp_Transform(1, 3, preciseRotation, zero, input, result);
	close_to(result[0], 4097.0);
	CTC2(4096, 0);
	NativePgxp_Transform(0, 3, preciseRotation, zero, input, result);
	close_to(result[0], 4096.0);
	NativePgxp_EndFrame();

	// IR is MVMVA's fourth vector, not an entry in the three input shadows.
	assert(!NativePgxp_GteGetInput(3, 1, 2, 3, result));
	gte_SetRotMatrix(&matrix);
	gte_SetTransMatrix(&matrix);
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	MTC2(1u | (2u << 16), 0);
	MTC2(100, 1);
	GTE_operator(0x80001);
	packed = MFC2(14);
	NativePgxp_StoreGteSXY(&packed, 14, packed);
	assert(!NativePgxp_Lookup(&packed, packed, &vertex));
	puts("Native precision checks passed");
	return 0;
}
