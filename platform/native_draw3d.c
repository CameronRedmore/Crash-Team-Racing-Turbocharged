#include <macros.h>
#include "platform/native_draw3d.h"

#include <psx/libgte.h>
#include <psx/libgpu.h>
#include <string.h>

#include "platform/native_log.h"

int gNativeRendererMode = NATIVE_RENDERER_CLASSIC;
int gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;

#if NATIVE_DRAW3D_SUPPORTED

#define NATIVE_DRAW3D_MAX_LAYERS 64

global_variable NativeDraw3DTriangle s_draw3dTriangles[NATIVE_DRAW3D_MAX_TRIANGLES];
global_variable u32 s_draw3dTriangleCount;
global_variable NativeDraw3DLayer s_draw3dLayers[NATIVE_DRAW3D_MAX_LAYERS];
global_variable int s_draw3dLayerCount;

void NativeDraw3D_BeginFrame(void)
{
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
		local_persist b32 s_reported;
		if (!s_reported)
		{
			s_reported = 1;
			Platform_LogError("[CTR Draw3D] %s\n", "layer limit reached, dropping native geometry");
		}
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
		local_persist b32 s_reported;
		if (!s_reported)
		{
			s_reported = 1;
			Platform_LogError("[CTR Draw3D] %s\n", "triangle limit reached, dropping native geometry");
		}
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
	if (NativeDraw3D_IsOutsideFrustum(&layer->view, triangle->position))
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
	triangle->sortDepth = sortDepth;

	s_draw3dTriangleCount++;
	layer->triangleCount++;
	return 1;
}

int NativeDraw3D_AddQuad(int layer, const NativeDraw3DVertex *v0, const NativeDraw3DVertex *v1, const NativeDraw3DVertex *v2,
                         const NativeDraw3DVertex *v3, const NativeDraw3DMaterial *material)
{
	return NativeDraw3D_AddTriangle(layer, v0, v1, v2, material) + NativeDraw3D_AddTriangle(layer, v1, v3, v2, material);
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
