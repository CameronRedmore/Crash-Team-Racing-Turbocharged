#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <macros.h>
#include <psx/libgte.h>
#include <psx/libgpu.h>
#include <platform/native_pgxp.h>

int gNativeDepthBufferEnabled;

void Platform_LogError(const char *fmt, ...)
{
	(void)fmt;
}

static NativeDraw3DView TestView(void)
{
	NativeDraw3DView view;
	memset(&view, 0, sizeof(view));
	view.rotation[0] = view.rotation[4] = view.rotation[8] = 1.0;
	view.projection = 256.0f;
	view.centerX = 256.0f;
	view.centerY = 120.0f;
	view.width = 512.0f;
	view.height = 240.0f;
	return view;
}

static NativeDraw3DVertex Vertex(float x, float y, float z)
{
	NativeDraw3DVertex v;
	memset(&v, 0, sizeof(v));
	v.x = x;
	v.y = y;
	v.z = z;
	v.r = v.g = v.b = 128;
	return v;
}

static NativeDraw3DMaterial Material(u8 flags)
{
	NativeDraw3DMaterial m;
	memset(&m, 0, sizeof(m));
	m.flags = flags;
	return m;
}

static void ModeAndDepth(void)
{
	gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
	gNativeDepthBufferEnabled = 0;
	assert(!NATIVE_DEPTH_BUFFER_ACTIVE());
	NativeDraw3DView view = TestView();
	NativeDraw3D_BeginFrame();
	assert(NativeDraw3D_BeginLayer(&view) == -1);

	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	assert(NATIVE_DEPTH_BUFFER_ACTIVE());
	assert(NATIVE_VERTEX_TRACKING_ACTIVE());
	assert(NativeDraw3D_BeginLayer(&view) == 0);
}

static void Winding(void)
{
	// Anticlockwise on a y-down screen: retail NCLIP > 0.
	const float a[3] = {0.0f, 0.0f, 100.0f};
	const float b[3] = {10.0f, 0.0f, 100.0f};
	const float c[3] = {0.0f, 10.0f, 100.0f};
	assert(NativeDraw3D_IsFrontFacing(a, b, c, 0));
	assert(!NativeDraw3D_IsFrontFacing(a, c, b, 0));
	assert(!NativeDraw3D_IsFrontFacing(a, b, c, 1));
	assert(NativeDraw3D_IsFrontFacing(a, c, b, 1));

	// One corner behind the camera: the camera-space test still holds.
	const float behind[3] = {0.0f, 10.0f, -50.0f};
	const float front0[3] = {-10.0f, 0.0f, 100.0f};
	const float front1[3] = {10.0f, 0.0f, 100.0f};
	assert(NativeDraw3D_IsFrontFacing(front0, front1, behind, 0) == !NativeDraw3D_IsFrontFacing(front1, front0, behind, 0));
}

static void Culling(void)
{
	NativeDraw3DView view = TestView();
	NativeDraw3D_BeginFrame();
	const int layer = NativeDraw3D_BeginLayer(&view);
	const NativeDraw3DMaterial single = Material(0);
	const NativeDraw3DMaterial both = Material(NATIVE_DRAW3D_DOUBLE_SIDED);

	NativeDraw3DVertex a = Vertex(0, 0, 100), b = Vertex(10, 0, 100), c = Vertex(0, 10, 100);
	assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &single) == 1);
	assert(NativeDraw3D_AddTriangle(layer, &a, &c, &b, &single) == 0);
	assert(NativeDraw3D_AddTriangle(layer, &a, &c, &b, &both) == 1);

	// Entirely nearer than the near plane, or behind the camera.
	NativeDraw3DVertex n0 = Vertex(0, 0, 10), n1 = Vertex(10, 0, 10), n2 = Vertex(0, 10, 10);
	assert(NativeDraw3D_AddTriangle(layer, &n0, &n1, &n2, &both) == 0);
	// Entirely left of the viewport.
	NativeDraw3DVertex l0 = Vertex(-1000, 0, 100), l1 = Vertex(-900, 0, 100), l2 = Vertex(-1000, 10, 100);
	assert(NativeDraw3D_AddTriangle(layer, &l0, &l1, &l2, &both) == 0);
	// Crossing the near plane: kept for the host clipper.
	NativeDraw3DVertex x0 = Vertex(-10, 0, 100), x1 = Vertex(10, 0, 100), x2 = Vertex(0, 10, -100);
	assert(NativeDraw3D_AddTriangle(layer, &x0, &x1, &x2, &both) == 1);

	NativeDraw3D_EndLayer(layer);
	const NativeDraw3DLayer *info = NativeDraw3D_GetLayer(layer);
	assert(info->firstTriangle == 0);
	assert(info->triangleCount == 3);
	// After EndLayer the layer is closed.
	assert(NativeDraw3D_AddTriangle(layer, &a, &b, &c, &single) == 0);
}

static void TransformsAndLayers(void)
{
	NativeDraw3DView view = TestView();
	view.translation[2] = 50.0;
	NativeDraw3D_BeginFrame();
	const int first = NativeDraw3D_BeginLayer(&view);
	const NativeDraw3DMaterial both = Material(NATIVE_DRAW3D_DOUBLE_SIDED);

	NativeDraw3DVertex a = Vertex(0, 0, 100), b = Vertex(10, 0, 100), c = Vertex(0, 10, 150);
	assert(NativeDraw3D_AddTriangle(first, &a, &b, &c, &both) == 1);
	const NativeDraw3DTriangle *triangle = &NativeDraw3D_GetTriangles()[0];
	assert(fabsf(triangle->position[0][2] - 150.0f) < 1e-4f);
	assert(fabsf(triangle->sortDepth - 200.0f) < 1e-4f);

	// Object transform: rotate 90 degrees about y and move 100 forward.
	const double rotation[9] = {0, 0, 1, 0, 1, 0, -1, 0, 0};
	const double translation[3] = {0, 0, 100};
	NativeDraw3D_SetObjectTransform(first, rotation, translation);
	NativeDraw3DVertex p = Vertex(10, 0, 0), q = Vertex(0, 0, 10), r = Vertex(0, 10, 0);
	assert(NativeDraw3D_AddTriangle(first, &p, &q, &r, &both) == 1);
	triangle = &NativeDraw3D_GetTriangles()[1];
	assert(fabsf(triangle->position[0][0] - 0.0f) < 1e-4f && fabsf(triangle->position[0][2] - 90.0f) < 1e-4f);
	assert(fabsf(triangle->position[1][0] - 10.0f) < 1e-4f && fabsf(triangle->position[1][2] - 100.0f) < 1e-4f);

	// A second layer starts after the first; the first can no longer grow.
	const int second = NativeDraw3D_BeginLayer(&view);
	assert(second == first + 1);
	assert(NativeDraw3D_AddTriangle(first, &a, &b, &c, &both) == 0);
	assert(NativeDraw3D_AddTriangle(second, &a, &b, &c, &both) == 1);
	assert(NativeDraw3D_GetLayer(second)->firstTriangle == 2);

	// A new frame resets the storage.
	NativeDraw3D_BeginFrame();
	assert(NativeDraw3D_GetLayer(first) == NULL);
	assert(NativeDraw3D_BeginLayer(&view) == 0);
}

static void Marker(void)
{
	DR_PSYX_DRAW3D marker;
	memset(&marker, 0, sizeof(marker));
	NativeDraw3D_SetMarker(&marker, 5);
	assert((marker.code & 0xff000000u) == 0xB3000000u);
	assert((marker.code & 0x00ffffffu) == 5);
	assert(getlen(&marker) == 1);
}

int main(void)
{
	ModeAndDepth();
	Winding();
	Culling();
	TransformsAndLayers();
	Marker();
	puts("native draw3d tests passed");
	return 0;
}
