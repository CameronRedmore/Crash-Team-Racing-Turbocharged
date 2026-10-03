#include <common.h>
#include "platform/native_hud_icons.h"
#include "platform/native_pgxp.h"
#include <math.h>
#include <string.h>

#if !defined(__vita__)
// Shapes are built back to front and linked in reverse, since the OT prepends.
// All positions stay fractional, including with Classic PGXP disabled.
#define HUD_ICON_SEGMENTS 48
#define HUD_BUTTON_MAX_QUADS (HUD_ICON_SEGMENTS * 4 + 8)
#define HUD_LIGHT_MAX_QUADS (HUD_ICON_SEGMENTS * 7 + 24)
struct NativeHudIconMesh
{
	POLY_G4 *first;
	int count;
	float cx, cy, rx, ry;
	float rxY, ryX; // Cross terms for rotated menu arrows.
};

static u32 NativeHudIcon_Color(u32 top, u32 bottom, float y)
{
	const float t = fmaxf(0.0f, fminf(1.0f, (y + 1.0f) * 0.5f));
	u32 color = 0;
	for (int shift = 0; shift < 24; shift += 8)
	{
		const float a = (float)((top >> shift) & 255);
		const float b = (float)((bottom >> shift) & 255);
		color |= (u32)(a + (b - a) * t + 0.5f) << shift;
	}
	return color;
}

static void NativeHudIcon_Quad(struct NativeHudIconMesh *mesh, const float x[4], const float y[4], u32 top, u32 bottom)
{
	POLY_G4 *p = &mesh->first[mesh->count++];
	memset(p, 0, sizeof(*p));
	setInt32RGB4(p, NativeHudIcon_Color(top, bottom, y[0]), NativeHudIcon_Color(top, bottom, y[1]),
	             NativeHudIcon_Color(top, bottom, y[2]), NativeHudIcon_Color(top, bottom, y[3]));
	setPolyG4(p);
	s16 *points[4] = {&p->x0, &p->x1, &p->x2, &p->x3};
	for (int i = 0; i < 4; i++)
	{
		const float px = mesh->cx + x[i] * mesh->rx + y[i] * mesh->ryX;
		const float py = mesh->cy + y[i] * mesh->ry + x[i] * mesh->rxY;
		points[i][0] = (s16)px;
		points[i][1] = (s16)py;
		NativePgxp_SetScreenXY(points[i], px, py);
	}
}

static void NativeHudIcon_Ring(struct NativeHudIconMesh *mesh, float outer, float inner, u32 top, u32 bottom)
{
	for (int i = 0; i < HUD_ICON_SEGMENTS; i++)
	{
		const float a = (float)i * (6.28318530718f / HUD_ICON_SEGMENTS);
		const float b = (float)(i + 1) * (6.28318530718f / HUD_ICON_SEGMENTS);
		const float x[4] = {cosf(a) * outer, cosf(b) * outer, cosf(a) * inner, cosf(b) * inner};
		const float y[4] = {sinf(a) * outer, sinf(b) * outer, sinf(a) * inner, sinf(b) * inner};
		NativeHudIcon_Quad(mesh, x, y, top, bottom);
	}
}

static void NativeHudIcon_Line(struct NativeHudIconMesh *mesh, float x0, float y0, float x1, float y1, float width, u32 color)
{
	const float dx = x1 - x0, dy = y1 - y0;
	const float length = sqrtf(dx * dx + dy * dy);
	const float nx = -dy * width * 0.5f / length, ny = dx * width * 0.5f / length;
	const float x[4] = {x0 + nx, x1 + nx, x0 - nx, x1 - nx};
	const float y[4] = {y0 + ny, y1 + ny, y0 - ny, y1 - ny};
	NativeHudIcon_Quad(mesh, x, y, color, color);
}

static void NativeHudIcon_Box(struct NativeHudIconMesh *mesh, float left, float topY, float right, float bottomY,
                              u32 top, u32 bottom)
{
	const float x[4] = {left, right, left, right};
	const float y[4] = {topY, topY, bottomY, bottomY};
	NativeHudIcon_Quad(mesh, x, y, top, bottom);
}

static u32 NativeHudIcon_LensColor(float x, float y, int green, int lit)
{
	// Curved glass: dark edges, a broad upper-left reflection and a lower-right glint.
	const float radius = x*x + y*y;
	const float reflection = expf(-((x+0.38f)*(x+0.38f) + (y+0.48f)*(y+0.48f)) * 15.0f);
	const float glint = expf(-((x-0.47f)*(x-0.47f) + (y-0.54f)*(y-0.54f)) * 28.0f);
	const float glow = lit ? expf(-radius * 4.0f) : 0.0f;
	const float strength = fmaxf(0.0f, 1.0f - radius);
	const float primary = fminf(255.0f, 42.0f + strength * (lit ? 198.0f : 125.0f) + reflection*155.0f + glint*92.0f);
	const float other = fminf(255.0f, 4.0f + reflection*190.0f + glint*72.0f + glow*155.0f);
	const u32 c = (u32)primary, h = (u32)other;
	return green ? h | (c << 8) | (h << 16) : c | (h << 8) | (h << 16);
}

static void NativeHudIcon_Lens(struct NativeHudIconMesh *mesh, int green, int lit)
{
	for (int band = 0; band < 4; band++)
	{
		const float inner = band * 0.25f, outer = (band + 1) * 0.25f;
		for (int i = 0; i < HUD_ICON_SEGMENTS; i++)
		{
			const float a = i * (6.28318530718f / HUD_ICON_SEGMENTS);
			const float b = (i+1) * (6.28318530718f / HUD_ICON_SEGMENTS);
			const float lx[4] = {cosf(a)*outer, cosf(b)*outer, cosf(a)*inner, cosf(b)*inner};
			const float ly[4] = {sinf(a)*outer, sinf(b)*outer, sinf(a)*inner, sinf(b)*inner};
			float x[4], y[4];
			for (int j = 0; j < 4; j++) { x[j] = lx[j]*0.74f; y[j] = ly[j]*0.74f; }
			NativeHudIcon_Quad(mesh, x, y, 0, 0);
			POLY_G4 *p = &mesh->first[mesh->count-1];
			setInt32RGB4(p, NativeHudIcon_LensColor(lx[0], ly[0], green, lit),
			             NativeHudIcon_LensColor(lx[1], ly[1], green, lit),
			             NativeHudIcon_LensColor(lx[2], ly[2], green, lit),
			             NativeHudIcon_LensColor(lx[3], ly[3], green, lit));
			setPolyG4(p);
		}
	}
}

static int NativeHudIcon_Begin(struct NativeHudIconMesh *mesh, const struct Icon *icon, float x, float y, s16 scale,
                               int button, struct PrimMem *primMem, u32 *ot)
{
	if (!gNativeModernHudIconsEnabled || !icon || !primMem || !ot || scale <= 0) return 0;
	const size_t needed = (button ? HUD_BUTTON_MAX_QUADS : HUD_LIGHT_MAX_QUADS) * sizeof(POLY_G4);
	if ((uintptr_t)primMem->cursor > (uintptr_t)primMem->end ||
	    (uintptr_t)primMem->end - (uintptr_t)primMem->cursor < needed) return 0;
	float width = (float)FP_Mult(icon->texLayout.u1 - icon->texLayout.u0, scale);
	const float height = (float)FP_Mult(icon->texLayout.v2 - icon->texLayout.v0, scale);
#if CTR_NATIVE_WIDESCREEN
	// Match DecalHUD's centred squeeze, and DecalFont's button width fix.
	const float squeezed = (float)CTR_WIDESCREEN_SCALE_X((int)width);
	x += (width - squeezed) * 0.5f;
	width = button ? (float)(((int)width * 34 + 44) / 45) : squeezed;
#else
	(void)button;
#endif
	if (width <= 0 || height <= 0) return 0;
	*mesh = (struct NativeHudIconMesh){primMem->cursor, 0, x + width * 0.5f, y + height * 0.5f, width * 0.5f, height * 0.5f, 0, 0};
	return 1;
}

static void NativeHudIcon_End(struct NativeHudIconMesh *mesh, struct PrimMem *primMem, u32 *ot)
{
	for (int i = mesh->count - 1; i >= 0; i--) AddPrim(ot, &mesh->first[i]);
	primMem->cursor = mesh->first + mesh->count;
}
#endif

int NativeHudIcons_DrawButton(u8 character, const struct Icon *icon, float x, float y,
                             s16 scale, struct PrimMem *primMem, u32 *ot)
{
#if !defined(__vita__)
	if (character != '*' && character != '@' && character != '[' && character != '^') return 0;
	struct NativeHudIconMesh mesh;
	if (!NativeHudIcon_Begin(&mesh, icon, x, y, scale, 1, primMem, ot)) return 0;
	NativeHudIcon_Ring(&mesh, 0.98f, 0.0f, 0x0c0c0c, 0x080808);
	NativeHudIcon_Ring(&mesh, 0.88f, 0.0f, 0x8c8c8c, 0x282828);
	NativeHudIcon_Ring(&mesh, 0.77f, 0.0f, 0x484848, 0x202020);
	// Packed colours are R,G,B, from least to most significant byte.
	if (character == '*')
	{
		NativeHudIcon_Line(&mesh, -0.40f, -0.40f, 0.40f, 0.40f, 0.17f, 0xff9860);
		NativeHudIcon_Line(&mesh, -0.40f, 0.40f, 0.40f, -0.40f, 0.17f, 0xff9860);
	}
	else if (character == '@') NativeHudIcon_Ring(&mesh, 0.53f, 0.38f, 0x7070ff, 0x5050e8);
	else if (character == '[')
	{
		const float px[4] = {-0.43f, 0.43f, 0.43f, -0.43f};
		const float py[4] = {-0.43f, -0.43f, 0.43f, 0.43f};
		for (int i = 0; i < 4; i++) NativeHudIcon_Line(&mesh, px[i], py[i], px[(i+1)%4], py[(i+1)%4], 0.14f, 0xd890ed);
	}
	else
	{
		NativeHudIcon_Line(&mesh, 0, -0.53f, 0.49f, 0.38f, 0.14f, 0xa0e860);
		NativeHudIcon_Line(&mesh, 0.49f, 0.38f, -0.49f, 0.38f, 0.14f, 0xa0e860);
		NativeHudIcon_Line(&mesh, -0.49f, 0.38f, 0, -0.53f, 0.14f, 0xa0e860);
	}
	NativeHudIcon_End(&mesh, primMem, ot);
	return 1;
#else
	(void)character; (void)icon; (void)x; (void)y; (void)scale; (void)primMem; (void)ot;
	return 0;
#endif
}

int NativeHudIcons_DrawLight(const struct Icon *icon, float x, float y, s16 scale,
                            int green, int lit, struct PrimMem *primMem, u32 *ot)
{
#if !defined(__vita__)
	struct NativeHudIconMesh mesh;
	if (!NativeHudIcon_Begin(&mesh, icon, x, y, scale, 0, primMem, ot)) return 0;
	// Blue steel brackets and bottom mounting foot sit behind the bevelled housing.
	NativeHudIcon_Box(&mesh, -0.24f, 0.66f, 0.24f, 0.99f, 0x30241a, 0x30241a);
	NativeHudIcon_Box(&mesh, -0.19f, 0.72f, 0.19f, 0.97f, 0x70583a, 0x48321e);
	NativeHudIcon_Box(&mesh, -0.08f, 0.75f, 0.04f, 0.97f, 0x96784c, 0x604226);
	for (int side = -1; side <= 1; side += 2)
	{
		const float left = side < 0 ? -0.99f : 0.78f;
		NativeHudIcon_Box(&mesh, left, -0.29f, left+0.21f, 0.36f, 0x342b22, 0x342b22);
		NativeHudIcon_Box(&mesh, left+0.025f, -0.25f, left+0.18f, 0.31f, 0x9c8056, 0x60462b);
		NativeHudIcon_Box(&mesh, left+0.025f, -0.16f, left+0.18f, -0.04f, 0xe0d0a0, 0x987c50);
	}
	mesh.cy -= mesh.ry * 0.07f;
	mesh.ry *= 0.90f;
	NativeHudIcon_Ring(&mesh, 0.95f, 0.0f, 0x383830, 0x10100e);
	NativeHudIcon_Ring(&mesh, 0.90f, 0.0f, 0xc8ccc8, 0x484842);
	NativeHudIcon_Ring(&mesh, 0.81f, 0.0f, 0x181c20, 0x303038);
	NativeHudIcon_Lens(&mesh, green, lit);
	NativeHudIcon_End(&mesh, primMem, ot);
	return 1;
#else
	(void)icon; (void)x; (void)y; (void)scale; (void)green; (void)lit; (void)primMem; (void)ot;
	return 0;
#endif
}

int NativeHudIcons_DrawMenuArrow(const float x[4], const float y[4], const u32 colors[4],
                                 struct PrimMem *primMem, u32 *ot)
{
#if !defined(__vita__)
	if (!gNativeModernHudIconsEnabled || !primMem || !ot) return 0;
	// Seven-sided silhouette: a broad arrowhead with a short rectangular stem.
	// Its centre is inside the stem/head junction, making the fan convex per wedge.
	static const float px[7] = {-0.55f, -0.55f, -0.95f, -0.95f, -0.55f, -0.55f, 0.95f};
	static const float py[7] = {-0.95f, -0.36f, -0.36f, 0.36f, 0.36f, 0.95f, 0.0f};
	const size_t needed = 14 * sizeof(POLY_G4);
	if ((uintptr_t)primMem->cursor > (uintptr_t)primMem->end ||
	    (uintptr_t)primMem->end - (uintptr_t)primMem->cursor < needed) return 0;
	struct NativeHudIconMesh mesh = {primMem->cursor, 0,
		(x[0] + x[3]) * 0.5f, (y[0] + y[3]) * 0.5f,
		(x[1] - x[0]) * 0.5f, (y[2] - y[0]) * 0.5f,
		(y[1] - y[0]) * 0.5f, (x[2] - x[0]) * 0.5f};
	for (int layer = 0; layer < 2; layer++)
	{
		const float inset = layer ? 0.78f : 1.0f;
		for (int i = 0; i < 7; i++)
		{
			const int next = (i + 1) % 7;
			const float qx[4] = {-0.55f * inset, px[i] * inset, -0.55f * inset, px[next] * inset};
			const float qy[4] = {0, py[i] * inset, 0, py[next] * inset};
			NativeHudIcon_Quad(&mesh, qx, qy, layer ? colors[0] : 0, layer ? colors[2] : 0);
		}
	}
	NativeHudIcon_End(&mesh, primMem, ot);
	return 1;
#else
	(void)x; (void)y; (void)colors; (void)primMem; (void)ot;
	return 0;
#endif
}
