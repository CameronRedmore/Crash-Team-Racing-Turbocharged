#ifndef NATIVE_DRAW3D_H
#define NATIVE_DRAW3D_H

#include <macros.h>

// NOTE: Depth-first native 3D path. Retail draw code projects every vertex to
// an integer screen position and sorts polygons through the ordering table;
// PGXP then tries to recover what was lost from the packet words. Native
// draw code instead submits triangles with exact camera-space positions into
// a layer. A one-word marker packet links the layer into the OT so it lands
// in the right viewport, draw environment and order relative to retail 2D
// work. The GPU parser expands the layer into homogeneous clip-space vertices:
// the host rasteriser clips at the near plane, interpolates in perspective
// and depth tests with the same depth encoding PGXP uses, so native and
// retail geometry share one depth buffer while both paths coexist.

#if defined(__vita__) || defined(__EMSCRIPTEN__)
#define NATIVE_DRAW3D_SUPPORTED 0
#else
#define NATIVE_DRAW3D_SUPPORTED 1
#endif

enum NativeRendererMode
{
	// Retail packets, optionally refined by PGXP.
	NATIVE_RENDERER_CLASSIC,
	// Converted draw paths submit native 3D layers; the rest stay classic.
	NATIVE_RENDERER_NATIVE,
	NATIVE_RENDERER_MODE_COUNT,
};

enum NativeColorDepth
{
	// Full 8-bit-per-channel output, dithering optional.
	NATIVE_COLOR_DEPTH_TRUE,
	// PS1 framebuffer precision: 5 bits per channel after dithering.
	NATIVE_COLOR_DEPTH_15BIT,
	NATIVE_COLOR_DEPTH_COUNT,
};

enum NativeTextureFilter
{
	NATIVE_TEXTURE_FILTER_NEAREST,
	NATIVE_TEXTURE_FILTER_BILINEAR,
	NATIVE_TEXTURE_FILTER_COUNT,
};

extern int gNativeRendererMode;
extern int gNativeColorDepth;

#if NATIVE_DRAW3D_SUPPORTED
#define NATIVE_DRAW3D_ACTIVE() (gNativeRendererMode == NATIVE_RENDERER_NATIVE)
#else
#define NATIVE_DRAW3D_ACTIVE() 0
#endif

// Matches the PGXP depth encoding: clip w is camera depth and NDC depth is
// 1 - 2 * near / depth, so geometry closer than this is clipped.
#define NATIVE_DRAW3D_NEAR_PLANE 32.0f

enum NativeDraw3DFlags
{
	NATIVE_DRAW3D_TEXTURED = 0x01,
	// Retail semi-transparency: blend with the tpage's blend mode.
	NATIVE_DRAW3D_SEMI_TRANS = 0x02,
	// Draw both windings (retail double-sided quadblocks).
	NATIVE_DRAW3D_DOUBLE_SIDED = 0x04,
	// Front faces wind clockwise on screen instead of anticlockwise.
	NATIVE_DRAW3D_REVERSE_WINDING = 0x08,
	// Retail dither bit of the draw mode.
	NATIVE_DRAW3D_DITHER = 0x10,
	// Native super turbo pad tint (see NATIVE_GPU_TPAGE_SUPER_TURBO_TINT).
	NATIVE_DRAW3D_SUPER_TURBO_TINT = 0x20,
};

typedef struct
{
	// Retail draw-mode word: texture page, blend mode and colour depth.
	u16 tpage;
	u16 clut;
	u8 flags;
	// Retail OT draw-order bias. Negative values draw over coplanar surfaces.
	s8 depthBias;
} NativeDraw3DMaterial;

typedef struct
{
	float x, y, z;
	u8 u, v;
	u8 r, g, b;
} NativeDraw3DVertex;

typedef struct
{
	// Camera space, GTE convention: +z forward, screen = centre + H * xy / z.
	float position[3][3];
	u8 uv[3][2];
	u8 color[3][3];
	NativeDraw3DMaterial material;
	// Farthest camera depth, for back-to-front translucent ordering.
	float sortDepth;
} NativeDraw3DTriangle;

typedef struct
{
	// World to camera, unit scale (retail GTE matrices divided by 4096), with
	// the retail aspect and widescreen scaling already applied.
	double rotation[9];
	double translation[3];
	// GTE H and OFX/OFY: projection distance and viewport centre in pixels.
	float projection;
	float centerX;
	float centerY;
	// Viewport size in pixels, for culling.
	float width;
	float height;
	b32 mirror;
} NativeDraw3DView;

typedef struct
{
	NativeDraw3DView view;
	// Object to camera transform applied by NativeDraw3D_AddTriangle.
	double objectRotation[9];
	double objectTranslation[3];
	u32 firstTriangle;
	u32 triangleCount;
	b32 open;
} NativeDraw3DLayer;

#if NATIVE_DRAW3D_SUPPORTED

// Max detail with four viewports stays well below this.
#define NATIVE_DRAW3D_MAX_TRIANGLES (1u << 18)

// Drops last frame's layers. Call before the frame starts submitting.
void NativeDraw3D_BeginFrame(void);
// Returns a layer index, or -1 when native drawing is off or storage is full.
// Layers fill one at a time: beginning a layer ends the previous one.
int NativeDraw3D_BeginLayer(const NativeDraw3DView *view);
void NativeDraw3D_EndLayer(int layer);
// Object to camera transform for following triangles; the layer starts with
// its view transform (world space input).
void NativeDraw3D_SetObjectTransform(int layer, const double *rotation, const double *translation);
// Returns 1 when the triangle was kept, 0 when culled or out of storage.
int NativeDraw3D_AddTriangle(int layer, const NativeDraw3DVertex *v0, const NativeDraw3DVertex *v1, const NativeDraw3DVertex *v2,
                             const NativeDraw3DMaterial *material);
// PS1 quad order: triangles (v0, v1, v2) and (v1, v3, v2).
int NativeDraw3D_AddQuad(int layer, const NativeDraw3DVertex *v0, const NativeDraw3DVertex *v1, const NativeDraw3DVertex *v2,
                         const NativeDraw3DVertex *v3, const NativeDraw3DMaterial *material);

// Writes a DR_PSYX_DRAW3D marker that draws `layer` at its OT position.
void NativeDraw3D_SetMarker(void *packet, int layer);

// GPU side.
const NativeDraw3DLayer *NativeDraw3D_GetLayer(int layer);
const NativeDraw3DTriangle *NativeDraw3D_GetTriangles(void);

// Exposed for tests.
int NativeDraw3D_IsFrontFacing(const float *a, const float *b, const float *c, int reverseWinding);

#else

static inline void NativeDraw3D_BeginFrame(void)
{
}
static inline int NativeDraw3D_BeginLayer(const NativeDraw3DView *view)
{
	(void)view;
	return -1;
}
static inline void NativeDraw3D_EndLayer(int layer)
{
	(void)layer;
}

#endif

#endif
