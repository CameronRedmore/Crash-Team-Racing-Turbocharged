#include <macros.h>
#include "platform/native_title_logo.h"

#if NATIVE_TITLE_LOGO_SUPPORTED

#include <math.h>
#include <stdlib.h>
#include <string.h>

// stb_truetype comes with native_font.c, earlier in the unity build.
#include "platform/native_aspect.h"
#include "platform/native_assets.h"
#include "platform/native_log.h"
#include "platform/native_renderer.h"

#define NATIVE_TITLE_LOGO_FONT        "fonts/FuzzyBubbles-Bold.ttf"
#define NATIVE_TITLE_LOGO_TEXT        "TURBOCHARGED"

// Distance field texels per cap height, and the supersampling of the mask
// the distance transform runs on (averaging smooths the stair-stepped edge
// into a field the bevel can shade).
#define NATIVE_TITLE_LOGO_CAP_TEXELS  112
#define NATIVE_TITLE_LOGO_SUPERSAMPLE 3
// Space around the lettering in the field, in cap heights.
#define NATIVE_TITLE_LOGO_PAD         0.35f
// The font's strokes are thin for a solid; fatten them, in cap heights.
#define NATIVE_TITLE_LOGO_GROW        0.04f
#define NATIVE_TITLE_LOGO_TRACKING    0.0f

// Room around the lettering in the rendered image for the extrusion and the
// tilt, in cap heights.
#define NATIVE_TITLE_LOGO_MARGIN_X    0.3f
#define NATIVE_TITLE_LOGO_MARGIN_Y    0.45f
#define NATIVE_TITLE_LOGO_MAX_TEXTURE 4096

// Matches the native minimap: override texture coordinates use a virtual
// 254x254 extent.
#define NATIVE_TITLE_LOGO_UV_EXTENT   254

// Each letter hops off the baseline (cap heights) and leans (degrees,
// anticlockwise), so the word looks hand-placed.
static const float s_nativeTitleLogoJitter[][2] = {
    {0.03f, -5.0f}, {-0.03f, 4.0f}, {0.04f, -3.0f}, {-0.02f, 5.0f}, {0.03f, -4.0f}, {-0.04f, 3.0f},
    {0.02f, -5.0f}, {-0.03f, 4.0f}, {0.04f, -3.0f}, {-0.02f, 5.0f}, {0.03f, -4.0f}, {-0.03f, 3.0f},
};

// Ray marched solid: the field extruded backwards with a shear, so every
// letter casts the same dark slab down and to the right, and a rounded bevel
// on the front edge. Units are cap heights; the front face is z = 0.
global_variable const char *s_nativeTitleLogoShader =
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
    "uniform sampler2D s_sdf;\n"
    "uniform vec2 u_sdfExtent;\n"
    "uniform vec2 u_textCenter;\n"
    "uniform vec2 u_textHalf;\n"
    "uniform vec2 u_viewHalf;\n"
    "uniform vec2 u_pixel;\n"
    "uniform float u_pad;\n"
    "uniform float u_normalEps;\n"
    "uniform mat3 u_rotation;\n"
    "const float BEVEL = 0.06;\n"
    "const float DEPTH = 0.30;\n"
    "const vec2 SHEAR = vec2(0.28, -0.36);\n"
    "const float CAMERA = 40.0;\n"
    "float field(vec2 p) {\n"
    "    vec2 q = p + u_textCenter;\n"
    "    vec2 half_ = 0.5 * u_sdfExtent;\n"
    "    if (any(greaterThan(abs(q), half_))) {\n"
    "        return length(max(abs(q) - (half_ - u_pad), 0.0));\n"
    "    }\n"
    "    return texture(s_sdf, vec2(q.x / u_sdfExtent.x + 0.5, 0.5 - q.y / u_sdfExtent.y)).r;\n"
    "}\n"
    "float scene(vec3 p) {\n"
    "    float depth = clamp(-p.z, 0.0, DEPTH);\n"
    "    vec2 q = vec2(field(p.xy - SHEAR * depth) + BEVEL, p.z + BEVEL);\n"
    "    float front = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - BEVEL;\n"
    "    return max(front, -p.z - DEPTH);\n"
    "}\n"
    "vec3 sceneNormal(vec3 p) {\n"
    "    vec2 e = vec2(u_normalEps, 0.0);\n"
    "    return normalize(vec3(scene(p + e.xyy) - scene(p - e.xyy), scene(p + e.yxy) - scene(p - e.yxy), scene(p + e.yyx) - scene(p - e.yyx)));\n"
    "}\n"
    "vec4 shade(vec2 uv) {\n"
    "    // Row 0 of the target is the top of the image.\n"
    "    vec2 s = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0) * u_viewHalf;\n"
    "    vec3 rd = normalize(vec3(s, -CAMERA));\n"
    "    vec3 ro = vec3(0.0, 0.0, CAMERA);\n"
    "    // Turn the camera instead of the solid.\n"
    "    ro = ro * u_rotation;\n"
    "    rd = rd * u_rotation;\n"
    "    vec3 boxHalf = vec3(u_textHalf + abs(SHEAR) * DEPTH + BEVEL, DEPTH * 0.5 + 0.01);\n"
    "    vec3 boxCenter = vec3(0.0, 0.0, -DEPTH * 0.5);\n"
    "    vec3 inv = 1.0 / rd;\n"
    "    vec3 t0 = (boxCenter - boxHalf - ro) * inv;\n"
    "    vec3 t1 = (boxCenter + boxHalf - ro) * inv;\n"
    "    vec3 tMin = min(t0, t1);\n"
    "    vec3 tMax = max(t0, t1);\n"
    "    float tNear = max(max(tMin.x, tMin.y), tMin.z);\n"
    "    float tFar = min(min(tMax.x, tMax.y), tMax.z);\n"
    "    if (tNear > tFar) {\n"
    "        return vec4(0.0);\n"
    "    }\n"
    "    float eps = 0.25 * u_pixel.y * 2.0 * u_viewHalf.y;\n"
    "    float t = tNear;\n"
    "    bool hit = false;\n"
    "    for (int i = 0; i < 112; i++) {\n"
    "        float d = scene(ro + rd * t);\n"
    "        if (d < eps) { hit = true; break; }\n"
    "        t += d * 0.8;\n"
    "        if (t > tFar) { break; }\n"
    "    }\n"
    "    if (!hit) {\n"
    "        return vec4(0.0);\n"
    "    }\n"
    "    vec3 p = ro + rd * t;\n"
    "    vec3 n = sceneNormal(p);\n"
    "    // Light and view in the solid's space.\n"
    "    vec3 l = normalize(vec3(-0.45, 0.65, 0.62)) * u_rotation;\n"
    "    vec3 v = -rd;\n"
    "    float depth = -p.z;\n"
    "    float face = clamp((n.z - 0.80) / 0.15, 0.0, 1.0);\n"
    "    float side = clamp((depth - BEVEL) / 0.06, 0.0, 1.0);\n"
    "    vec3 base = mix(vec3(0.93, 0.48, 0.08), vec3(1.0, 0.80, 0.22), face);\n"
    "    base = mix(base, vec3(0.20, 0.11, 0.05), side);\n"
    "    float diffuse = clamp(dot(n, l) * 0.6 + 0.4, 0.0, 1.0);\n"
    "    float spec = pow(clamp(dot(n, normalize(l + v)), 0.0, 1.0), 40.0) * (1.0 - side * 0.7);\n"
    "    float fresnel = pow(1.0 - clamp(dot(n, v), 0.0, 1.0), 3.0);\n"
    "    float ao = 1.0;\n"
    "    for (int k = 1; k <= 4; k++) {\n"
    "        float h = 0.03 * float(k);\n"
    "        ao -= (h - scene(p + n * h)) * 6.0 / float(1 << k);\n"
    "    }\n"
    "    ao = clamp(ao, 0.35, 1.0);\n"
    "    vec3 color = base * (0.35 + 0.75 * diffuse) * ao + spec * 0.55 + fresnel * 0.12 * base;\n"
    "    return vec4(clamp(color, 0.0, 1.0), 1.0);\n"
    "}\n"
    "void main() {\n"
    "    // Rotated-grid 4x supersampling, resolved to straight alpha.\n"
    "    vec4 sum = vec4(0.0);\n"
    "    sum += shade(v_uv + u_pixel * vec2(-0.125, -0.375));\n"
    "    sum += shade(v_uv + u_pixel * vec2( 0.375, -0.125));\n"
    "    sum += shade(v_uv + u_pixel * vec2( 0.125,  0.375));\n"
    "    sum += shade(v_uv + u_pixel * vec2(-0.375,  0.125));\n"
    "    fragColor = (sum.a > 0.0) ? vec4(sum.rgb / sum.a, sum.a * 0.25) : vec4(0.0);\n"
    "}\n"
    "#endif\n";

struct NativeTitleLogoState
{
	b32 built;
	b32 failed;
	// Distance field, cap heights per texel = 1 / NATIVE_TITLE_LOGO_CAP_TEXELS.
	float *field;
	int fieldWidth;
	int fieldHeight;
	// Field extent, the lettering's centre relative to the field's centre and
	// the lettering's half size, all in cap heights.
	float extentX, extentY;
	float centerX, centerY;
	float halfX, halfY;

	u32 fieldTexture;
	ShaderID shader;
	GLint locExtent, locTextCenter, locTextHalf, locViewHalf, locPixel, locPad, locNormalEps, locRotation;
	struct NativeRenderTarget target;
	b32 targetReady;
	b32 rendered;
	float renderedPitch;
	float renderedRoll;
};

global_variable struct NativeTitleLogoState s_nativeTitleLogo;

// Felzenszwalb and Huttenlocher's exact squared distance transform along one
// row or column: d[q] = min over p of (q - p)^2 + f[p].
internal void NativeTitleLogo_Distance1D(const float *f, int n, float *d, int *v, float *z)
{
	const float inf = 1e20f;
	int k = 0;
	v[0] = 0;
	z[0] = -inf;
	z[1] = inf;
	for (int q = 1; q < n; q++)
	{
		// z[0] is -inf, so this stops at k = 0 at the latest.
		float s;
		for (;;)
		{
			const int p = v[k];
			s = ((f[q] + (float)q * (float)q) - (f[p] + (float)p * (float)p)) / (float)(2 * q - 2 * p);
			if (s > z[k])
			{
				break;
			}
			k--;
		}
		k++;
		v[k] = q;
		z[k] = s;
		z[k + 1] = inf;
	}
	k = 0;
	for (int q = 0; q < n; q++)
	{
		while (z[k + 1] < (float)q)
		{
			k++;
		}
		const float dq = (float)(q - v[k]);
		d[q] = dq * dq + f[v[k]];
	}
}

// Squared distance from every pixel to the nearest pixel where mask == want.
internal b32 NativeTitleLogo_Distance2D(const u8 *mask, u8 want, int width, int height, float *out)
{
	const int n = width > height ? width : height;
	float *f = (float *)malloc(sizeof(float) * n);
	float *d = (float *)malloc(sizeof(float) * n);
	int *v = (int *)malloc(sizeof(int) * n);
	float *z = (float *)malloc(sizeof(float) * (n + 1));
	b32 ok = (f != NULL) && (d != NULL) && (v != NULL) && (z != NULL);

	if (ok)
	{
		for (int i = 0; i < width * height; i++)
		{
			out[i] = (mask[i] == want) ? 0.0f : 1e20f;
		}
		for (int x = 0; x < width; x++)
		{
			for (int y = 0; y < height; y++)
			{
				f[y] = out[y * width + x];
			}
			NativeTitleLogo_Distance1D(f, height, d, v, z);
			for (int y = 0; y < height; y++)
			{
				out[y * width + x] = d[y];
			}
		}
		for (int y = 0; y < height; y++)
		{
			float *row = &out[y * width];
			memcpy(f, row, sizeof(float) * width);
			NativeTitleLogo_Distance1D(f, width, row, v, z);
		}
	}

	free(f);
	free(d);
	free(v);
	free(z);
	return ok;
}

internal b32 NativeTitleLogo_BuildField(void)
{
	struct NativeAssetsByteBuffer bytes = {0};
	stbtt_fontinfo info;
	u8 *mask = NULL;
	float *inside = NULL;
	float *outside = NULL;
	b32 ok = false;

	if (!NativeAssets_ReadBytes(NATIVE_TITLE_LOGO_FONT, NATIVE_ASSET_READ_DATA_FILE, &bytes))
	{
		Platform_LogError("[CTR Title] Failed to read assets/%s\n", NATIVE_TITLE_LOGO_FONT);
		return false;
	}
	if (!stbtt_InitFont(&info, bytes.data, stbtt_GetFontOffsetForIndex(bytes.data, 0)))
	{
		Platform_LogError("[CTR Title] assets/%s is not a usable TrueType font\n", NATIVE_TITLE_LOGO_FONT);
		goto done;
	}

	int capX0, capY0, capX1, capY1;
	if (!stbtt_GetCodepointBox(&info, 'H', &capX0, &capY0, &capX1, &capY1) || (capY1 <= 0))
	{
		Platform_LogError("[CTR Title] assets/%s has no cap height\n", NATIVE_TITLE_LOGO_FONT);
		goto done;
	}

	const char *text = NATIVE_TITLE_LOGO_TEXT;
	const int length = (int)strlen(text);
	const float capPixels = (float)(NATIVE_TITLE_LOGO_CAP_TEXELS * NATIVE_TITLE_LOGO_SUPERSAMPLE);
	const float scale = capPixels / (float)capY1;
	const int pad = (int)(NATIVE_TITLE_LOGO_PAD * capPixels);

	// Pen positions, in mask pixels from the left padding.
	float pen[16];
	if (length > (int)(sizeof(pen) / sizeof(pen[0])))
	{
		goto done;
	}
	float penX = 0.0f;
	for (int i = 0; i < length; i++)
	{
		int advance, bearing;
		stbtt_GetCodepointHMetrics(&info, text[i], &advance, &bearing);
		pen[i] = penX;
		penX += (float)advance * scale + NATIVE_TITLE_LOGO_TRACKING * capPixels;
		if (i + 1 < length)
		{
			penX += (float)stbtt_GetCodepointKernAdvance(&info, text[i], text[i + 1]) * scale;
		}
	}

	// Whole supersampled texels, so the field downsamples evenly.
	const int ss = NATIVE_TITLE_LOGO_SUPERSAMPLE;
	const int maskWidth = (((int)ceilf(penX) + 2 * pad) + ss - 1) / ss * ss;
	const int maskHeight = (((int)capPixels + 2 * pad) + ss - 1) / ss * ss;
	const float baseline = (float)pad + capPixels;

	mask = (u8 *)calloc((size_t)maskWidth * maskHeight, 1);
	inside = (float *)malloc(sizeof(float) * maskWidth * maskHeight);
	outside = (float *)malloc(sizeof(float) * maskWidth * maskHeight);
	if ((mask == NULL) || (inside == NULL) || (outside == NULL))
	{
		goto done;
	}

	int minX = maskWidth, minY = maskHeight, maxX = -1, maxY = -1;
	for (int i = 0; i < length; i++)
	{
		stbtt_vertex *shape = NULL;
		const int numVertices = stbtt_GetCodepointShape(&info, text[i], &shape);
		int glyphX0, glyphY0, glyphX1, glyphY1;
		if ((numVertices <= 0) || !stbtt_GetCodepointBox(&info, text[i], &glyphX0, &glyphY0, &glyphX1, &glyphY1))
		{
			stbtt_FreeShape(&info, shape);
			continue;
		}

		// Turn the outline itself (font units, y up) about the middle of the
		// glyph at half cap height, and lift it off the baseline, so stb
		// rasterises the leaning letter exactly.
		const float *jitter = s_nativeTitleLogoJitter[i % (int)(sizeof(s_nativeTitleLogoJitter) / sizeof(s_nativeTitleLogoJitter[0]))];
		const float angle = jitter[1] * 3.14159265f / 180.0f;
		const float c = cosf(angle);
		const float s = sinf(angle);
		const float pivotX = 0.5f * (float)(glyphX0 + glyphX1);
		const float pivotY = 0.5f * (float)capY1;
		const float hop = jitter[0] * (float)capY1;
		float boxX0 = 1e9f, boxY0 = 1e9f, boxX1 = -1e9f, boxY1 = -1e9f;

#define NATIVE_TITLE_LOGO_TURN(px, py)                   \
	do                                                   \
	{                                                    \
		const float dx = (float)(px) - pivotX;           \
		const float dy = (float)(py) - pivotY;           \
		const float tx = pivotX + dx * c - dy * s;       \
		const float ty = pivotY + dx * s + dy * c + hop; \
		(px) = (stbtt_vertex_type)lrintf(tx);            \
		(py) = (stbtt_vertex_type)lrintf(ty);            \
		boxX0 = fminf(boxX0, tx);                        \
		boxY0 = fminf(boxY0, ty);                        \
		boxX1 = fmaxf(boxX1, tx);                        \
		boxY1 = fmaxf(boxY1, ty);                        \
	} while (0)
		// Control points are only set (and only read) on curves.
		for (int v = 0; v < numVertices; v++)
		{
			NATIVE_TITLE_LOGO_TURN(shape[v].x, shape[v].y);
			if ((shape[v].type == STBTT_vcurve) || (shape[v].type == STBTT_vcubic))
			{
				NATIVE_TITLE_LOGO_TURN(shape[v].cx, shape[v].cy);
			}
			if (shape[v].type == STBTT_vcubic)
			{
				NATIVE_TITLE_LOGO_TURN(shape[v].cx1, shape[v].cy1);
			}
		}
#undef NATIVE_TITLE_LOGO_TURN

		// Same box convention as stbtt_GetGlyphBitmapBox; y grows down.
		const int ix0 = (int)floorf(boxX0 * scale) - 1;
		const int iy0 = (int)floorf(-boxY1 * scale) - 1;
		stbtt__bitmap bitmap;
		bitmap.w = (int)ceilf(boxX1 * scale) + 1 - ix0;
		bitmap.h = (int)ceilf(-boxY0 * scale) + 1 - iy0;
		bitmap.stride = bitmap.w;
		bitmap.pixels = (u8 *)calloc((size_t)bitmap.w * bitmap.h, 1);
		if (bitmap.pixels == NULL)
		{
			stbtt_FreeShape(&info, shape);
			goto done;
		}
		stbtt_Rasterize(&bitmap, 0.35f, shape, numVertices, scale, scale, 0.0f, 0.0f, ix0, iy0, 1, info.userdata);
		stbtt_FreeShape(&info, shape);

		const int left = pad + (int)lrintf(pen[i]) + ix0;
		const int top = (int)baseline + iy0;
		for (int y = 0; y < bitmap.h; y++)
		{
			const int maskY = top + y;
			if ((maskY < 0) || (maskY >= maskHeight))
			{
				continue;
			}
			for (int x = 0; x < bitmap.w; x++)
			{
				const int maskX = left + x;
				if ((maskX < 0) || (maskX >= maskWidth) || (bitmap.pixels[y * bitmap.w + x] < 128))
				{
					continue;
				}
				mask[maskY * maskWidth + maskX] = 1;
				minX = maskX < minX ? maskX : minX;
				minY = maskY < minY ? maskY : minY;
				maxX = maskX > maxX ? maskX : maxX;
				maxY = maskY > maxY ? maskY : maxY;
			}
		}
		free(bitmap.pixels);
	}

	if (maxX < 0)
	{
		Platform_LogError("[CTR Title] assets/%s has no glyphs for the title\n", NATIVE_TITLE_LOGO_FONT);
		goto done;
	}

	if (!NativeTitleLogo_Distance2D(mask, 1, maskWidth, maskHeight, outside) || !NativeTitleLogo_Distance2D(mask, 0, maskWidth, maskHeight, inside))
	{
		goto done;
	}

	const int width = maskWidth / ss;
	const int height = maskHeight / ss;
	float *field = (float *)malloc(sizeof(float) * width * height);
	if (field == NULL)
	{
		goto done;
	}

	// Signed distance in cap heights (negative inside), averaged down.
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			float sum = 0.0f;
			for (int sy = 0; sy < ss; sy++)
			{
				for (int sx = 0; sx < ss; sx++)
				{
					const int i = (y * ss + sy) * maskWidth + x * ss + sx;
					sum += sqrtf(outside[i]) - sqrtf(inside[i]);
				}
			}
			field[y * width + x] = sum / ((float)(ss * ss) * capPixels) - NATIVE_TITLE_LOGO_GROW;
		}
	}

	s_nativeTitleLogo.field = field;
	s_nativeTitleLogo.fieldWidth = width;
	s_nativeTitleLogo.fieldHeight = height;
	s_nativeTitleLogo.extentX = (float)maskWidth / capPixels;
	s_nativeTitleLogo.extentY = (float)maskHeight / capPixels;
	// Field y points up from the field's centre.
	s_nativeTitleLogo.centerX = (0.5f * (float)(minX + maxX + 1) - 0.5f * (float)maskWidth) / capPixels;
	s_nativeTitleLogo.centerY = (0.5f * (float)maskHeight - 0.5f * (float)(minY + maxY + 1)) / capPixels;
	s_nativeTitleLogo.halfX = 0.5f * (float)(maxX + 1 - minX) / capPixels + NATIVE_TITLE_LOGO_GROW;
	s_nativeTitleLogo.halfY = 0.5f * (float)(maxY + 1 - minY) / capPixels + NATIVE_TITLE_LOGO_GROW;
	ok = true;

done:
	free(mask);
	free(inside);
	free(outside);
	NativeAssets_FreeBytes(&bytes);
	return ok;
}

internal b32 NativeTitleLogo_BuildGpu(void)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, s_nativeTitleLogo.fieldWidth, s_nativeTitleLogo.fieldHeight, 0, GL_RED, GL_FLOAT, s_nativeTitleLogo.field);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	NativeRenderer_InvalidateTextureBinding();
	s_nativeTitleLogo.fieldTexture = texture;

	const ShaderID shader = NativeRenderer_Shader_Compile(s_nativeTitleLogoShader, false, NULL);
	GLint linked = GL_FALSE;
	glGetProgramiv(shader, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE)
	{
		glDeleteProgram(shader);
		return false;
	}
	s_nativeTitleLogo.shader = shader;
	s_nativeTitleLogo.locExtent = glGetUniformLocation(shader, "u_sdfExtent");
	s_nativeTitleLogo.locTextCenter = glGetUniformLocation(shader, "u_textCenter");
	s_nativeTitleLogo.locTextHalf = glGetUniformLocation(shader, "u_textHalf");
	s_nativeTitleLogo.locViewHalf = glGetUniformLocation(shader, "u_viewHalf");
	s_nativeTitleLogo.locPixel = glGetUniformLocation(shader, "u_pixel");
	s_nativeTitleLogo.locPad = glGetUniformLocation(shader, "u_pad");
	s_nativeTitleLogo.locNormalEps = glGetUniformLocation(shader, "u_normalEps");
	s_nativeTitleLogo.locRotation = glGetUniformLocation(shader, "u_rotation");

	glUseProgram(shader);
	glUniform1i(glGetUniformLocation(shader, "s_sdf"), 0);
	glUseProgram(s_previousShader == (ShaderID)-1 ? 0 : s_previousShader);

	NativeRenderer_InitRenderTarget(&s_nativeTitleLogo.target);
	s_nativeTitleLogo.targetReady = true;
	return true;
}

int NativeTitleLogo_Prepare(void)
{
	if (s_nativeTitleLogo.built)
	{
		return 1;
	}
	if (s_nativeTitleLogo.failed)
	{
		return 0;
	}

	const u64 start = SDL_GetTicksNS();
	if (!NativeTitleLogo_BuildField() || !NativeTitleLogo_BuildGpu())
	{
		Platform_LogError("[CTR Title] 3D title lettering unavailable; keeping the plaque\n");
		NativeTitleLogo_ReleaseGpu();
		s_nativeTitleLogo.failed = true;
		return 0;
	}
	Platform_Log("[CTR Title] Built %dx%d title lettering field in %.1f ms\n", s_nativeTitleLogo.fieldWidth, s_nativeTitleLogo.fieldHeight,
	             (double)(SDL_GetTicksNS() - start) / 1e6);
	s_nativeTitleLogo.built = true;
	return 1;
}

// Image half size in cap heights: the lettering plus room for the extrusion
// and the tilt.
internal void NativeTitleLogo_GetViewHalf(float *halfX, float *halfY)
{
	*halfX = s_nativeTitleLogo.halfX + NATIVE_TITLE_LOGO_MARGIN_X;
	*halfY = s_nativeTitleLogo.halfY + NATIVE_TITLE_LOGO_MARGIN_Y;
}

internal void NativeTitleLogo_Render(int width, int height, float pitch, float roll)
{
	struct NativeTitleLogoState *logo = &s_nativeTitleLogo;
	float viewHalfX, viewHalfY;
	NativeTitleLogo_GetViewHalf(&viewHalfX, &viewHalfY);

	NativeRenderer_EnsureRenderTarget(&logo->target, width, height);
	glBindTexture(GL_TEXTURE_2D, logo->target.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Roll about z, then pitch about x. GLSL fills matrices column by column;
	// the shader multiplies row vectors, which applies the inverse.
	const float cp = cosf(pitch), sp = sinf(pitch);
	const float cr = cosf(roll), sr = sinf(roll);
	const float rotation[9] = {
	    cr, cp * sr, sp * sr, -sr, cp * cr, sp * cr, 0.0f, -sp, cp,
	};

	struct NativeRendererPassState state;
	NativeRenderer_BeginUtilityPass(&state, logo->target.framebuffer, 0, 0, width, height);

	glUseProgram(logo->shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, logo->fieldTexture);
	glUniform2f(logo->locExtent, logo->extentX, logo->extentY);
	glUniform2f(logo->locTextCenter, logo->centerX, logo->centerY);
	glUniform2f(logo->locTextHalf, logo->halfX, logo->halfY);
	glUniform2f(logo->locViewHalf, viewHalfX, viewHalfY);
	glUniform2f(logo->locPixel, 1.0f / (float)width, 1.0f / (float)height);
	glUniform1f(logo->locPad, NATIVE_TITLE_LOGO_PAD);
	glUniform1f(logo->locNormalEps, 1.0f / (float)NATIVE_TITLE_LOGO_CAP_TEXELS);
	glUniformMatrix3fv(logo->locRotation, 1, GL_FALSE, rotation);

	glBindVertexArray(s_vramQuadVAO);
	NativeRenderer_DrawTriangles(0, 2);

	NativeRenderer_EndUtilityPass(&state);

	logo->rendered = true;
	logo->renderedPitch = pitch;
	logo->renderedRoll = roll;
}

int NativeTitleLogo_Draw(struct PrimMem *primMem, u32 *ot, const struct NativeTitleLogoPose *pose)
{
	struct NativeTitleLogoState *logo = &s_nativeTitleLogo;

	if (!NativeTitleLogo_Prepare() || (primMem == NULL) || (ot == NULL))
	{
		return 0;
	}

	const size_t packetSize = sizeof(DR_PSYX_TEX) * 2 + sizeof(POLY_FT4);
	if (((uintptr_t)primMem->cursor > (uintptr_t)primMem->end) || ((uintptr_t)primMem->end - (uintptr_t)primMem->cursor < packetSize))
	{
		return 0;
	}

	float viewHalfX, viewHalfY;
	NativeTitleLogo_GetViewHalf(&viewHalfX, &viewHalfY);

	// One cap height in UI pixels. UI pixels are 4:3 picture fractions of
	// 512x216, and widescreen squeezes x.
	const float scaleX = (float)NativeAspect_GetScaleX();
	const float unitX = pose->width / (2.0f * logo->halfX) * scaleX;
	const float unitY = pose->width / (2.0f * logo->halfX) * (4.0f / 3.0f) * ((float)SCREEN_HEIGHT / (float)SCREEN_WIDTH);
	const float quadHalfX = viewHalfX * unitX;
	const float quadHalfY = viewHalfY * unitY;

	// Render at the output's pixel density.
	const int targetHeight = (s_mainRenderTarget.height > 1) ? s_mainRenderTarget.height : 720;
	int height = (int)ceilf(2.0f * quadHalfY * (float)targetHeight / (float)SCREEN_HEIGHT);
	int width = (int)ceilf((float)height * viewHalfX / viewHalfY);
	if (width > NATIVE_TITLE_LOGO_MAX_TEXTURE)
	{
		height = height * NATIVE_TITLE_LOGO_MAX_TEXTURE / width;
		width = NATIVE_TITLE_LOGO_MAX_TEXTURE;
	}
	if ((width < 16) || (height < 4))
	{
		return 0;
	}

	if (!logo->rendered || (logo->target.width != width) || (logo->target.height != height) || (fabsf(logo->renderedPitch - pose->pitch) > 1e-4f) ||
	    (fabsf(logo->renderedRoll - pose->roll) > 1e-4f))
	{
		NativeTitleLogo_Render(width, height, pose->pitch, pose->roll);
	}

	const float left = pose->centerX - quadHalfX;
	const float right = pose->centerX + quadHalfX;
	const float top = pose->centerY - quadHalfY;
	const float bottom = pose->centerY + quadHalfY;

	DR_PSYX_TEX *set = primMem->cursor;
	POLY_FT4 *p = (POLY_FT4 *)(set + 1);
	DR_PSYX_TEX *reset = (DR_PSYX_TEX *)(p + 1);
	memset(p, 0, sizeof(*p));
	CtrGpu_WriteColorCode(&p->r0, 0x808080);
	setPolyFT4(p);
	p->x0 = p->x2 = (s16)floorf(left);
	p->x1 = p->x3 = (s16)ceilf(right);
	p->y0 = p->y1 = (s16)floorf(top);
	p->y2 = p->y3 = (s16)ceilf(bottom);
	p->u1 = p->u3 = NATIVE_TITLE_LOGO_UV_EXTENT;
	p->v2 = p->v3 = NATIVE_TITLE_LOGO_UV_EXTENT;
	NativePgxp_SetScreenXY(&p->x0, left, top);
	NativePgxp_SetScreenXY(&p->x1, right, top);
	NativePgxp_SetScreenXY(&p->x2, left, bottom);
	NativePgxp_SetScreenXY(&p->x3, right, bottom);
	SetPsyXTexture(set, logo->target.texture, NATIVE_TITLE_LOGO_UV_EXTENT, NATIVE_TITLE_LOGO_UV_EXTENT);
	set->code[1] |= PSYX_TEX_FLAG_STRAIGHT_ALPHA;
	SetPsyXTexture(reset, 0, 0, 0);
	const u32 oldTag = *ot;
	set->tag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(p), 2u << 24);
	p->tag = CtrGpu_PackOTTag(CtrGpu_PrimToOTLink24(reset), 9u << 24);
	reset->tag = CtrGpu_PackOTTag(oldTag, 2u << 24);
	*ot = CtrGpu_PrimToOTLink24(set);
	primMem->cursor = reset + 1;
	return 1;
}

void NativeTitleLogo_ReleaseGpu(void)
{
	struct NativeTitleLogoState *logo = &s_nativeTitleLogo;

	if (logo->fieldTexture != 0)
	{
		GLuint texture = logo->fieldTexture;
		glDeleteTextures(1, &texture);
		NativeRenderer_InvalidateTextureBinding();
		logo->fieldTexture = 0;
	}
	if (logo->shader != 0)
	{
		glDeleteProgram(logo->shader);
		logo->shader = 0;
	}
	if (logo->targetReady)
	{
		NativeRenderer_DestroyRenderTarget(&logo->target);
		logo->targetReady = false;
	}
	free(logo->field);
	logo->field = NULL;
	logo->built = false;
	logo->rendered = false;
}

#endif
