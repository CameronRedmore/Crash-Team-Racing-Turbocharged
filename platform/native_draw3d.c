#include <macros.h>
#include "platform/native_draw3d.h"

#include <psx/libgte.h>
#include <psx/libgpu.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "platform/native_log.h"

int gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
int gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;

float NativeDraw3D_GetDrawOrderSlotDepth(void)
{
	static float slotDepth = -1.0f;
	if (slotDepth < 0.0f)
	{
		slotDepth = NATIVE_DRAW3D_DRAW_ORDER_SLOT_DEPTH;
#if defined(CTR_INTERNAL)
		const char *env = getenv("CTR_DRAW_ORDER_SLOT_DEPTH");
		if ((env != NULL) && (env[0] != '\0'))
		{
			const float value = strtof(env, NULL);
			slotDepth = value > 0.0f ? value : 0.0f;
			Platform_Log("[CTR Draw3D] draw-order slot depth: %.1f\n", slotDepth);
		}
#endif
	}
	return slotDepth;
}

#if NATIVE_DRAW3D_SUPPORTED

// Instance/viewport entries, particle billboards, ground decals and effects.
#define NATIVE_DRAW3D_MAX_LAYERS 16384

global_variable NativeDraw3DTriangle s_draw3dTriangles[NATIVE_DRAW3D_MAX_TRIANGLES];
global_variable u32 s_draw3dTriangleCount;
global_variable NativeDraw3DLayer s_draw3dLayers[NATIVE_DRAW3D_MAX_LAYERS];
global_variable int s_draw3dLayerCount;

static const char *const s_draw3dDiagnosticNames[NATIVE_DRAW3D_DIAG_COUNT] = {
    "model skipped: unsupported setup/selector",
    "model unsupported draw handler",
    "model unsupported primitive",
    "model Classic fallback: invalid depth scale",
    "model Classic fallback: missing OT range",
    "model Classic fallback: stale shared OT range",
    "level skipped: missing input",
    "overlay drawn without depth: nested pass",
    "overlay drawn without depth: unexpected target",
    "overlay drawn without depth: incomplete framebuffer",
    "projected polygon packet using geometry recovery",
};
static u64 s_draw3dDiagnosticTotals[NATIVE_DRAW3D_DIAG_COUNT];
static u64 s_draw3dDiagnosticInterval[NATIVE_DRAW3D_DIAG_COUNT];
static double s_draw3dReportTime;

static void NativeDraw3D_ReportDiagnostics(void)
{
	if (!NATIVE_DRAW3D_ACTIVE() && s_draw3dLayerCount == 0)
		return;
	// time() rather than timespec_get(): the i686 MinGW (msvcrt) runtime lacks TIME_UTC.
	const time_t now = time(NULL);
	if (now == (time_t)-1)
		return;
	const double seconds = (double)now;
	if (s_draw3dReportTime == 0.0 || seconds < s_draw3dReportTime)
	{
		s_draw3dReportTime = seconds;
		return;
	}
	if (seconds - s_draw3dReportTime < 10.0)
		return;
	for (int i = 0; i < NATIVE_DRAW3D_DIAG_COUNT; i++)
	{
		if (s_draw3dDiagnosticInterval[i])
			Platform_LogWarn("[CTR Draw3D] %s: interval=%llu, session=%llu\n", s_draw3dDiagnosticNames[i], (unsigned long long)s_draw3dDiagnosticInterval[i],
			                 (unsigned long long)s_draw3dDiagnosticTotals[i]);
		s_draw3dDiagnosticInterval[i] = 0;
	}
	s_draw3dReportTime = seconds;
}

void NativeDraw3D_BeginFrame(void)
{
	// Report recovery events before releasing the completed frame.
	NativeDraw3D_ReportDiagnostics();
	s_draw3dTriangleCount = 0;
	s_draw3dLayerCount = 0;
}

int NativeDraw3D_BeginLayer(const NativeDraw3DView *view)
{
	if (!NATIVE_DRAW3D_ACTIVE() || (view == NULL))
	{
		return -1;
	}
	if (s_draw3dLayerCount >= NATIVE_DRAW3D_MAX_LAYERS)
	{
		return -1;
	}

	// Layers fill one at a time so each stays contiguous.
	if (s_draw3dLayerCount > 0)
	{
		s_draw3dLayers[s_draw3dLayerCount - 1].open = 0;
	}

	const int index = s_draw3dLayerCount++;
	NativeDraw3DLayer *layer = &s_draw3dLayers[index];
	layer->view = *view;
	memcpy(layer->objectRotation, view->rotation, sizeof(layer->objectRotation));
	memcpy(layer->objectTranslation, view->translation, sizeof(layer->objectTranslation));
	layer->firstTriangle = s_draw3dTriangleCount;
	layer->triangleCount = 0;
	layer->open = 1;
	return index;
}

void NativeDraw3D_EndLayer(int layer)
{
	if ((layer >= 0) && (layer < s_draw3dLayerCount))
	{
		s_draw3dLayers[layer].open = 0;
	}
}

void NativeDraw3D_SetObjectTransform(int layer, const double *rotation, const double *translation)
{
	if ((layer < 0) || (layer >= s_draw3dLayerCount))
	{
		return;
	}
	memcpy(s_draw3dLayers[layer].objectRotation, rotation, sizeof(s_draw3dLayers[layer].objectRotation));
	memcpy(s_draw3dLayers[layer].objectTranslation, translation, sizeof(s_draw3dLayers[layer].objectTranslation));
}

internal void NativeDraw3D_Transform(const NativeDraw3DLayer *layer, const NativeDraw3DVertex *vertex, float *out)
{
	const double *r = layer->objectRotation;
	const double *t = layer->objectTranslation;
	const double x = vertex->x;
	const double y = vertex->y;
	const double z = vertex->z;

	out[0] = (float)(r[0] * x + r[1] * y + r[2] * z + t[0]);
	out[1] = (float)(r[3] * x + r[4] * y + r[5] * z + t[1]);
	out[2] = (float)(r[6] * x + r[7] * y + r[8] * z + t[2]);
}

// Screen-space signed area of a projected triangle has the sign of the
// camera-space determinant det(a, b, c) whenever all three depths are
// positive, and the determinant stays meaningful for triangles crossing the
// near plane. Retail NCLIP > 0 (screen y down) is the front face; mirror mode
// negates both, so this test runs before the mirror.
int NativeDraw3D_IsFrontFacing(const float *a, const float *b, const float *c, int reverseWinding)
{
	const double det = (double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1]) - (double)a[1] * ((double)b[0] * c[2] - (double)b[2] * c[0]) +
	                   (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0]);

	return reverseWinding ? (det < 0.0) : (det > 0.0);
}

// Conservative rejection against each frustum plane. The planes are linear in
// camera space, so a triangle whose three corners are outside the same one
// cannot touch the viewport regardless of their depth signs.
internal int NativeDraw3D_IsOutsideFrustum(const NativeDraw3DView *view, float position[3][3])
{
	int outside[5] = {1, 1, 1, 1, 1};

	for (int i = 0; i < 3; i++)
	{
		const float x = position[i][0];
		const float y = position[i][1];
		const float z = position[i][2];
		// screen = centre + H * xy / z, inside when 0 <= screen <= size.
		const float sx = view->projection * x + view->centerX * z;
		const float sy = view->projection * y + view->centerY * z;

		outside[0] &= z < NATIVE_DRAW3D_NEAR_PLANE;
		outside[1] &= sx < 0.0f;
		outside[2] &= sx > view->width * z;
		outside[3] &= sy < 0.0f;
		outside[4] &= sy > view->height * z;
	}

	return outside[0] | outside[1] | outside[2] | outside[3] | outside[4];
}

int NativeDraw3D_AddTriangle(int layerIndex, const NativeDraw3DVertex *v0, const NativeDraw3DVertex *v1, const NativeDraw3DVertex *v2,
                             const NativeDraw3DMaterial *material)
{
	if ((layerIndex < 0) || (layerIndex >= s_draw3dLayerCount))
	{
		return 0;
	}

	NativeDraw3DLayer *layer = &s_draw3dLayers[layerIndex];
	if (!layer->open)
	{
		return 0;
	}
	if (s_draw3dTriangleCount >= NATIVE_DRAW3D_MAX_TRIANGLES)
	{
		return 0;
	}

	NativeDraw3DTriangle *triangle = &s_draw3dTriangles[s_draw3dTriangleCount];
	const NativeDraw3DVertex *vertices[3] = {v0, v1, v2};

	for (int i = 0; i < 3; i++)
	{
		NativeDraw3D_Transform(layer, vertices[i], triangle->position[i]);
	}

	if (((material->flags & NATIVE_DRAW3D_DOUBLE_SIDED) == 0) &&
	    !NativeDraw3D_IsFrontFacing(triangle->position[0], triangle->position[1], triangle->position[2],
	                                (material->flags & NATIVE_DRAW3D_REVERSE_WINDING) != 0))
	{
		return 0;
	}
	// Account for the post-mirror screen offset in conservative rejection.
	NativeDraw3DView cullView = layer->view;
	cullView.centerX += layer->view.mirror ? -material->screenOffsetX : material->screenOffsetX;
	if (NativeDraw3D_IsOutsideFrustum(&cullView, triangle->position))
	{
		return 0;
	}

	float sortDepth = triangle->position[0][2];
	for (int i = 0; i < 3; i++)
	{
		triangle->uv[i][0] = vertices[i]->u;
		triangle->uv[i][1] = vertices[i]->v;
		triangle->color[i][0] = vertices[i]->r;
		triangle->color[i][1] = vertices[i]->g;
		triangle->color[i][2] = vertices[i]->b;
		if (triangle->position[i][2] > sortDepth)
		{
			sortDepth = triangle->position[i][2];
		}
	}
	triangle->material = *material;
	triangle->sortDepth = sortDepth + (float)material->depthSlots * NativeDraw3D_GetDrawOrderSlotDepth();

	s_draw3dTriangleCount++;
	layer->triangleCount++;
	return 1;
}

int NativeDraw3D_AddQuad(int layer, const NativeDraw3DVertex *v0, const NativeDraw3DVertex *v1, const NativeDraw3DVertex *v2, const NativeDraw3DVertex *v3,
                         const NativeDraw3DMaterial *material)
{
	return NativeDraw3D_AddTriangle(layer, v0, v1, v2, material) + NativeDraw3D_AddTriangle(layer, v1, v3, v2, material);
}

int NativeDraw3D_AddLine(int layerIndex, const NativeDraw3DVertex *v0, const NativeDraw3DVertex *v1, const NativeDraw3DMaterial *material, float width)
{
	if (layerIndex < 0 || layerIndex >= s_draw3dLayerCount || width <= 0.0f)
		return 0;
	NativeDraw3DLayer *layer = &s_draw3dLayers[layerIndex];
	if (!layer->open || layer->view.projection <= 0.0f)
		return 0;
	float position[2][3];
	NativeDraw3D_Transform(layer, v0, position[0]);
	NativeDraw3D_Transform(layer, v1, position[1]);
	if (position[0][2] < NATIVE_DRAW3D_NEAR_PLANE && position[1][2] < NATIVE_DRAW3D_NEAR_PLANE)
		return 0;
	NativeDraw3DVertex ends[2] = {*v0, *v1};
	for (int i = 0; i < 2; i++)
	{
		if (position[i][2] < NATIVE_DRAW3D_NEAR_PLANE)
		{
			int other = i ^ 1;
			float t = (NATIVE_DRAW3D_NEAR_PLANE - position[i][2]) / (position[other][2] - position[i][2]);
			for (int axis = 0; axis < 3; axis++)
				position[i][axis] += t * (position[other][axis] - position[i][axis]);
			position[i][2] = NATIVE_DRAW3D_NEAR_PLANE;
			ends[i].r = (u8)(ends[i].r + t * ((float)ends[other].r - ends[i].r));
			ends[i].g = (u8)(ends[i].g + t * ((float)ends[other].g - ends[i].g));
			ends[i].b = (u8)(ends[i].b + t * ((float)ends[other].b - ends[i].b));
		}
	}
	float dx = position[1][0] / position[1][2] - position[0][0] / position[0][2];
	float dy = position[1][1] / position[1][2] - position[0][1] / position[0][2];
	float length = sqrtf(dx * dx + dy * dy);
	// A stationary weather point is still a visible pixel.
	float nx = length > 1.0e-8f ? -dy / length : 1.0f;
	float ny = length > 1.0e-8f ? dx / length : 0.0f;
	NativeDraw3DVertex vertices[4];
	for (int i = 0; i < 4; i++)
	{
		int end = i >> 1;
		float halfWidth = width * 0.5f * position[end][2] / layer->view.projection;
		float sign = (i & 1) ? 1.0f : -1.0f;
		vertices[i] = ends[end];
		vertices[i].x = position[end][0] + nx * sign * halfWidth;
		vertices[i].y = position[end][1] + ny * sign * halfWidth;
		vertices[i].z = position[end][2];
		if (length <= 1.0e-8f)
			vertices[i].y += (end ? 1.0f : -1.0f) * halfWidth;
	}
	// The generated ribbon is already in camera space. Restore the object's
	// transform afterwards so subsequent particles share the original layer.
	double savedRotation[9], savedTranslation[3];
	memcpy(savedRotation, layer->objectRotation, sizeof(savedRotation));
	memcpy(savedTranslation, layer->objectTranslation, sizeof(savedTranslation));
	static const double identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
	static const double zero[3] = {0, 0, 0};
	NativeDraw3D_SetObjectTransform(layerIndex, identity, zero);
	NativeDraw3DMaterial lineMaterial = *material;
	lineMaterial.flags |= NATIVE_DRAW3D_DOUBLE_SIDED;
	int count = NativeDraw3D_AddQuad(layerIndex, &vertices[0], &vertices[1], &vertices[2], &vertices[3], &lineMaterial);
	NativeDraw3D_SetObjectTransform(layerIndex, savedRotation, savedTranslation);
	return count;
}

void NativeDraw3D_SetMarker(void *packet, int layer)
{
	DR_PSYX_DRAW3D *marker = (DR_PSYX_DRAW3D *)packet;
	marker->code = 0xB3000000u | ((u32)layer & 0x00ffffffu);
	setlen(marker, 1);
}

const NativeDraw3DLayer *NativeDraw3D_GetLayer(int layer)
{
	if ((layer < 0) || (layer >= s_draw3dLayerCount))
	{
		return NULL;
	}
	return &s_draw3dLayers[layer];
}

const NativeDraw3DTriangle *NativeDraw3D_GetTriangles(void)
{
	return s_draw3dTriangles;
}

#endif

void NativeDraw3D_ReportDiagnostic(enum NativeDraw3DDiagnosticEvent event, const char *source, u32 detail)
{
#if NATIVE_DRAW3D_SUPPORTED
	if (!NATIVE_DRAW3D_ACTIVE() || event < 0 || event >= NATIVE_DRAW3D_DIAG_COUNT)
		return;
	s_draw3dDiagnosticInterval[event]++;
	if (++s_draw3dDiagnosticTotals[event] == 1)
		Platform_LogWarn("[CTR Draw3D] first %s; source=%s detail=0x%08x\n", s_draw3dDiagnosticNames[event], source ? source : "unknown", detail);
#else
	(void)event;
	(void)source;
	(void)detail;
#endif
}
