/*
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/include/PsyX/PsyX_render.h
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#ifndef NATIVE_RENDERER_TYPES_H
#define NATIVE_RENDERER_TYPES_H

#include <macros.h>
#include <psx/libgte.h>
#include <psx/libgpu.h>
#include <platform/native_pgxp.h>

#define LUT_WIDTH              (256)
#define LUT_HEIGHT             (256)

#define VRAM_WIDTH             (1024)
#define VRAM_HEIGHT            (512)

#define TPAGE_WIDTH            (256)
#define TPAGE_HEIGHT           (256)

#if defined(__vita__)
#define MAX_VERTEX_BUFFER_SIZE (1u << 16)
#else
// Max detail subdivides all level geometry; one 1P frame can pass 64K
// vertices, and split screen multiplies that.
#define MAX_VERTEX_BUFFER_SIZE (1u << 19)
#endif

#pragma pack(push, 1)
typedef struct
{
#if NATIVE_PGXP_SUPPORTED
	// Sub-pixel when PGXP recovered the GTE vertex, otherwise retail integers.
	// Native geometry (clipSpace set) holds screen xy times clip w here.
	float x, y;
	// Perspective divisor for PGXP polygons; 0 keeps retail affine mapping.
	// Clip depth for native 3D vertices.
	float w;
	// Optional world depth, independent of the texture perspective divisor.
	// Clip w (camera depth) for native 3D vertices.
	float depth;
	s16 page, clut;
#else
	s16 x, y, page, clut;
#endif

	u8 u, v, bright, dither;
	u8 r, g, b, a;

	// clipSpace marks homogeneous native geometry. Screen geometry uses W=1,
	// Z=0 with depth disabled; native 3D uses camera depth (native_draw3d.h).
	s8 tcx, tcy, clipSpace, _p1;
	u16 orderDepth;
#if NATIVE_PGXP_SUPPORTED
	// Keeps the float attributes 4-byte aligned in the vertex buffer.
	u16 _p2;
#endif
} GrVertex;
#pragma pack(pop)

typedef enum
{
	a_position,
	a_texcoord,
	a_color,
	a_extra,
	a_order_depth,
	a_page_clut,
} ShaderAttrib;

typedef enum
{
	BM_NONE,
	BM_AVERAGE,
	BM_ADD,
	BM_SUBTRACT,
	BM_ADD_QUATER_SOURCE,
	// Not a PS1 mode: source-alpha coverage for anti-aliased font glyphs.
	BM_STRAIGHT_ALPHA
} BlendMode;

typedef enum
{
	TF_4_BIT,
	TF_8_BIT,
	TF_16_BIT,

	TF_32_BIT_RGBA,
	// Single-channel signed distance field font atlas (native_font.h).
	TF_TEXT_SDF
} TexFormat;

typedef u32 TextureID;
typedef u32 ShaderID;

#endif
