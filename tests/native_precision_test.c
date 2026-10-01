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

static void portal_translation(void)
{
	const double view[9] = {4096, 0, 0, 0, 4096, 0, 0, 0, 4096};
	const s16 camera[3] = {-100, 200, 300};
	const float preciseCamera[3] = {-99.5f, 200.25f, 300.75f};
	// SpinRewards stores a negative sine in u32 before its logical shifts.
	// The resulting high bits disappear when QueueDraw writes GTE IR1..IR3.
	const u32 negativeSine = (u32)-4096;
	const s32 position[3] = {
		camera[0] + ((negativeSine * 0xa0u) >> 12),
		camera[1] + 512 + ((negativeSine << 6) >> 12),
		camera[2] + 800
	};
	double translation[3];
	NativePgxp_ModelViewTranslation(view, position, camera, preciseCamera, translation);
	close_to(translation[0], -160.5);
	close_to(translation[1], 447.75);
	close_to(translation[2], 799.25);

	// Exercise the same IR-vector transform as QueueDraw's integer path.
	MATRIX matrix = {0};
	matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 4096;
	gte_SetLightMatrix(&matrix);
	for (int i = 0; i < 3; i++) MTC2((u32)position[i] - (u32)(s32)camera[i], 9 + i);
	GTE_operator(0x04be012);
	for (int i = 0; i < 3; i++)
	{
		const s32 retail = MFC2_S(9 + i);
		close_to(translation[i], retail - (preciseCamera[i] - camera[i]));
		matrix.t[i] = retail * 4;
		translation[i] *= 4.0;
	}
	NativePgxp_SetTransform(&matrix, &matrix.m[0][0], matrix.t, view, translation);
	gte_SetRotMatrix(&matrix);
	gte_SetTransMatrix(&matrix);
	CTC2(256u << 16, 24);
	CTC2(120u << 16, 25);
	CTC2(256, 26);
	MTC2(0, 0);
	MTC2(0, 1);
	GTE_operator(0x80001);
	u32 packed = MFC2(14);
	NativePgxpVertex vertex;
	NativePgxp_StoreGteSXY(&packed, 14, packed);
	assert(NativePgxp_Lookup(&packed, packed, &vertex));
	close_to(vertex.x, 256.0 - 160.5 * 256.0 / 799.25);
	close_to(vertex.y, 120.0 + 447.75 * 256.0 / 799.25);
	close_to(vertex.w, 799.25 * 4.0);
	NativePgxp_EndFrame();
}

static void model_translation_boundaries(void)
{
	const double view[9] = {4096, 0, 0, 0, 4096, 0, 0, 0, 4096};
	const s16 camera[3] = {32760, -32760, 0};
	const float preciseCamera[3] = {32760.75f, -32760.5f, 0.25f};
	const s32 position[3] = {-32760, 32760, 65536 + 100};
	double translation[3];
	NativePgxp_ModelViewTranslation(view, position, camera, preciseCamera, translation);
	close_to(translation[0], 15.25);
	close_to(translation[1], -15.5);
	close_to(translation[2], 99.75);

	const double scaledView[9] = {8192, 0, 0, 0, 8192, 0, 0, 0, 4096};
	const s16 zeroCamera[3] = {0};
	const float fractionalCamera[3] = {0.5f, -0.25f, 0.75f};
	const s32 farPosition[3] = {20000, -20000, 100};
	NativePgxp_ModelViewTranslation(scaledView, farPosition, zeroCamera, fractionalCamera, translation);
	close_to(translation[0], 32767);
	close_to(translation[1], -32768);
	close_to(translation[2], 99.25);
}

static void project_near_vertex(s16 x, s16 y, s16 z, const float *precise, NativePgxpVertex *vertex)
{
	MTC2((u32)(u16)x | ((u32)(u16)y << 16), 0);
	MTC2((u32)(u16)z, 1);
	if (precise != NULL)
	{
		const s16 vector[3] = {x, y, z};
		NativePgxp_GteSetInput(0, vector, precise);
	}
	GTE_operator(0x80001); // RTPS, sf=1
	u32 packed = MFC2(14);
	NativePgxp_StoreGteSXY(&packed, 14, packed);
	assert(NativePgxp_Lookup(&packed, packed, vertex));
}

static void near_projection(void)
{
	NativePgxp_EndFrame();
	MATRIX identity = {0};
	identity.m[0][0] = identity.m[1][1] = identity.m[2][2] = 4096;
	gte_SetRotMatrix(&identity);
	gte_SetTransMatrix(&identity);
	CTC2(256u << 16, 24);
	CTC2(120u << 16, 25);
	CTC2(256, 26);
	NativePgxpVertex vertex;

	// Saturated SX keeps the retail clamp, but the precise vertex keeps its
	// real position and depth so the polygon stays perspective-correct.
	project_near_vertex(1000, 0, 200, NULL, &vertex);
	assert((s16)MFC2(14) == 1023);
	close_to(vertex.x, 256.0 + 1000.0 * 256.0 / 200.0);
	close_to(vertex.w, 200);

	// Divide overflow has no usable depth and stays retail.
	project_near_vertex(1000, 0, 100, NULL, &vertex);
	close_to(vertex.x, 1023);
	close_to(vertex.w, 0);

	// The near clipper projects doubled view coordinates through an identity
	// matrix. Halved precise input keeps SX/SY and recovers the true depth.
	project_near_vertex(100, 50, 150, NULL, &vertex);
	const NativePgxpVertex single = vertex;
	const float view[3] = {100, 50, 150};
	project_near_vertex(200, 100, 300, view, &vertex);
	close_to(vertex.x, single.x);
	close_to(vertex.y, single.y);
	close_to(vertex.w, 150);
	NativePgxp_EndFrame();
}

int main(void)
{
	portal_translation();
	model_translation_boundaries();
	near_projection();
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
	// Depth tracking defaults to on; this check needs both trackers off.
	gNativeDepthBufferEnabled = 0;
	MTC2(1u | (2u << 16), 0);
	MTC2(100, 1);
	GTE_operator(0x80001);
	packed = MFC2(14);
	NativePgxp_StoreGteSXY(&packed, 14, packed);
	assert(!NativePgxp_Lookup(&packed, packed, &vertex));

	// Depth-only tracking retains PGXP Off's integer screen coordinates and
	// winding while carrying the Z which PS1 packets normally discard.
	NativePgxp_EndFrame();
	MATRIX depthMatrix = {0};
	depthMatrix.m[0][0] = depthMatrix.m[1][1] = depthMatrix.m[2][2] = 4096;
	gte_SetRotMatrix(&depthMatrix);
	gte_SetTransMatrix(&depthMatrix);
	MTC2(1u | (2u << 16), 0);
	MTC2(1000, 1);
	GTE_operator(0x80001);
	u32 retailXY = MFC2(14);
	gNativeDepthBufferEnabled = 1;
	NativePgxp_SetWorldPhase(1);
	NativePgxp_SetDepthContext(0.25f);
	GTE_operator(0x80001);
	packed = MFC2(14);
	assert(packed == retailXY);
	gte_stsxy_reg(&packed, 14);
	assert(NativePgxp_Lookup(&packed, packed, &vertex));
	close_to(vertex.x, (s16)packed);
	close_to(vertex.y, (s16)(packed >> 16));
	close_to(vertex.w, 1000);
	close_to(vertex.depth, 250);
	assert(NATIVE_VERTEX_TRACKING_ACTIVE() && !NATIVE_PGXP_ACTIVE());

	// Different projections can land on exactly the same screen coordinate.
	// CPU-written packets must recover the latest depth rather than treating
	// that projection as a repeated read of the earlier vertex.
	packed = 42u | (33u << 16);
	NativePgxp_GteProject(42, 33, 200, packed);
	NativePgxp_GteReadSXY(14, packed);
	NativePgxp_GteProject(42, 33, 800, packed);
	NativePgxp_GteReadSXY(14, packed);
	NativePgxp_BindWrittenXY(&packed, packed);
	assert(NativePgxp_Lookup(&packed, packed, &vertex));
	close_to(vertex.depth, 200);

	// A HUD projection has precision but does not take part in world depth.
	NativePgxp_SetWorldPhase(0);
	GTE_operator(0x80001);
	gte_stsxy_reg(&packed, 14);
	assert(NativePgxp_Lookup(&packed, packed, &vertex));
	close_to(vertex.depth, 0);
	NativePgxp_SetModelDepthScale(&depthMatrix, 0.25f);
	close_to(NativePgxp_GetModelDepthScale(&depthMatrix), 0.25f);
	NativePgxp_EndFrame();
	close_to(NativePgxp_GetModelDepthScale(&depthMatrix), 0);
	gNativeDepthBufferEnabled = 0;
	puts("Native precision checks passed");
	return 0;
}
