/*
 * Shader sources and shader objects: the PSX GTE/texture-sampling shaders, the
 * full-screen blit pipelines (VRAM pack, presentation, SSAA resolve, pause
 * backdrop, projected world) and the 15-bit colour LUT.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdbool.h>

#include "platform/native_log.h"
#include "platform/native_options.h"
#include "platform/native_renderer_internal.h"

#ifdef __vita__
enum NativePsxShaderVariant
{
	NATIVE_PSX_SHADER_OPAQUE,
	NATIVE_PSX_SHADER_OPAQUE_AVERAGE,
	NATIVE_PSX_SHADER_OPAQUE_QUARTER,
	NATIVE_PSX_SHADER_NON_STP,
	NATIVE_PSX_SHADER_STP,
	NATIVE_PSX_SHADER_STP_AVERAGE,
	NATIVE_PSX_SHADER_STP_QUARTER,
	NATIVE_PSX_SHADER_MIXED_AVERAGE,
	NATIVE_PSX_SHADER_MIXED_ADD,
	NATIVE_PSX_SHADER_MIXED_QUARTER,
	NATIVE_PSX_SHADER_VARIANT_COUNT
};
#endif
#ifdef __vita__
global_variable GTEShader s_gteShaderVariants[4][NATIVE_PSX_SHADER_VARIANT_COUNT];
global_variable GTEShader s_gteCachedP4ShaderVariants[NATIVE_PSX_SHADER_VARIANT_COUNT];
global_variable GTEShader s_gteFullyOpaqueShaderVariants[2][3];
global_variable GTEShader s_gteUntexturedShaderVariants[3];
#else
global_variable GTEShader s_gteShader4;
global_variable GTEShader s_gteShader8;
global_variable GTEShader s_gteShader16;
global_variable GTEShader s_gteShader4SuperTurbo;
global_variable GTEShader s_gteShader8SuperTurbo;
global_variable GTEShader s_gteShader16SuperTurbo;
global_variable GTEShader s_gteShader32Rgba;
global_variable GTEShader s_gteShaderTextSdf;
#endif

GLint u_projectionLoc;
GLint u_bilinearFilterLoc;
GLint u_texelSizeLoc;
#ifndef __vita__
GLint u_psxSemiTransPassLoc;
GLint u_psxDitherEnabledLoc;
GLint u_psxColorDepth15Loc;
#endif
GLint u_psxDrawMaskSetLoc;
GLint u_psxTextureOutputStpLoc;

internal void NativeRenderer_DestroyPSXShaders(void)
{
#ifdef __vita__
	for (int format = TF_4_BIT; format <= TF_32_BIT_RGBA; format++)
	{
		for (int variant = 0; variant < NATIVE_PSX_SHADER_VARIANT_COUNT; variant++)
		{
			if (s_gteShaderVariants[format][variant].shader != 0)
			{
				glDeleteProgram(s_gteShaderVariants[format][variant].shader);
			}
		}
	}
	for (int variant = 0; variant < NATIVE_PSX_SHADER_VARIANT_COUNT; variant++)
	{
		if (s_gteCachedP4ShaderVariants[variant].shader != 0)
		{
			glDeleteProgram(s_gteCachedP4ShaderVariants[variant].shader);
		}
	}
	for (int format = TF_4_BIT; format <= TF_8_BIT; format++)
	{
		for (int variant = 0; variant < 3; variant++)
		{
			glDeleteProgram(s_gteFullyOpaqueShaderVariants[format][variant].shader);
		}
	}
	for (int variant = 0; variant < 3; variant++)
	{
		glDeleteProgram(s_gteUntexturedShaderVariants[variant].shader);
	}
#else
	glDeleteProgram(s_gteShader4.shader);
	glDeleteProgram(s_gteShader8.shader);
	glDeleteProgram(s_gteShader16.shader);
	glDeleteProgram(s_gteShader4SuperTurbo.shader);
	glDeleteProgram(s_gteShader8SuperTurbo.shader);
	glDeleteProgram(s_gteShader16SuperTurbo.shader);
	glDeleteProgram(s_gteShader32Rgba.shader);
	glDeleteProgram(s_gteShaderTextSdf.shader);
#endif
}

#ifdef __vita__
#define GPU_SAMPLE_TEXTURE_4BIT_FUNC                                                             \
	"\tvec2 samplePSX(vec2 tc) {\n"                                                              \
	"\t\tvec2 texel = floor(tc + vec2(0.5));\n"                                                  \
	"\t\tfloat texelX = texel.x;\n"                                                              \
	"\t\tfloat lanePhase = fract(texelX * 0.25);\n"                                              \
	"\t\tfloat highByte = step(0.5, lanePhase);\n"                                               \
	"\t\tfloat highNibbleSel = step(0.5, fract(texelX * 0.5));\n"                                \
	"\t\tvec2 packedPixel = v_page_clut.xy + vec2(floor(texelX * 0.25), texel.y);\n"             \
	"\t\tvec2 packedRg = VRAM((packedPixel + vec2(0.5)) * c_VRAMTexel);\n"                       \
	"\t\tfloat packedByte = floor(mix(packedRg.x, packedRg.y, highByte) * 255.0 + 0.5);\n"       \
	"\t\tfloat highNibble = floor(packedByte * (1.0 / 16.0));\n"                                 \
	"\t\tfloat paletteIndex = mix(packedByte - highNibble * 16.0, highNibble, highNibbleSel);\n" \
	"\t\treturn VRAM((v_page_clut.zw + vec2(paletteIndex + 0.5, 0.5)) * c_VRAMTexel);\n"         \
	"\t}\n"
#else
#define GPU_SAMPLE_TEXTURE_4BIT_FUNC                                                   \
	"	// returns 16 bit colour\n"                                                      \
	"	vec2 samplePSX(vec2 tc) {\n"                                                     \
	"		vec2 uv = (tc * vec2(0.25, 1.0) + v_page_clut.xy) * c_VRAMTexel;\n"             \
	"		vec2 comp = VRAM(uv);\n"                                                        \
	"		float lane = mod(floor(tc.x + 0.0001), 4.0);\n"                                 \
	"		float byteValue = floor(mix(comp.x, comp.y, step(1.5, lane)) * 255.0 + 0.5);\n" \
	"		float lowNibble = mod(byteValue, 16.0);\n"                                      \
	"		float highNibble = floor(byteValue * (1.0 / 16.0));\n"                          \
	"		float paletteIndex = mix(lowNibble, highNibble, mod(lane, 2.0));\n"             \
	"		vec2 clut_pos = v_page_clut.zw;\n"                                              \
	"		clut_pos.x += paletteIndex * c_VRAMTexel.x;\n"                                  \
	"		return VRAM(clut_pos);\n"                                                       \
	"	}\n"
#endif

#ifdef __vita__
#define GPU_SAMPLE_TEXTURE_8BIT_FUNC                                                                    \
	"\tvec2 samplePSX(vec2 tc) {\n"                                                                     \
	"\t\tfloat highByte = step(0.5, fract(tc.x * 0.5 + 0.25));\n"                                       \
	"\t\tvec2 packedRg = VRAM((v_page_clut.xy + vec2(tc.x * 0.5 + 0.25, tc.y + 0.5)) * c_VRAMTexel);\n" \
	"\t\tfloat paletteIndex = floor(mix(packedRg.x, packedRg.y, highByte) * 255.0 + 0.5);\n"            \
	"\t\treturn VRAM((v_page_clut.zw + vec2(paletteIndex + 0.5, 0.5)) * c_VRAMTexel);\n"                \
	"\t}\n"
#else
#define GPU_SAMPLE_TEXTURE_8BIT_FUNC                                      \
	"	// returns 16 bit colour\n"                                         \
	"	vec2 samplePSX(vec2 tc) {\n"                                        \
	"		vec2 uv = (tc * vec2(0.5, 1.0) + v_page_clut.xy) * c_VRAMTexel;\n" \
	"		vec2 comp = VRAM(uv);\n"                                           \
	"		float lane = mod(floor(tc.x + 0.0001), 2.0);\n"                    \
	"		float paletteIndex = mix(comp.x, comp.y, lane) * 255.0;\n"         \
	"		vec2 clut_pos = v_page_clut.zw;\n"                                 \
	"		clut_pos.x += paletteIndex * c_VRAMTexel.x;\n"                     \
	"		return VRAM(clut_pos);\n"                                          \
	"	}\n"
#endif

#ifdef __vita__
#define GPU_SAMPLE_TEXTURE_16BIT_FUNC                                     \
	"	vec2 samplePSX(vec2 tc) {\n"                                        \
	"\t\treturn VRAM((v_page_clut.xy + tc + vec2(0.5)) * c_VRAMTexel);\n" \
	"	}\n"
#else
#define GPU_SAMPLE_TEXTURE_16BIT_FUNC                    \
	"	vec2 samplePSX(vec2 tc) {\n"                       \
	"		vec2 uv = (tc + v_page_clut.xy) * c_VRAMTexel;\n" \
	"		return VRAM(uv);\n"                               \
	"	}\n"
#endif

#define GPU_FETCH_VRAM_FUNC                                        \
	"	const vec2 c_VRAMTexel = vec2(1.0 / 1024.0, 1.0 / 512.0);\n" \
	"	uniform sampler2D s_texture;\n"                              \
	"	vec2 VRAM(vec2 uv) { return texture2D(s_texture, uv).rg; }\n"

#ifdef __vita__
#define GPU_STP_PASS_FUNC                                                        \
	"	float texelVisible(vec2 rg) { return step(0.5 / 255.0, rg.x + rg.y); }\n"  \
	"	float stpWeight(vec2 rg) { return step(0.5, rg.y); }\n"                    \
	"	bool discardForSemiTransPass(float visible, float stpClass) {\n"           \
	"		if (visible < 0.5) { return true; }\n"                                    \
	"#ifdef PSX_PASS_NON_STP\n		if (stpClass >= 0.5) { return true; }\n#endif\n" \
	"#ifdef PSX_PASS_STP\n		if (stpClass < 0.5) { return true; }\n#endif\n"      \
	"		return false;\n"                                                          \
	"	}\n"
#define GPU_SEMI_TRANS_UNIFORM
#else
#define GPU_STP_PASS_FUNC                                                 \
	"	float texelVisible(vec2 rg) { return float(rg.x + rg.y > 0.0); }\n" \
	"	float stpWeight(vec2 rg) { return step(0.5, rg.y); }\n"             \
	"	bool discardForSemiTransPass(float visible, float stpClass) {\n"    \
	"		if (visible < 0.5) { return true; }\n"                             \
	"		if (psxSemiTransPass == 1 && stpClass >= 0.5) { return true; }\n"  \
	"		if (psxSemiTransPass == 2 && stpClass < 0.5) { return true; }\n"   \
	"		return false;\n"                                                   \
	"	}\n"
#define GPU_SEMI_TRANS_UNIFORM "\tuniform int psxSemiTransPass;\n"
#endif

#ifdef __vita__
#define GPU_DITHERING "\tvec4 dither(vec4 color) { return color; }\n"
#else
// NOTE: v_ditherCoord holds screen xy scaled by clip w, plus w, all
// interpolated in perspective. Dividing gives the exact screen pixel for both
// retail vertices and native clip-space vertices, including near-plane
// clipped native triangles whose off-screen corners have no screen position.
// 15-bit colour truncates to the PS1 framebuffer's 5 bits per channel after
// dithering and expands back like the VRAM presentation does.
#define GPU_DITHERING                                                                       \
	"\tuniform int psxDitherEnabled;\n"                                                     \
	"\tuniform int psxColorDepth15;\n"                                                      \
	"	const mat4 c_dither = mat4(\n"                                                        \
	"		-4.0,  +0.0,  -3.0,  +1.0,\n"                                                        \
	"		+2.0,  -2.0,  +3.0,  -1.0,\n"                                                        \
	"		-3.0,  +1.0,  -4.0,  +0.0,\n"                                                        \
	"		+3.0,  -1.0,  +2.0,  -2.0) / 255.0;\n"                                               \
	"	vec4 dither(vec4 color) {\n"                                                          \
	"		ivec2 dc = ivec2(mod(floor(v_ditherCoord.xy / v_ditherCoord.z), 4.0));\n"            \
	"		color.xyz += vec3(c_dither[dc.x][dc.y] * v_texcoord.w * float(psxDitherEnabled));\n" \
	"		if (psxColorDepth15 != 0) {\n"                                                       \
	"			vec3 color5 = floor(clamp(color.xyz, 0.0, 1.0) * (255.0 / 8.0) + vec3(0.0001));\n"  \
	"			color.xyz = (color5 * 8.0 + floor(color5 * 0.25)) * (1.0 / 255.0);\n"               \
	"		}\n"                                                                                 \
	"		return color;\n"                                                                     \
	"	}\n"                                                                                  \
	"	vec4 psxShade() { return (v_clipSpace > 0.5) ? v_colorPerspective : v_color; }\n"
#endif

#ifdef __vita__
#define GPU_PSX_COLOR_UNIFORM
#define GPU_PSX_COLOR_DECODE                                                \
	"	vec4 decodePSX(vec2 rg) {\n"                                          \
	"		vec2 scaled = rg * vec2(255.0 / 32.0, 255.0 / 4.0);\n"               \
	"		vec2 whole = floor(scaled + vec2(0.001));\n"                         \
	"		vec2 part = scaled - whole;\n"                                       \
	"		float blue = whole.y - step(32.0, whole.y) * 32.0;\n"                \
	"		vec3 color5 = vec3(part.x * 32.0, whole.x + part.y * 32.0, blue);\n" \
	"		return vec4(color5 * (8.0 / 255.0), 1.0);\n"                         \
	"	}\n"
#define GPU_PSX_FRAGMENT_OUTPUT                       \
	"		gl_FragColor.rgb = color.rgb * v_color.rgb;\n" \
	"		gl_FragColor.a = max(psxDrawMaskSet, psxTextureOutputStp * sampledStp);\n"
#else
#define GPU_PSX_COLOR_UNIFORM "\tuniform sampler2D s_rgLut;\n"
#define GPU_PSX_COLOR_DECODE                                     \
	"	const vec2 c_LUTTexel = vec2(1.0 / 256.0, 1.0 / 256.0);\n" \
	"	vec4 decodePSX(vec2 rg) { return texture2D(s_rgLut, rg - c_LUTTexel * 0.0001); }\n"
#define GPU_PSX_FRAGMENT_OUTPUT                                                                          \
	"#ifdef SUPER_TURBO_TINT\n"                                                                          \
	"		vec3 stpColor5 = floor(color.rgb * (255.0 / 8.0) + vec3(0.001));\n"                               \
	"		float stpLum5 = max(stpColor5.r, max(stpColor5.g, stpColor5.b));\n"                               \
	"		float stpBoost5 = min(31.0, floor((stpLum5 * 5.0 + 3.0) * 0.25));\n"                              \
	"		color.rgb = vec3(floor(stpLum5 * 0.3), stpBoost5, min(31.0, stpBoost5 + 2.0)) * (8.0 / 255.0);\n" \
	"#endif\n"                                                                                           \
	"		gl_FragColor = dither(color * psxShade());\n"                                                     \
	"		gl_FragColor.a = max(psxDrawMaskSet, psxTextureOutputStp * sampledStp);\n"
#endif

#ifdef __vita__
// This is used to simulate GL_CONSTANT_COLOR blending since sceGxm has no equivalents for them
#define GPU_PSX_BLEND_APPLY                                                                                       \
	"#ifdef PSX_BLEND_AVERAGE\n		gl_FragColor.a = (127.0 + gl_FragColor.a) / 255.0;\n#endif\n"                    \
	"#ifdef PSX_BLEND_QUARTER\n		gl_FragColor.rgb *= 0.25;\n#endif\n"                                             \
	"#ifdef PSX_MIXED_AVERAGE\n		gl_FragColor.rgb *= 1.0 - sampledStp * 0.5;\n		gl_FragColor.a *= 0.5;\n#endif\n" \
	"#ifdef PSX_MIXED_QUARTER\n		gl_FragColor.rgb *= 1.0 - sampledStp * 0.75;\n#endif\n"
#else
#define GPU_PSX_BLEND_APPLY
#endif

#ifdef __vita__
#define GPU_TEXTURE_SAMPLE_MAIN "		vec4 color = nearestTextureSample(v_texcoord.xy);\n"
#else
#define GPU_TEXTURE_SAMPLE_MAIN "		vec4 color = (bilinearFilter > 0) ? bilinearTextureSample(v_texcoord.xy) : nearestTextureSample(v_texcoord.xy);\n"
#endif

#define GPU_FRAGMENT_SAMPLE_SHADER(bit)                                                                                                             \
	GPU_FETCH_VRAM_FUNC                                                                                                                             \
	GPU_SAMPLE_TEXTURE_##bit##BIT_FUNC GPU_PSX_COLOR_UNIFORM                                                                                        \
	    "#ifndef VITA_NEAREST_ONLY\n\tuniform int bilinearFilter;\n#endif\n" GPU_SEMI_TRANS_UNIFORM "	uniform float psxDrawMaskSet;\n"              \
	    "	uniform float psxTextureOutputStp;\n"                                                                                                     \
	    "	float sampledStp = 0.0;\n" GPU_PSX_COLOR_DECODE GPU_STP_PASS_FUNC "#ifndef VITA_NEAREST_ONLY\n\tvec4 bilinearTextureSample(vec2 P) {\n" \
	    "		vec2 _frac = fract(P);\n"                                                                                                                \
	    "		vec2 pixel = floor(P);\n"                                                                                                                \
	    "		vec2 C11 = samplePSX(pixel);\n"                                                                                                          \
	    "		vec2 C21 = samplePSX(pixel + vec2(1.0, 0.0));\n"                                                                                         \
	    "		vec2 C12 = samplePSX(pixel + vec2(0.0, 1.0));\n"                                                                                         \
	    "		vec2 C22 = samplePSX(pixel + vec2(1.0, 1.0));\n"                                                                                         \
	    "		float v11 = texelVisible(C11);\n"                                                                                                        \
	    "		float v21 = texelVisible(C21);\n"                                                                                                        \
	    "		float v12 = texelVisible(C12);\n"                                                                                                        \
	    "		float v22 = texelVisible(C22);\n"                                                                                                        \
	    "		float s11 = v11 * stpWeight(C11);\n"                                                                                                     \
	    "		float s21 = v21 * stpWeight(C21);\n"                                                                                                     \
	    "		float s12 = v12 * stpWeight(C12);\n"                                                                                                     \
	    "		float s22 = v22 * stpWeight(C22);\n"                                                                                                     \
	    "		float n11 = v11 - s11;\n"                                                                                                                \
	    "		float n21 = v21 - s21;\n"                                                                                                                \
	    "		float n12 = v12 - s12;\n"                                                                                                                \
	    "		float n22 = v22 - s22;\n"                                                                                                                \
	    "		float ax1 = mix(v11, v21, _frac.x);\n"                                                                                                   \
	    "		float ax2 = mix(v12, v22, _frac.x);\n"                                                                                                   \
	    "		float axm = mix(ax1, ax2, _frac.y);\n"                                                                                                   \
	    "		float sx1 = mix(s11, s21, _frac.x);\n"                                                                                                   \
	    "		float sx2 = mix(s12, s22, _frac.x);\n"                                                                                                   \
	    "		float stp = mix(sx1, sx2, _frac.y);\n"                                                                                                   \
	    "		float nx1 = mix(n11, n21, _frac.x);\n"                                                                                                   \
	    "		float nx2 = mix(n12, n22, _frac.x);\n"                                                                                                   \
	    "		float nonStp = mix(nx1, nx2, _frac.y);\n"                                                                                                \
	    "		vec2 rg = mix(mix(C11, C21, _frac.x), mix(C12, C22, _frac.x), _frac.y);\n"                                                               \
	    "		float stpClass = step(nonStp, stp);\n"                                                                                                   \
	    "		sampledStp = stpClass;\n"                                                                                                                \
	    "		if(discardForSemiTransPass(axm, stpClass)) { discard; }\n"                                                                               \
	    "		vec4 x1 = mix(decodePSX(C11), decodePSX(C21), _frac.x);\n"                                                                               \
	    "		vec4 x2 = mix(decodePSX(C12), decodePSX(C22), _frac.x);\n"                                                                               \
	    "		vec4 t = mix(x1, x2, _frac.y);\n"                                                                                                        \
	    "		return t;\n"                                                                                                                             \
	    "	}\n#endif\n"                                                                                                                              \
	    "	vec4 nearestTextureSample(vec2 P) {\n"                                                                                                    \
	    "		vec2 rg = samplePSX(P);\n"                                                                                                               \
	    "#ifdef PSX_FULLY_OPAQUE\n"                                                                                                                 \
	    "		sampledStp = stpWeight(rg);\n"                                                                                                           \
	    "#else\n"                                                                                                                                   \
	    "		float visible = texelVisible(rg);\n"                                                                                                     \
	    "		sampledStp = visible * stpWeight(rg);\n"                                                                                                 \
	    "		if(discardForSemiTransPass(visible, sampledStp)) { discard; }\n"                                                                         \
	    "#endif\n"                                                                                                                                  \
	    "		vec4 t = decodePSX(rg);\n"                                                                                                               \
	    "		return t;\n"                                                                                                                             \
	    "	}\n"                                                                                                                                      \
	    "	void main() {\n" GPU_TEXTURE_SAMPLE_MAIN GPU_PSX_FRAGMENT_OUTPUT GPU_PSX_BLEND_APPLY "	}\n"

#ifdef __vita__
global_variable const char *gte_shader_cached_p4 =
    "\tuniform sampler2D s_texture;\n"
    "\tuniform float psxDrawMaskSet;\n"
    "\tuniform float psxTextureOutputStp;\n"
    "\tfloat sampledStp = 0.0;\n"
    "\tbool discardCachedP4(float visible, float stpClass) {\n"
    "\t\tif (visible < 0.5) { return true; }\n"
    "#ifdef PSX_PASS_NON_STP\n\t\tif (stpClass >= 0.5) { return true; }\n#endif\n"
    "#ifdef PSX_PASS_STP\n\t\tif (stpClass < 0.5) { return true; }\n#endif\n"
    "\t\treturn false;\n"
    "\t}\n"
    "\tvec4 nearestTextureSample(vec2 P) {\n"
    "\t\tvec2 texel = floor(P + vec2(0.5));\n"
    "\t\tvec4 t = texture2D(s_texture, (texel + vec2(0.5)) * (1.0 / 256.0));\n"
    "\t\tfloat visible = step(0.25, t.a);\n"
    "\t\tsampledStp = step(0.75, t.a);\n"
    "\t\tif (discardCachedP4(visible, sampledStp)) { discard; }\n"
    "\t\tt.a = 1.0;\n"
    "\t\treturn t;\n"
    "\t}\n"
    "\tvoid main() {\n"
    "\t\tvec4 color = nearestTextureSample(v_texcoord.xy);\n" GPU_PSX_FRAGMENT_OUTPUT GPU_PSX_BLEND_APPLY "\t}\n";
#endif

#if NATIVE_PGXP_SUPPORTED
// NOTE: PGXP polygons carry a real W, so the GPU interpolates texture
// coordinates perspective-correctly. Gouraud colour and the dither pattern stay
// screen-space like the PS1 rasteriser (DuckStation's default as well), and the
// per-polygon page/CLUT is flat so it never picks up interpolation error.
// Native 3D triangles shade in perspective instead: screen-linear interpolation
// is undefined across a clipped corner behind the camera, and drivers
// disagree (Mesa turns it black).
global_variable const char *gpu_shader_common = "	centroid varying vec4 v_texcoord;\n"
                                                "	PSX_NOPERSPECTIVE varying vec4 v_color;\n"
                                                "	varying vec4 v_colorPerspective;\n"
                                                "	flat varying float v_clipSpace;\n"
                                                "	flat varying vec4 v_page_clut;\n"
                                                "	varying vec3 v_ditherCoord;\n"
                                                "	varying float v_z;\n";
#else
global_variable const char *gpu_shader_common = "	centroid varying vec4 v_texcoord;\n"
                                                "	varying vec4 v_color;\n"
                                                "	varying vec4 v_page_clut;\n"
                                                "	varying vec2 v_ditherCoord;\n"
                                                "	varying float v_z;\n";
#endif

const char *gte_shader_4 = GPU_FRAGMENT_SAMPLE_SHADER(4);
const char *gte_shader_8 = GPU_FRAGMENT_SAMPLE_SHADER(8);
const char *gte_shader_16 = GPU_FRAGMENT_SAMPLE_SHADER(16);
#ifdef __vita__
const char *gte_shader_untextured = "\tuniform float psxDrawMaskSet;\n"
                                    "\tvoid main() {\n"
                                    "\t\tgl_FragColor.rgb = v_color.rgb * (248.0 / 255.0);\n"
                                    "\t\tgl_FragColor.a = psxDrawMaskSet;\n" GPU_PSX_BLEND_APPLY "\t}\n";

#define GPU_RGBA_FRAGMENT_OUTPUT                      \
	"		gl_FragColor.rgb = color.rgb * v_color.rgb;\n" \
	"		gl_FragColor.a = psxDrawMaskSet;\n"
#else
#define GPU_RGBA_FRAGMENT_OUTPUT                     \
	"		gl_FragColor = dither(color * psxShade());\n" \
	"		gl_FragColor.a = psxDrawMaskSet;\n"
#endif

const char *gte_shader_32_rgba = "	uniform sampler2D s_texture;\n" GPU_SEMI_TRANS_UNIFORM "	uniform float psxDrawMaskSet;\n"
                                 "	uniform vec2 texelSize;\n"
                                 "	void main() {\n"
                                 "		vec2 tc = v_texcoord.xy * texelSize + texelSize * 0.5;\n"
                                 "		vec4 color = texture2D(s_texture, tc);\n"
#ifndef __vita__
                                 // Pass 4 resolves native HUD coverage with edge UVs.
                                 "\t\tif (psxSemiTransPass == 4) {\n"
                                 "\t\t\tcolor = texture2D(s_texture, v_texcoord.xy * texelSize);\n"
                                 "\t\t\tgl_FragColor = vec4(color.rgb * psxShade().rgb, color.a);\n"
                                 "\t\t\treturn;\n"
                                 "\t\t}\n"
#endif
#ifdef __vita__
                                 "#if defined(PSX_PASS_NON_STP) || defined(PSX_PASS_STP)\n"
                                 "		if (color.a < 0.25) { discard; }\n"
                                 "		float sampledStp = step(0.75, color.a);\n"
                                 "#ifdef PSX_PASS_NON_STP\n		if (sampledStp >= 0.5) { discard; }\n#endif\n"
                                 "#ifdef PSX_PASS_STP\n		if (sampledStp < 0.5) { discard; }\n#endif\n"
                                 "#else\n"
                                 "		if (color.a < 0.5) { discard; }\n"
                                 "#endif\n"
#else
                                 "		if (psxSemiTransPass == 0) {\n"
                                 "			if (color.a < 0.5) { discard; }\n"
                                 "		} else {\n"
                                 "			if (color.a < 0.25) { discard; }\n"
                                 "			float sampledStp = step(0.75, color.a);\n"
                                 "			if (psxSemiTransPass == 1 && sampledStp >= 0.5) { discard; }\n"
                                 "			if (psxSemiTransPass == 2 && sampledStp < 0.5) { discard; }\n"
                                 "		}\n"
#endif
    GPU_RGBA_FRAGMENT_OUTPUT GPU_PSX_BLEND_APPLY "	}\n";

#ifndef __vita__
// Enhancements > Font (native_font.h). The distance field is resolved per
// screen pixel, so glyph edges stay sharp and anti-aliased at any resolution.
// Retail font texels are mid grey under a 2x shade, so half the shade gives
// the retail fill colour; the outline is black like the retail glyphs.
// Blending only writes colour, which keeps the PS1 mask bit in alpha intact.
const char *gte_shader_text_sdf = "	uniform sampler2D s_texture;\n"
                                  "	uniform vec2 texelSize;\n"
                                  "	uniform vec2 sdfEdges; // glyph edge, outline edge\n"
                                  "	void main() {\n"
                                  "		float dist = texture2D(s_texture, v_texcoord.xy * texelSize).r;\n"
                                  "		float aa = max(fwidth(dist) * 0.7, 1.0 / 255.0);\n"
                                  "		float shape = smoothstep(sdfEdges.y - aa, sdfEdges.y + aa, dist);\n"
                                  "		if (shape <= 0.0) { discard; }\n"
                                  "		float body = smoothstep(sdfEdges.x - aa, sdfEdges.x + aa, dist);\n"
                                  "		gl_FragColor = vec4(clamp(psxShade().rgb * 0.5, 0.0, 1.0) * body, shape);\n"
                                  "	}\n";
#endif

#ifdef __vita__
#define GTE_ORDER_DEPTH_ATTRIBUTE "\tattribute float a_orderDepth;\n"
#define GTE_PERSPECTIVE_CORRECTION                                  \
	"\tgl_Position = Projection * vec4(a_position.xy, 0.0, 1.0);\n" \
	"\tgl_Position.z = 1.0 - a_orderDepth * (2.0 / 65535.0);\n"     \
	"\tv_ditherCoord = a_position.xy;\n"
#else
#define GTE_ORDER_DEPTH_ATTRIBUTE ""
// NOTE: a_position.z is the PGXP view depth (0 = affine). Scaling the whole
// clip position by it leaves the screen position unchanged after the divide
// but makes the rasteriser interpolate texture coordinates in perspective.
// World depth uses an infinite-far reciprocal projection with near=32, below
// CTR's normal clip distance. Affine UVs still use W=1; their depth interpolates
// 1/Z in screen space, just as it does after the perspective divide.
//
// Native vertices (a_extra.z set, see native_draw3d.h) are already
// homogeneous: xy are screen coordinates times clip w, z is the clip depth and
// w the camera depth. The host clips them at the near plane.
// Native 2D uses the same input with clip w=1 and z=0, without depth testing.
#define GTE_PERSPECTIVE_CORRECTION                                                    \
	"\tif (a_extra.z > 0.5) {\n"                                                      \
	"\t\tgl_Position = Projection * vec4(a_position.xy, 0.0, a_position.w);\n"        \
	"\t\tgl_Position.z = a_position.z;\n"                                             \
	"\t\tv_ditherCoord = a_position.xyw;\n"                                           \
	"\t} else {\n"                                                                    \
	"\t\tgl_Position = Projection * vec4(a_position.xy, 0.0, 1.0);\n"                 \
	"\t\tgl_Position *= (a_position.z > 0.0) ? a_position.z : 1.0;\n"                 \
	"\t\tif (a_position.w > 0.0) {\n"                                                 \
	"\t\t\tgl_Position.z = (1.0 - 64.0 / max(a_position.w, 32.0)) * gl_Position.w;\n" \
	"\t\t}\n"                                                                         \
	"\t\tv_ditherCoord = vec3(a_position.xy, 1.0) * gl_Position.w;\n"                 \
	"\t}\n"                                                                           \
	"\tv_colorPerspective = vec4(a_color.xyz * a_texcoord.z, a_color.w);\n"           \
	"\tv_clipSpace = a_extra.z;\n"
#endif

#if NATIVE_PGXP_SUPPORTED
#define GTE_POSITION_ATTRIBUTES                                  \
	"	attribute vec4 a_position; // x, y, PGXP w, world depth\n" \
	"	attribute vec2 a_page_clut;\n"
#define GTE_PAGE_ATTRIBUTE "a_page_clut.x"
#define GTE_CLUT_ATTRIBUTE "a_page_clut.y"
#else
#define GTE_POSITION_ATTRIBUTES "	attribute vec4 a_position;\n"
#define GTE_PAGE_ATTRIBUTE      "a_position.z"
#define GTE_CLUT_ATTRIBUTE      "a_position.w"
#endif

#ifdef __vita__
#define GTE_PAGE_CLUT_SETUP                                      \
	"\t\tv_page_clut.x = fract(a_position.z / 16.0) * 1024.0;\n" \
	"\t\tv_page_clut.y = floor(a_position.z / 16.0) * 256.0;\n"  \
	"\t\tv_page_clut.z = fract(a_position.w / 64.0) * 1024.0;\n" \
	"\t\tv_page_clut.w = floor(a_position.w / 64.0);\n"
#else
#define GTE_PAGE_CLUT_SETUP                                                \
	"\t\tv_page_clut.x = fract(" GTE_PAGE_ATTRIBUTE " / 16.0) * 1024.0;\n" \
	"\t\tv_page_clut.y = floor(" GTE_PAGE_ATTRIBUTE " / 16.0) * 256.0;\n"  \
	"\t\tv_page_clut.z = fract(" GTE_CLUT_ATTRIBUTE " / 64.0);\n"          \
	"\t\tv_page_clut.w = floor(" GTE_CLUT_ATTRIBUTE " / 64.0) / 512.0;\n"  \
	"\t\tv_page_clut.xy += c_UVFudge;\n"                                   \
	"\t\tv_page_clut.zw += c_UVFudge;\n"
#endif

#define GTE_VERTEX_SHADER                                                                                                                     \
	GTE_POSITION_ATTRIBUTES                                                                                                                   \
	"	attribute vec4 a_texcoord; // uv, color multiplier, dither\n"                                                                           \
	"	attribute vec4 a_color;\n"                                                                                                              \
	"	attribute vec4 a_extra; // texcoord.xy ofs, native clip-space flag, unused\n" GTE_ORDER_DEPTH_ATTRIBUTE "	uniform mat4 Projection;\n" \
	"	const vec2 c_UVFudge = vec2(0.00025, 0.00025);\n"                                                                                       \
	"	void main() {\n"                                                                                                                        \
	"		v_texcoord = a_texcoord;\n"                                                                                                            \
	"		v_texcoord.xy += a_extra.xy * 0.5;\n"                                                                                                  \
	"		v_color = a_color;\n"                                                                                                                  \
	"		v_color.xyz *= a_texcoord.z;\n" GTE_PAGE_CLUT_SETUP GTE_PERSPECTIVE_CORRECTION "		v_z = (gl_Position.z - 40.0) * 0.005;\n"        \
	"	}\n"

internal int NativeRenderer_Shader_CheckShaderStatus(GLuint shader)
{
	char info[1024];
	GLint result;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

	if (result == GL_TRUE)
	{
		return 1;
	}

	glGetShaderInfoLog(shader, sizeof(info), NULL, info);
	if (info[0] && strlen(info) > 8)
	{
		NATIVE_RENDERER_ERROR("%s\n", info);
		assert(0);
	}

	return 0;
}

internal int NativeRenderer_Shader_CheckProgramStatus(GLuint program)
{
	char info[1024];
	GLint result;

	glGetProgramiv(program, GL_LINK_STATUS, &result);

	if (result == GL_TRUE)
	{
		return 1;
	}

	glGetProgramInfoLog(program, sizeof(info), NULL, info);
	if (info[0] && strlen(info) > 8)
	{
		NATIVE_RENDERER_ERROR("%s\n", info);
		assert(0);
	}

	return 0;
}

// GLSL ES has no noperspective qualifier; there everything interpolates in
// perspective, which only differs from desktop for PGXP Gouraud shading.
#ifdef __EMSCRIPTEN__
#define GLSL_NOPERSPECTIVE_DEFINE "	#define PSX_NOPERSPECTIVE\n"
#else
#define GLSL_NOPERSPECTIVE_DEFINE "	#define PSX_NOPERSPECTIVE noperspective\n"
#endif

internal ShaderID NativeRenderer_Shader_Compile(const char *source, bool isPsxShader, const char *fragmentDefines)
{
	const char *GLSL_HEADER_VERT =
#ifdef __EMSCRIPTEN__
	    "	#version 300 es\n"
#else
	    "	#version 140\n"
#endif
	    "	precision lowp  int;\n"
	    "	precision highp float;\n"
#ifndef __vita__
	    "	#define varying   out\n"
	    "	#define attribute in\n"
	    "	#define texture2D texture\n" GLSL_NOPERSPECTIVE_DEFINE
#endif
	    ;

	const char *GLSL_HEADER_FRAG =
#ifdef __EMSCRIPTEN__
	    "	#version 300 es\n"
#else
	    "	#version 140\n"
#endif
	    "	precision lowp  int;\n"
	    "	precision highp float;\n"
#ifndef __vita__
	    "	#define varying     in\n"
	    "	#define texture2D   texture\n" GLSL_NOPERSPECTIVE_DEFINE "	out vec4 fragColor;\n"
#ifdef __EMSCRIPTEN__
	    "\t#define gl_FragColor fragColor\n"
#endif
#endif
	    ;

	char extra_vs_defines[1024];
	char extra_fs_defines[1024];
	extra_vs_defines[0] = 0;
	extra_fs_defines[0] = 0;

	strcat(extra_vs_defines, "#define VERTEX\n");
	strcat(extra_fs_defines, "#define FRAGMENT\n");
	if (fragmentDefines != NULL)
	{
		strcat(extra_fs_defines, fragmentDefines);
	}
#ifdef __vita__
	strcat(extra_fs_defines, "#define VITA_NEAREST_ONLY\n");
#endif
	if (g_cfg_bilinearFiltering)
	{
		strcat(extra_fs_defines, "#define BILINEAR_FILTER\n");
	}

	const char *vs_list_psx[] = {GLSL_HEADER_VERT, extra_vs_defines, gpu_shader_common, GTE_VERTEX_SHADER};
	const char *fs_list_psx[] = {GLSL_HEADER_FRAG, extra_fs_defines, gpu_shader_common, GPU_DITHERING, source};
	const char *vs_list_src[] = {
	    GLSL_HEADER_VERT,
	    extra_vs_defines,
	    source,
	};
	const char *fs_list_src[] = {GLSL_HEADER_FRAG, extra_fs_defines, source};

	const char **vs_list = isPsxShader ? vs_list_psx : vs_list_src;
	const char **fs_list = isPsxShader ? fs_list_psx : fs_list_src;
	const int vs_list_cnt = isPsxShader ? 4 : 3;
	const int fs_list_cnt = isPsxShader ? 5 : 3;

	GLuint program = glCreateProgram();

	{
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, vs_list_cnt, vs_list, NULL);
		glCompileShader(vertexShader);

		if (NativeRenderer_Shader_CheckShaderStatus(vertexShader) == 0)
		{
			NATIVE_RENDERER_ERROR("%s\n", "Failed to compile Vertex Shader!");
		}

		glAttachShader(program, vertexShader);
		glDeleteShader(vertexShader);
	}

	{
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, fs_list_cnt, fs_list, NULL);
		glCompileShader(fragmentShader);

		if (NativeRenderer_Shader_CheckShaderStatus(fragmentShader) == 0)
		{
			NATIVE_RENDERER_ERROR("%s\n", "Failed to compile Fragment Shader!");
		}

		glAttachShader(program, fragmentShader);
		glDeleteShader(fragmentShader);
	}

	glBindAttribLocation(program, a_position, "a_position");
	glBindAttribLocation(program, a_texcoord, "a_texcoord");
	glBindAttribLocation(program, a_color, "a_color");
	glBindAttribLocation(program, a_extra, "a_extra");
#ifdef __vita__
	glBindAttribLocation(program, a_order_depth, "a_orderDepth");
#endif
#if NATIVE_PGXP_SUPPORTED
	glBindAttribLocation(program, a_page_clut, "a_page_clut");
#endif

	glLinkProgram(program);
	if (NativeRenderer_Shader_CheckProgramStatus(program) == 0)
	{
		NATIVE_RENDERER_ERROR("%s\n", "Failed to link Shader!");
	}

	GLint textureSampler = 0;
	GLint presentLutSampler = 2;
	glUseProgram(program);
	glUniform1iv(glGetUniformLocation(program, "s_texture"), 1, &textureSampler);
#ifndef __vita__
	GLint lutSampler = 1;
	glUniform1iv(glGetUniformLocation(program, "s_rgLut"), 1, &lutSampler);
#endif
	glUniform1iv(glGetUniformLocation(program, "s_presentLut"), 1, &presentLutSampler);
	glUseProgram(0);

	return program;
}

internal void NativeRenderer_CompilePSXShader(GTEShader *sh, const char *source, const char *fragmentDefines)
{
	sh->shader = NativeRenderer_Shader_Compile(source, true, fragmentDefines);

	sh->bilinearFilterLoc = glGetUniformLocation(sh->shader, "bilinearFilter");
	sh->projectionLoc = glGetUniformLocation(sh->shader, "Projection");
	sh->texelSizeLoc = glGetUniformLocation(sh->shader, "texelSize");
	sh->texLoc = glGetUniformLocation(sh->shader, "s_texture");
	sh->lutLoc = glGetUniformLocation(sh->shader, "s_rgLut");
#ifndef __vita__
	sh->psxSemiTransPassLoc = glGetUniformLocation(sh->shader, "psxSemiTransPass");
	sh->psxDitherEnabledLoc = glGetUniformLocation(sh->shader, "psxDitherEnabled");
	sh->psxColorDepth15Loc = glGetUniformLocation(sh->shader, "psxColorDepth15");
#endif
	sh->psxDrawMaskSetLoc = glGetUniformLocation(sh->shader, "psxDrawMaskSet");
	sh->psxTextureOutputStpLoc = glGetUniformLocation(sh->shader, "psxTextureOutputStp");
}

internal void NativeRenderer_InitialisePSXShaders(void)
{
#ifdef __vita__
	local_persist const char *variantDefines[NATIVE_PSX_SHADER_VARIANT_COUNT] = {
	    "",
	    "#define PSX_BLEND_AVERAGE\n",
	    "#define PSX_BLEND_QUARTER\n",
	    "#define PSX_PASS_NON_STP\n",
	    "#define PSX_PASS_STP\n",
	    "#define PSX_PASS_STP\n#define PSX_BLEND_AVERAGE\n",
	    "#define PSX_PASS_STP\n#define PSX_BLEND_QUARTER\n",
	    "#define PSX_MIXED_AVERAGE\n",
	    "#define PSX_MIXED_ADD\n",
	    "#define PSX_MIXED_QUARTER\n",
	};
	local_persist const char *fullyOpaqueVariantDefines[3] = {
	    "#define PSX_FULLY_OPAQUE\n",
	    "#define PSX_FULLY_OPAQUE\n#define PSX_BLEND_AVERAGE\n",
	    "#define PSX_FULLY_OPAQUE\n#define PSX_BLEND_QUARTER\n",
	};
	const char *sources[4] = {gte_shader_4, gte_shader_8, gte_shader_16, gte_shader_32_rgba};

	for (int format = TF_4_BIT; format <= TF_32_BIT_RGBA; format++)
	{
		const int variantCount = format == TF_32_BIT_RGBA ? NATIVE_PSX_SHADER_STP_QUARTER + 1 : NATIVE_PSX_SHADER_VARIANT_COUNT;
		for (int variant = 0; variant < variantCount; variant++)
		{
			NativeRenderer_CompilePSXShader(&s_gteShaderVariants[format][variant], sources[format], variantDefines[variant]);
		}
	}
	for (int variant = 0; variant < NATIVE_PSX_SHADER_VARIANT_COUNT; variant++)
	{
		NativeRenderer_CompilePSXShader(&s_gteCachedP4ShaderVariants[variant], gte_shader_cached_p4, variantDefines[variant]);
	}
	for (int format = TF_4_BIT; format <= TF_8_BIT; format++)
	{
		for (int variant = 0; variant < 3; variant++)
		{
			NativeRenderer_CompilePSXShader(&s_gteFullyOpaqueShaderVariants[format][variant], sources[format], fullyOpaqueVariantDefines[variant]);
		}
	}
	for (int variant = 0; variant < 3; variant++)
	{
		NativeRenderer_CompilePSXShader(&s_gteUntexturedShaderVariants[variant], gte_shader_untextured, variantDefines[variant]);
	}
#else
	NativeRenderer_CompilePSXShader(&s_gteShader4, gte_shader_4, NULL);
	NativeRenderer_CompilePSXShader(&s_gteShader8, gte_shader_8, NULL);
	NativeRenderer_CompilePSXShader(&s_gteShader16, gte_shader_16, NULL);
	NativeRenderer_CompilePSXShader(&s_gteShader4SuperTurbo, gte_shader_4, "#define SUPER_TURBO_TINT\n");
	NativeRenderer_CompilePSXShader(&s_gteShader8SuperTurbo, gte_shader_8, "#define SUPER_TURBO_TINT\n");
	NativeRenderer_CompilePSXShader(&s_gteShader16SuperTurbo, gte_shader_16, "#define SUPER_TURBO_TINT\n");
	NativeRenderer_CompilePSXShader(&s_gteShader32Rgba, gte_shader_32_rgba, NULL);
	NativeRenderer_CompilePSXShader(&s_gteShaderTextSdf, gte_shader_text_sdf, NULL);
	glUseProgram(s_gteShaderTextSdf.shader);
	glUniform2f(glGetUniformLocation(s_gteShaderTextSdf.shader, "sdfEdges"), NATIVE_FONT_SDF_EDGE, NATIVE_FONT_SDF_OUTLINE);
	glUseProgram(0);
#endif
}

#ifdef __vita__
global_variable const char *ctr_pack_shader = "#ifdef VERTEX\n"
                                              "attribute vec2 a_position;\n"
                                              "varying vec2 v_uv;\n"
                                              "uniform int flipY;\n"
                                              "void main() {\n"
                                              "    v_uv = a_position * 0.5 + 0.5;\n"
                                              "    if (flipY != 0) { v_uv.y = 1.0 - v_uv.y; }\n"
                                              "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                              "}\n"
                                              "#endif\n"
                                              "#ifdef FRAGMENT\n"
                                              "varying vec2 v_uv;\n"
                                              "uniform sampler2D s_src;\n"
                                              "void main() {\n"
                                              "    vec4 c = texture2D(s_src, v_uv);\n"
                                              "    vec3 color5 = floor(c.rgb * (255.0 / 8.0) + 0.001);\n"
                                              "    float lowByte = color5.r + mod(color5.g, 8.0) * 32.0;\n"
                                              "    float highByte = floor(color5.g / 8.0) + color5.b * 4.0 + step(0.5, c.a) * 128.0;\n"
                                              "    gl_FragColor = vec4(lowByte, highByte, 0.0, 0.0) / 255.0;\n"
                                              "}\n"
                                              "#endif\n";

global_variable const char *ctr_present_vram_shader = "#ifdef VERTEX\n"
                                                      "attribute vec2 a_position;\n"
                                                      "varying vec2 v_uv;\n"
                                                      "uniform vec4 sourceRect;\n"
                                                      "void main() {\n"
                                                      "    vec2 screenUV = a_position * 0.5 + 0.5;\n"
                                                      "    vec2 sourcePixel = sourceRect.xy + vec2(screenUV.x, 1.0 - screenUV.y) * sourceRect.zw;\n"
                                                      "    v_uv = sourcePixel / vec2(1024.0, 512.0);\n"
                                                      "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                      "}\n"
                                                      "#endif\n"
                                                      "#ifdef FRAGMENT\n"
                                                      "varying vec2 v_uv;\n"
                                                      "uniform sampler2D s_texture;\n"
                                                      "uniform sampler2D s_presentLut;\n"
                                                      "const vec2 c_LUTTexel = vec2(1.0 / 256.0, 1.0 / 256.0);\n"
                                                      "void main() {\n"
                                                      "    vec2 packedRg = texture2D(s_texture, v_uv).rg;\n"
                                                      "    gl_FragColor = texture2D(s_presentLut, packedRg - c_LUTTexel * 0.0001);\n"
                                                      "}\n"
                                                      "#endif\n";
#else
// NOTE(aalhendi): GPU VRAM pack. Samples an RGBA render texture and writes PS1
// 5:5:5:1 pixels into RG8 VRAM, low byte in R and high byte in G. NEAREST
// sampling and integer channel shifts preserve the packed PS1 pixel value.
global_variable const char *ctr_pack_shader = "#ifdef VERTEX\n"
                                              "attribute vec2 a_position;\n"
                                              "varying vec2 v_uv;\n"
                                              "uniform int flipY;\n"
                                              "void main() {\n"
                                              "	v_uv = a_position * 0.5 + 0.5;\n"
                                              "	if (flipY != 0) { v_uv.y = 1.0 - v_uv.y; }\n"
                                              "	gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                              "}\n"
                                              "#endif\n"
                                              "#ifdef FRAGMENT\n"
                                              "varying vec2 v_uv;\n"
                                              "uniform sampler2D s_src;\n"
                                              "void main() {\n"
                                              "	ivec4 c = ivec4(texture2D(s_src, v_uv) * 255.0 + 0.5);\n"
                                              "	int px16 = (c.r >> 3) | ((c.g >> 3) << 5) | ((c.b >> 3) << 10) | ((c.a >> 7) << 15);\n"
                                              "	gl_FragColor = vec4(float(px16 & 0xFF) / 255.0, float((px16 >> 8) & 0xFF) / 255.0, 0.0, 0.0);\n"
                                              "}\n"
                                              "#endif\n";

// NOTE(aalhendi): Expand packed VRAM without losing bit 15. Internal render
// targets carry that PS1 STP/mask bit in alpha so packing them is lossless.
global_variable const char *ctr_present_vram_shader = "#ifdef VERTEX\n"
                                                      "attribute vec2 a_position;\n"
                                                      "varying vec2 v_uv;\n"
                                                      "uniform vec4 sourceRect;\n"
                                                      "void main() {\n"
                                                      "\tvec2 screenUV = a_position * 0.5 + 0.5;\n"
                                                      "\tvec2 sourcePixel = sourceRect.xy + vec2(screenUV.x, 1.0 - screenUV.y) * sourceRect.zw;\n"
                                                      "\tv_uv = sourcePixel / vec2(1024.0, 512.0);\n"
                                                      "\tgl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                      "}\n"
                                                      "#endif\n"
                                                      "#ifdef FRAGMENT\n"
                                                      "varying vec2 v_uv;\n"
                                                      "uniform sampler2D s_texture;\n"
                                                      "void main() {\n"
                                                      "\tivec2 packedBytes = ivec2(texture2D(s_texture, v_uv).rg * 255.0 + 0.5);\n"
                                                      "\tint pixel = packedBytes.r | (packedBytes.g << 8);\n"
                                                      "\tivec3 color5 = ivec3(pixel & 31, (pixel >> 5) & 31, (pixel >> 10) & 31);\n"
                                                      "\tivec3 color8 = (color5 << 3) | (color5 >> 2);\n"
                                                      "\tfloat stp = float((pixel >> 15) & 1);\n"
                                                      "\tgl_FragColor = vec4(vec3(color8) / 255.0, stp);\n"
                                                      "}\n"
                                                      "#endif\n";

// Pause backdrop. Reduces the presented frame to the retail pause palette: the
// posterised mode picks the same entry as retail (top 4 bits of the 8-bit
// green), the smooth mode blends neighbouring entries. Alpha stays opaque.
global_variable const char *ctr_pause_bg_shader = "#ifdef VERTEX\n"
                                                  "attribute vec2 a_position;\n"
                                                  "varying vec2 v_uv;\n"
                                                  "uniform int flipY;\n"
                                                  "void main() {\n"
                                                  "\tv_uv = a_position * 0.5 + 0.5;\n"
                                                  "\tif (flipY != 0) { v_uv.y = 1.0 - v_uv.y; }\n"
                                                  "\tgl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                  "}\n"
                                                  "#endif\n"
                                                  "#ifdef FRAGMENT\n"
                                                  "varying vec2 v_uv;\n"
                                                  "uniform sampler2D s_src;\n"
                                                  "uniform vec3 palette[16];\n"
                                                  "uniform int smoothMode;\n"
                                                  "void main() {\n"
                                                  "\tfloat g = clamp(texture2D(s_src, v_uv).g, 0.0, 1.0);\n"
                                                  "\tvec3 rgb;\n"
                                                  "\tif (smoothMode != 0) {\n"
                                                  "\t\tfloat f = g * 15.0;\n"
                                                  "\t\tfloat base = floor(f);\n"
                                                  "\t\tint lo = int(base);\n"
                                                  "\t\tint hi = min(lo + 1, 15);\n"
                                                  "\t\trgb = mix(palette[lo], palette[hi], f - base);\n"
                                                  "\t} else {\n"
                                                  "\t\trgb = palette[int(floor(floor(g * 255.0 + 0.5) / 16.0))];\n"
                                                  "\t}\n"
                                                  "\tgl_FragColor = vec4(rgb, 1.0);\n"
                                                  "}\n"
                                                  "#endif\n";
#endif

#ifdef __vita__
global_variable const char *ctr_present_rgba_shader = "#ifdef VERTEX\n"
                                                      "attribute vec2 a_position;\n"
                                                      "varying vec2 v_uv;\n"
                                                      "void main() {\n"
                                                      "    v_uv = a_position * 0.5 + 0.5;\n"
                                                      "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                      "}\n"
                                                      "#endif\n"
                                                      "#ifdef FRAGMENT\n"
                                                      "varying vec2 v_uv;\n"
                                                      "uniform sampler2D s_src;\n"
                                                      "uniform float flipY;\n"
                                                      "void main() {\n"
                                                      "    gl_FragColor = texture2D(s_src, vec2(v_uv.x, mix(v_uv.y, 1.0 - v_uv.y, flipY)));\n"
                                                      "}\n"
                                                      "#endif\n";
#else
global_variable const char *ctr_present_rgba_shader =
    "#ifdef VERTEX\n"
    "attribute vec2 a_position;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_position * 0.5 + 0.5;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n"
    "#endif\n"
    "#ifdef FRAGMENT\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D s_src;\n"
    "uniform float flipY;\n"
    "uniform vec2 texelSize;\n"
    "uniform int fxaaEnabled;\n"
    "float fxaaLuma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "vec4 fxaaSample(vec2 uv) {\n"
    "    vec4 center = texture2D(s_src, uv);\n"
    "    vec3 nw = texture2D(s_src, uv + texelSize * vec2(-1.0, -1.0)).rgb;\n"
    "    vec3 ne = texture2D(s_src, uv + texelSize * vec2( 1.0, -1.0)).rgb;\n"
    "    vec3 sw = texture2D(s_src, uv + texelSize * vec2(-1.0,  1.0)).rgb;\n"
    "    vec3 se = texture2D(s_src, uv + texelSize * vec2( 1.0,  1.0)).rgb;\n"
    "    float lNW = fxaaLuma(nw), lNE = fxaaLuma(ne), lSW = fxaaLuma(sw), lSE = fxaaLuma(se), lM = fxaaLuma(center.rgb);\n"
    "    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));\n"
    "    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));\n"
    "    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));\n"
    "    float reduce = max((lNW + lNE + lSW + lSE) * 0.0078125, 0.0009765625);\n"
    "    float invMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);\n"
    "    dir = clamp(dir * invMin, vec2(-8.0), vec2(8.0)) * texelSize;\n"
    "    vec3 a = 0.5 * (texture2D(s_src, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture2D(s_src, uv + dir * (2.0 / 3.0 - 0.5)).rgb);\n"
    "    vec3 b = a * 0.5 + 0.25 * (texture2D(s_src, uv + dir * -0.5).rgb + texture2D(s_src, uv + dir * 0.5).rgb);\n"
    "    float lB = fxaaLuma(b);\n"
    "    return vec4((lB < lMin || lB > lMax) ? a : b, center.a);\n"
    "}\n"
    "void main() {\n"
    "    vec2 uv = vec2(v_uv.x, mix(v_uv.y, 1.0 - v_uv.y, flipY));\n"
    "    gl_FragColor = (fxaaEnabled != 0) ? fxaaSample(uv) : texture2D(s_src, uv);\n"
    "}\n"
    "#endif\n";

#ifndef __vita__
// Inverse-map a completed world image after rasterization. The whole world pass
// shares one target and depth buffer; only its final colour resolve is warped.
global_variable const char *ctr_projected_world_shader =
    "#ifdef VERTEX\n"
    "attribute vec2 a_position;\n"
    "varying vec2 v_uv;\n"
    "void main() { v_uv = a_position * 0.5 + 0.5; gl_Position = vec4(a_position, 0.0, 1.0); }\n"
    "#endif\n"
    "#ifdef FRAGMENT\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D s_src;\n"
    "uniform vec4 sourceRect;\n"
    "uniform vec2 sourceTexelSize;\n"
    "uniform int projectionMode;\n"
    "uniform float projectionStrength;\n"
    "uniform float horizontalTanHalf;\n"
    "uniform float sourceOverscan;\n"
    "uniform float paniniEndpoint;\n"
    "float edgeSourceX(float outputX) {\n"
    "    float k = projectionStrength;\n"
    "    float a = abs(clamp(outputX, -1.0, 1.0));\n"
    "    float c = 0.35;\n"
    "    float warped = (a <= c || k <= 0.0) ? a : c + (exp(k * (a - c)) - 1.0) / k;\n"
    "    return sign(outputX) * warped / sourceOverscan;\n"
    "}\n"
    "vec2 paniniSource(vec2 outputNdc) {\n"
    "    float d = projectionStrength;\n"
    "    float p = clamp(outputNdc.x, -1.0, 1.0) * paniniEndpoint;\n"
    "    float a = d + 1.0;\n"
    "    float denominator = a * a - p * p * d * d;\n"
    "    float radicand = max(0.0, a * a + (1.0 - d * d) * p * p);\n"
    "    float rayX = p * (a + d * sqrt(radicand)) / max(denominator, 0.000001);\n"
    "    float factor = a / (1.0 + d * sqrt(1.0 + rayX * rayX));\n"
    "    float endpointFactor = paniniEndpoint / horizontalTanHalf;\n"
    "    return vec2(rayX / horizontalTanHalf, outputNdc.y * endpointFactor / factor);\n"
    "}\n"
    "void main() {\n"
    "    vec2 outputNdc = v_uv * 2.0 - 1.0;\n"
    "    vec2 sourceNdc = outputNdc;\n"
    "    if (projectionMode == 1 && projectionStrength > 0.0) sourceNdc = paniniSource(outputNdc);\n"
    "    else if (projectionMode == 2 && projectionStrength > 0.0) sourceNdc.x = edgeSourceX(outputNdc.x);\n"
    "    vec2 sourceUv = sourceRect.xy + (sourceNdc * 0.5 + 0.5) * sourceRect.zw;\n"
    "    sourceUv = clamp(sourceUv, sourceRect.xy + sourceTexelSize * 0.5, sourceRect.xy + sourceRect.zw - sourceTexelSize * 0.5);\n"
    "    gl_FragColor = texture2D(s_src, sourceUv);\n"
    "}\n"
    "#endif\n";
#endif

// SSAA resolve: an area-weighted box filter. Each destination pixel averages
// the source texels its footprint covers, weighted by overlap, so non-integer
// scales (SSAA 2X) resolve without the bias of a single bilinear tap.
global_variable const char *ctr_downsample_shader = "#ifdef VERTEX\n"
                                                    "attribute vec2 a_position;\n"
                                                    "varying vec2 v_uv;\n"
                                                    "void main() {\n"
                                                    "    v_uv = a_position * 0.5 + 0.5;\n"
                                                    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                    "}\n"
                                                    "#endif\n"
                                                    "#ifdef FRAGMENT\n"
                                                    "varying vec2 v_uv;\n"
                                                    "uniform sampler2D s_src;\n"
                                                    "uniform vec2 srcSize;\n"
                                                    "uniform vec2 dstSize;\n"
                                                    "void main() {\n"
                                                    "    vec2 scale = srcSize / dstSize;\n"
                                                    "    vec2 lo = floor(v_uv * dstSize) * scale;\n"
                                                    "    vec2 hi = lo + scale;\n"
                                                    "    highp ivec2 first = ivec2(floor(lo));\n"
                                                    "    highp ivec2 last = ivec2(srcSize) - 1;\n"
                                                    "    vec4 sum = vec4(0.0);\n"
                                                    "    float total = 0.0;\n"
                                                    "    for (int y = 0; y < 4; y++) {\n"
                                                    "        float y0 = float(first.y + y);\n"
                                                    "        float wy = min(hi.y, y0 + 1.0) - max(lo.y, y0);\n"
                                                    "        if (wy <= 0.0) continue;\n"
                                                    "        for (int x = 0; x < 4; x++) {\n"
                                                    "            float x0 = float(first.x + x);\n"
                                                    "            float wx = min(hi.x, x0 + 1.0) - max(lo.x, x0);\n"
                                                    "            if (wx <= 0.0) continue;\n"
                                                    "            highp ivec2 tc = clamp(first + ivec2(x, y), ivec2(0), last);\n"
                                                    "            sum += texelFetch(s_src, tc, 0) * (wx * wy);\n"
                                                    "            total += wx * wy;\n"
                                                    "        }\n"
                                                    "    }\n"
                                                    "    gl_FragColor = sum / max(total, 1e-6);\n"
                                                    "}\n"
                                                    "#endif\n";
#endif

internal void NativeRenderer_InitVRAMPipelines(void)
{
	local_persist const float quad[12] = {-1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f};

	s_packShader = NativeRenderer_Shader_Compile(ctr_pack_shader, false, NULL);
	glUseProgram(s_packShader);
	const GLint packSrcLoc = glGetUniformLocation(s_packShader, "s_src");
	s_packFlipYLoc = glGetUniformLocation(s_packShader, "flipY");
	glUniform1i(packSrcLoc, 0);
	glUseProgram(0);

#ifndef __vita__
	s_pauseBackgroundShader = NativeRenderer_Shader_Compile(ctr_pause_bg_shader, false, NULL);
	glUseProgram(s_pauseBackgroundShader);
	glUniform1i(glGetUniformLocation(s_pauseBackgroundShader, "s_src"), 0);
	s_pauseBackgroundFlipYLoc = glGetUniformLocation(s_pauseBackgroundShader, "flipY");
	s_pauseBackgroundPaletteLoc = glGetUniformLocation(s_pauseBackgroundShader, "palette");
	s_pauseBackgroundSmoothLoc = glGetUniformLocation(s_pauseBackgroundShader, "smoothMode");
	glUseProgram(0);

	s_downsampleShader = NativeRenderer_Shader_Compile(ctr_downsample_shader, false, NULL);
	glUseProgram(s_downsampleShader);
	glUniform1i(glGetUniformLocation(s_downsampleShader, "s_src"), 0);
	s_downsampleSrcSizeLoc = glGetUniformLocation(s_downsampleShader, "srcSize");
	s_downsampleDstSizeLoc = glGetUniformLocation(s_downsampleShader, "dstSize");
	glUseProgram(0);
#endif

	s_presentVramShader = NativeRenderer_Shader_Compile(ctr_present_vram_shader, false, NULL);
	s_presentVramSourceRectLoc = glGetUniformLocation(s_presentVramShader, "sourceRect");

	s_presentRgbaShader = NativeRenderer_Shader_Compile(ctr_present_rgba_shader, false, NULL);
	glUseProgram(s_presentRgbaShader);
	const GLint presentRgbaSrcLoc = glGetUniformLocation(s_presentRgbaShader, "s_src");
	s_presentRgbaFlipYLoc = glGetUniformLocation(s_presentRgbaShader, "flipY");
#ifndef __vita__
	s_presentRgbaTexelSizeLoc = glGetUniformLocation(s_presentRgbaShader, "texelSize");
	s_presentRgbaFxaaLoc = glGetUniformLocation(s_presentRgbaShader, "fxaaEnabled");
	glUniform1i(s_presentRgbaFxaaLoc, 0);
	glUniform2f(s_presentRgbaTexelSizeLoc, 1.0f, 1.0f);
	s_projectedWorldShader = NativeRenderer_Shader_Compile(ctr_projected_world_shader, false, NULL);
	s_projectedWorldSourceLoc = glGetUniformLocation(s_projectedWorldShader, "s_src");
	s_projectedWorldRectLoc = glGetUniformLocation(s_projectedWorldShader, "sourceRect");
	s_projectedWorldTexelLoc = glGetUniformLocation(s_projectedWorldShader, "sourceTexelSize");
	s_projectedWorldModeLoc = glGetUniformLocation(s_projectedWorldShader, "projectionMode");
	s_projectedWorldStrengthLoc = glGetUniformLocation(s_projectedWorldShader, "projectionStrength");
	s_projectedWorldTanHalfLoc = glGetUniformLocation(s_projectedWorldShader, "horizontalTanHalf");
	s_projectedWorldOverscanLoc = glGetUniformLocation(s_projectedWorldShader, "sourceOverscan");
	s_projectedWorldEndpointLoc = glGetUniformLocation(s_projectedWorldShader, "paniniEndpoint");
	glUseProgram(s_projectedWorldShader);
	glUniform1i(s_projectedWorldSourceLoc, 0);
	glUseProgram(s_presentRgbaShader);
#endif
	glUniform1i(presentRgbaSrcLoc, 0);
	glUniform1f(s_presentRgbaFlipYLoc, 0.0f);
	glUseProgram(0);

	glGenVertexArrays(1, &s_vramQuadVAO);
	glGenBuffers(1, &s_vramQuadVBO);
	glBindVertexArray(s_vramQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, s_vramQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(a_position);
	glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

internal void NativeRenderer_InitRG8LUT(void)
{
	for (u16 y = 0; y < LUT_HEIGHT; y++)
	{
		u8 *row = rgLUT + y * (LUT_WIDTH * 4);
		for (u16 x = 0; x < LUT_WIDTH; x++)
		{
			const u16 c = (y << 8) | x;
			u8 *pixel = row + x * 4;
			pixel[0] = (u8)((c & 31) << 3);
			pixel[1] = (u8)(((c >> 5) & 31) << 3);
			pixel[2] = (u8)(((c >> 10) & 31) << 3);
			pixel[3] = (u8)(((c >> 15) & 1) << 7);
		}
	}
}
