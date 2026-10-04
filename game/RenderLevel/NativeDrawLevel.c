#include <common.h>

// NOTE: Native 3D level geometry (see native_draw3d.h). Walks the same BSP
// render lists and per-quadblock visibility bits as overlay 226, and makes the
// same per-face decisions (face permutation, UV flip, winding, texture LOD,
// semi-transparency and draw-order bias), but submits the exact source
// vertices once instead of projecting them. The host clips at the near plane,
// so clip records are not needed and faces are only subdivided where retail
// changes texture per cell (mosaics) or morphs vertices (full-dynamic LOD).
// Shares overlay 226's texture helpers like overlays 227-229 do.

#if NATIVE_DRAW3D_SUPPORTED


enum
{
	NATIVE_DRAW_LEVEL_GRID_VERTICES = 9,
	// Sky faces can use negative D offsets from slot 0x3ff. Put the marker at
	// the retail level's farthest OT slot so those faces draw before the level.
	NATIVE_DRAW_LEVEL_MARKER_OT_INDEX = 0x3fc,
	// tpage blend bits 3 mark an opaque level face.
	NATIVE_DRAW_LEVEL_TPAGE_BLEND_MASK = 0x60,
};

// Retail 4x1 face selectors: byte lanes hold the 0x14-byte scratch record
// offsets of each face's grid vertices; the R226 table word picks the lanes.
static const u32 sNativeDrawLevelFaceSelectors[4] = {0x00506478, 0x5014788c, 0x647828a0, 0x788ca03c};

// Quarters of a subdivision frame, also the water faces.
static const u8 sNativeDrawLevelGridFaces[4][4] = {{0, 4, 5, 6}, {4, 1, 6, 7}, {5, 6, 2, 8}, {6, 7, 8, 3}};
static const u8 sNativeDrawLevelGridHalves[2][4] = {{0, 4, 2, 8}, {4, 1, 8, 3}};

// Retail subdivision frames hold corners 0-3, edge midpoints 4 (0-1), 5 (0-2),
// 7 (1-3) and 8 (2-3), and a centre 6.
enum NativeDrawLevelSplit
{
	// Quarters around the 1-2 diagonal midpoint (3x3 frames).
	NATIVE_DRAW_LEVEL_SPLIT_DIAGONAL,
	// Quarters around the midpoint of 4 and 8 (4x4 frames).
	NATIVE_DRAW_LEVEL_SPLIT_CENTER,
	// Halves across the 0-1 edge.
	NATIVE_DRAW_LEVEL_SPLIT_HALVES,
};

// Mosaic textures (retail deepest-frame UV reload): a near face whose fourth
// texture entry points to records is split twice and every cell takes its own
// TextureLayout record. Records form a row-major grid with four columns along
// the 0-1 edge; UV-swapped faces use a second grid right after the first.
struct NativeDrawLevelMosaicShape
{
	u8 split[2];
	u8 rows;
	// Bit n: near-corner set n selects the full subdivision handler, the only
	// one that loads mosaic records.
	u16 fullNearSets;
};

enum
{
	// Two opposite corners or three or more.
	NATIVE_DRAW_LEVEL_FULL_NEAR_SETS = 0xeac0,
	// At least one corner on each side of the split.
	NATIVE_DRAW_LEVEL_FULL_NEAR_SETS_HALVES = 0xeee0,
};

static const struct NativeDrawLevelMosaicShape sNativeDrawLevelMosaicShapes[RENDER_LIST_SLOT_WATER] = {
    [RENDER_LIST_SLOT_4X4] = {{NATIVE_DRAW_LEVEL_SPLIT_CENTER, NATIVE_DRAW_LEVEL_SPLIT_CENTER}, 4, NATIVE_DRAW_LEVEL_FULL_NEAR_SETS},
    [RENDER_LIST_SLOT_DYNAMIC_SUBDIV] = {{NATIVE_DRAW_LEVEL_SPLIT_DIAGONAL, NATIVE_DRAW_LEVEL_SPLIT_DIAGONAL}, 4, NATIVE_DRAW_LEVEL_FULL_NEAR_SETS},
    [RENDER_LIST_SLOT_4X2] = {{NATIVE_DRAW_LEVEL_SPLIT_DIAGONAL, NATIVE_DRAW_LEVEL_SPLIT_HALVES}, 2, NATIVE_DRAW_LEVEL_FULL_NEAR_SETS},
    [RENDER_LIST_SLOT_4X1] = {{NATIVE_DRAW_LEVEL_SPLIT_HALVES, NATIVE_DRAW_LEVEL_SPLIT_HALVES}, 1, NATIVE_DRAW_LEVEL_FULL_NEAR_SETS_HALVES},
};

struct NativeDrawLevelContext
{
	int layer;
	struct PushBuffer *pushBuffer;
	const struct LevVertex *vertices;
	const struct MainRenderLevelGeometryScratch *lod;
	const struct TextureLayout *waterEnvMap;
	const NativeDraw3DView *view;
	// Mosaic layout of the render list being drawn.
	const struct NativeDrawLevelMosaicShape *mosaic;
	// Camera position for the water distance fade (retail pb->data6).
	s16 center[3];
};

struct NativeDrawLevelVisibility
{
	const u32 *word;
	u32 value;
	s32 bit;
};

// Retail 0x800a0f0c/0x800a0f34: seed from the leaf's first blockID, then
// consume one bit per quadblock.
static void NativeDrawLevel_SeedVisibility(struct NativeDrawLevelVisibility *vis, const int *visFaceList, const struct QuadBlock *block)
{
	const u32 blockID = (u16)block->blockID;
	vis->word = (const u32 *)((const u8 *)visFaceList + ((blockID >> 3) & 0x1fc));
	vis->value = *vis->word;
	vis->bit = (s32)(blockID & 0x1f);
}

static int NativeDrawLevel_ConsumeVisibility(struct NativeDrawLevelVisibility *vis)
{
	if (vis->bit < 0)
	{
		vis->word++;
		vis->value = *vis->word;
		vis->bit = 0x1f;
	}

	const u32 shifted = vis->value << vis->bit;
	vis->bit--;
	return (s32)shifted < 0;
}

static float NativeDrawLevel_CameraDepth(const NativeDraw3DView *view, const NativeDraw3DVertex *vertex)
{
	const double *r = view->rotation;
	return (float)(r[6] * vertex->x + r[7] * vertex->y + r[8] * vertex->z + view->translation[2]);
}

static NativeDraw3DVertex NativeDrawLevel_Vertex(const struct LevVertex *vertex, const u8 *color)
{
	NativeDraw3DVertex out;
	out.x = vertex->pos.x;
	out.y = vertex->pos.y;
	out.z = vertex->pos.z;
	out.u = 0;
	out.v = 0;
	out.r = color[0];
	out.g = color[1];
	out.b = color[2];
	return out;
}

static void NativeDrawLevel_SetUv(NativeDraw3DVertex *vertex, u16 uv)
{
	vertex->u = (u8)uv;
	vertex->v = (u8)(uv >> 8);
}

// Retail sorts a face drawOrder OT slots behind its depth. Positive values hide
// faces behind walls they poke through (hub tunnel mouths), so those move back
// (NativeDraw3D_GetDrawOrderSlotDepth). A forward pull would cut through karts
// beside the face, so negative values keep the small coplanar bias.
static void NativeDrawLevel_SetDrawOrder(NativeDraw3DMaterial *material, s8 drawOrder)
{
	material->depthBias = drawOrder;
	material->depthSlots = drawOrder > 0 ? (u8)drawOrder : 0;
}

static u8 NativeDrawLevel_BaseFlags(const struct QuadBlock *block, const struct TextureLayout *texture)
{
	u8 flags = NATIVE_DRAW3D_TEXTURED;

	if ((texture->tpage & NATIVE_DRAW_LEVEL_TPAGE_BLEND_MASK) != NATIVE_DRAW_LEVEL_TPAGE_BLEND_MASK)
	{
		flags |= NATIVE_DRAW3D_SEMI_TRANS;
	}
	if ((block->draw_order_low & QUADBLOCK_DRAW_ORDER_LOW_DOUBLE_SIDED) != 0)
	{
		flags |= NATIVE_DRAW3D_DOUBLE_SIDED;
	}
	if (LevInstDef_IsSuperTurboVisualQuad(block))
	{
		flags |= NATIVE_DRAW3D_SUPER_TURBO_TINT;
	}
	return flags;
}

// Retail texture LOD (0x800a3e00): far, middle, near, then mosaic when the
// fourth entry is a texture rather than a pointer to mosaic records. Returns
// the records through `mosaic` when a near face has them.
static const struct TextureLayout *NativeDrawLevel_SelectMidTexture(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, int faceIndex,
                                                                    float maxDepth, const struct TextureLayout **mosaic)
{
	*mosaic = NULL;

	const struct TextureLayout *texture = DrawLevelOvr1P_ResolveTexturePointerChecked((uintptr_t)block->ptr_texture_mid[faceIndex]);
	if (texture == NULL)
	{
		return NULL;
	}

	const u32 mosaicWord = DrawLevelOvr1P_ReadPackedWord((const u8 *)texture + 0x24);
	if (maxDepth < (float)ctx->lod->textureLodDepthThreshold0)
	{
		texture++;
	}
	if (maxDepth < (float)ctx->lod->textureLodDepthThreshold1)
	{
		texture++;
	}
	if (maxDepth < (float)ctx->lod->topLevelNearDepthThreshold)
	{
		if (!DrawLevelOvr1P_TreatAsRetailNegativeTextureWord(mosaicWord))
		{
			texture++;
		}
		else if ((ctx->mosaic != NULL) && DrawLevelOvr1P_IsNativeLevelSpan(mosaicWord, (uintptr_t)ctx->mosaic->rows * 4 * 2 * sizeof(struct TextureLayout)))
		{
			*mosaic = (const struct TextureLayout *)(uintptr_t)mosaicWord;
		}
	}
	return texture;
}

static NativeDraw3DVertex NativeDrawLevel_Midpoint(const NativeDraw3DVertex *a, const NativeDraw3DVertex *b)
{
	NativeDraw3DVertex out;
	out.x = (a->x + b->x) * 0.5f;
	out.y = (a->y + b->y) * 0.5f;
	out.z = (a->z + b->z) * 0.5f;
	// Retail truncates colours and UVs.
	out.u = (u8)((a->u + b->u) >> 1);
	out.v = (u8)((a->v + b->v) >> 1);
	out.r = (u8)((a->r + b->r) >> 1);
	out.g = (u8)((a->g + b->g) >> 1);
	out.b = (u8)((a->b + b->b) >> 1);
	return out;
}

// Fills a subdivision frame from four corners and returns its parts.
static int NativeDrawLevel_Split(const NativeDraw3DVertex *corner, int split, NativeDraw3DVertex *grid, const u8 (**parts)[4])
{
	for (int k = 0; k < 4; k++)
	{
		grid[k] = corner[k];
	}
	grid[4] = NativeDrawLevel_Midpoint(&corner[0], &corner[1]);
	grid[8] = NativeDrawLevel_Midpoint(&corner[2], &corner[3]);
	if (split == NATIVE_DRAW_LEVEL_SPLIT_HALVES)
	{
		*parts = sNativeDrawLevelGridHalves;
		return 2;
	}

	grid[5] = NativeDrawLevel_Midpoint(&corner[0], &corner[2]);
	grid[7] = NativeDrawLevel_Midpoint(&corner[1], &corner[3]);
	grid[6] = (split == NATIVE_DRAW_LEVEL_SPLIT_CENTER) ? NativeDrawLevel_Midpoint(&grid[4], &grid[8]) : NativeDrawLevel_Midpoint(&corner[1], &corner[2]);
	*parts = sNativeDrawLevelGridFaces;
	return 4;
}

// Retail subdivides near faces twice and reloads UVs, CLUT and tpage from the
// mosaic record of each deepest cell. Away from Max detail, a part whose
// corners are not near enough for the full second split keeps the near
// texture, as retail's partial handlers do.
static void NativeDrawLevel_EmitMosaic(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, const NativeDraw3DVertex *corner,
                                       const struct TextureLayout *records, int swapUv, const NativeDraw3DMaterial *faceMaterial)
{
	const struct NativeDrawLevelMosaicShape *shape = ctx->mosaic;
	const int rowScale = (shape->split[1] == NATIVE_DRAW_LEVEL_SPLIT_HALVES) ? 1 : 2;
	NativeDraw3DVertex outer[NATIVE_DRAW_LEVEL_GRID_VERTICES];
	const u8(*outerParts)[4];
	const int outerCount = NativeDrawLevel_Split(corner, shape->split[0], outer, &outerParts);

	if (swapUv)
	{
		records += shape->rows * 4;
	}

	for (int i = 0; i < outerCount; i++)
	{
		NativeDraw3DVertex part[4];
		u32 nearSet = 0;
		for (int k = 0; k < 4; k++)
		{
			part[k] = outer[outerParts[i][k]];
			if (NativeDrawLevel_CameraDepth(ctx->view, &part[k]) < (float)ctx->lod->recursiveNearDepthThreshold)
			{
				nearSet |= 1u << k;
			}
		}

		if ((shape->fullNearSets & (1u << nearSet)) == 0)
		{
			NativeDraw3D_AddQuad(ctx->layer, &part[0], &part[1], &part[2], &part[3], faceMaterial);
			continue;
		}

		NativeDraw3DVertex inner[NATIVE_DRAW_LEVEL_GRID_VERTICES];
		const u8(*innerParts)[4];
		const int innerCount = NativeDrawLevel_Split(part, shape->split[1], inner, &innerParts);
		for (int j = 0; j < innerCount; j++)
		{
			const int column = (i & 1) * 2 + (j & 1);
			const int row = (i >> 1) * rowScale + (j >> 1);
			const struct TextureLayout *record = &records[row * 4 + column];
			if ((record->tpage & 0xfe00) != 0)
			{
				continue;
			}

			NativeDraw3DVertex cell[4];
			const u8 uv[4][2] = {{record->u0, record->v0}, {record->u1, record->v1}, {record->u2, record->v2}, {record->u3, record->v3}};
			for (int k = 0; k < 4; k++)
			{
				cell[k] = inner[innerParts[j][k]];
				cell[k].u = uv[k][0];
				cell[k].v = uv[k][1];
			}

			NativeDraw3DMaterial material = *faceMaterial;
			material.tpage = record->tpage;
			material.clut = record->clut;
			material.flags = NativeDrawLevel_BaseFlags(block, record) | (faceMaterial->flags & NATIVE_DRAW3D_REVERSE_WINDING);
			NativeDraw3D_AddQuad(ctx->layer, &cell[0], &cell[1], &cell[2], &cell[3], &material);
		}
	}
}

// The quadblock's nine vertices in retail grid order.
static void NativeDrawLevel_LoadGrid(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, NativeDraw3DVertex *grid)
{
	for (int i = 0; i < NATIVE_DRAW_LEVEL_GRID_VERTICES; i++)
	{
		const struct LevVertex *vertex = &ctx->vertices[block->index[i]];
		grid[i] = NativeDrawLevel_Vertex(vertex, vertex->color_hi);
	}
}

static void NativeDrawLevel_EmitFace(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, const NativeDraw3DVertex *grid, int faceIndex)
{
	const u32 field = (block->draw_order_low >> (8 + faceIndex * 5)) & 0x1f;
	const u32 tableWord = R226.scratchInitTable[field];
	// Bit 7: the face is a single triangle. Bit 23: UV pairs swap.
	const int triangle = (tableWord & 0x80u) != 0;
	const int count = triangle ? 3 : 4;
	NativeDraw3DVertex v[4];
	float maxDepth = 0.0f;

	for (int k = 0; k < count; k++)
	{
		const u32 shift = (tableWord >> ((3 - k) * 8)) & 0x1f;
		const u32 gridIndex = ((sNativeDrawLevelFaceSelectors[faceIndex] >> shift) & 0xff) / sizeof(struct DrawLevelOvr1PScratchVertex);
		if (gridIndex >= NATIVE_DRAW_LEVEL_GRID_VERTICES)
		{
			return;
		}
		v[k] = grid[gridIndex];
		const float depth = NativeDrawLevel_CameraDepth(ctx->view, &v[k]);
		if ((k == 0) || (depth > maxDepth))
		{
			maxDepth = depth;
		}
	}

	const struct TextureLayout *mosaic;
	const struct TextureLayout *texture = NativeDrawLevel_SelectMidTexture(ctx, block, faceIndex, maxDepth, &mosaic);
	if (texture == NULL)
	{
		return;
	}

	u16 uv[4];
	DrawLevelOvr1P_GetTextureUv(block, texture, uv);
	static const int swapped[4] = {1, 0, 3, 2};
	const int swapUv = (tableWord & 0x00800000u) != 0;

	for (int k = 0; k < count; k++)
	{
		NativeDrawLevel_SetUv(&v[k], uv[swapUv ? swapped[k] : k]);
	}

	NativeDraw3DMaterial material = {0};
	material.tpage = texture->tpage;
	material.clut = texture->clut;
	material.flags = NativeDrawLevel_BaseFlags(block, texture);
	if ((tableWord & 0x80000000u) != 0)
	{
		material.flags |= NATIVE_DRAW3D_REVERSE_WINDING;
	}
	NativeDrawLevel_SetDrawOrder(&material, (s8)(block->draw_order_high >> (faceIndex * 8)));

	if (mosaic != NULL)
	{
		// Retail triangles are quads whose last corner repeats the third.
		if (triangle)
		{
			v[3] = v[2];
		}
		NativeDrawLevel_EmitMosaic(ctx, block, v, mosaic, swapUv, &material);
	}
	else if (triangle)
	{
		NativeDraw3D_AddTriangle(ctx->layer, &v[0], &v[1], &v[2], &material);
	}
	else
	{
		NativeDraw3D_AddQuad(ctx->layer, &v[0], &v[1], &v[2], &v[3], &material);
	}
}

enum
{
	NATIVE_DRAW_LEVEL_PART_PRIMARY = 1,
	NATIVE_DRAW_LEVEL_PART_SECONDARY = 2,
	NATIVE_DRAW_LEVEL_PART_QUAD = 3,
};

struct NativeDrawLevelTransitionPart
{
	u8 index[4];
	// Which of the quad's triangles (0, 1, 2) and (1, 3, 2) to draw.
	u8 triangles;
};

// Retail full-dynamic partial helpers (0x800a15d4..0x800a170c), by near-corner
// set: low-texture quads over the morphed grid, some cut to one triangle.
static const struct NativeDrawLevelTransitionPart sNativeDrawLevelTransitionParts[16][4] = {
    [1] = {{{4, 1, 6, 3}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{5, 6, 2, 3}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{0, 4, 5, 6}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [2] = {{{4, 1, 6, 7}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{2, 0, 6, 4}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{2, 6, 3, 7}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [3] = {{{2, 6, 3, 7}, NATIVE_DRAW_LEVEL_PART_QUAD},
	       {{5, 6, 2, 8}, NATIVE_DRAW_LEVEL_PART_PRIMARY},
	       {{0, 4, 5, 6}, NATIVE_DRAW_LEVEL_PART_QUAD},
	       {{4, 1, 6, 7}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [4] = {{{5, 0, 6, 1}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{5, 6, 2, 8}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{1, 3, 6, 8}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [5] = {{{1, 3, 6, 8}, NATIVE_DRAW_LEVEL_PART_QUAD},
	       {{4, 1, 6, 7}, NATIVE_DRAW_LEVEL_PART_PRIMARY},
	       {{0, 4, 5, 6}, NATIVE_DRAW_LEVEL_PART_QUAD},
	       {{5, 6, 2, 8}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [8] = {{{0, 1, 6, 7}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{0, 6, 2, 8}, NATIVE_DRAW_LEVEL_PART_QUAD}, {{6, 7, 8, 3}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [10] = {{{2, 0, 6, 4}, NATIVE_DRAW_LEVEL_PART_QUAD},
	        {{5, 6, 2, 8}, NATIVE_DRAW_LEVEL_PART_SECONDARY},
	        {{4, 1, 6, 7}, NATIVE_DRAW_LEVEL_PART_QUAD},
	        {{6, 7, 8, 3}, NATIVE_DRAW_LEVEL_PART_QUAD}},
    [12] = {{{5, 0, 6, 1}, NATIVE_DRAW_LEVEL_PART_QUAD},
	        {{4, 1, 6, 7}, NATIVE_DRAW_LEVEL_PART_SECONDARY},
	        {{5, 6, 2, 8}, NATIVE_DRAW_LEVEL_PART_QUAD},
	        {{6, 7, 8, 3}, NATIVE_DRAW_LEVEL_PART_QUAD}},
};

// Middle vertices and the corners they morph between (6 uses the diagonal).
static const u8 sNativeDrawLevelMorphEdges[5][3] = {{4, 0, 1}, {5, 0, 2}, {6, 1, 2}, {7, 1, 3}, {8, 2, 3}};

// Retail 0x800a1408: past the fade start, a middle vertex slides to its edge
// midpoint and fades from its high to its low colour over 0x400 depth units.
static void NativeDrawLevel_MorphMiddle(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, NativeDraw3DVertex *grid)
{
	for (int i = 0; i < 5; i++)
	{
		const u8 *edge = sNativeDrawLevelMorphEdges[i];
		NativeDraw3DVertex *mid = &grid[edge[0]];
		const float factor = (NativeDrawLevel_CameraDepth(ctx->view, mid) - (float)ctx->lod->fullDynamicFadeDepthStart) * 4.0f;
		if (factor < 0.0f)
		{
			continue;
		}

		const float t = (factor < 4096.0f) ? factor / 4096.0f : 1.0f;
		const NativeDraw3DVertex *a = &grid[edge[1]];
		const NativeDraw3DVertex *b = &grid[edge[2]];
		const u8 *low = ctx->vertices[block->index[edge[0]]].color_lo;
		mid->x += ((a->x + b->x) * 0.5f - mid->x) * t;
		mid->y += ((a->y + b->y) * 0.5f - mid->y) * t;
		mid->z += ((a->z + b->z) * 0.5f - mid->z) * t;
		mid->r = (u8)((float)mid->r + ((float)low[0] - (float)mid->r) * t);
		mid->g = (u8)((float)mid->g + ((float)low[1] - (float)mid->g) * t);
		mid->b = (u8)((float)mid->b + ((float)low[2] - (float)mid->b) * t);
	}
}

// Far quadblocks (retail full-dynamic list). One quad with the low texture,
// unless a corner is within the morph distance: then the middle vertices morph
// and retail draws either the four regular faces (when the full handler is
// picked) or a partial topology with the low texture.
static void NativeDrawLevel_EmitFullDynamic(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block)
{
	const struct TextureLayout *texture = DrawLevelOvr1P_ResolveTexturePointerChecked((uintptr_t)block->ptr_texture_low);
	if (texture == NULL)
	{
		return;
	}

	NativeDraw3DVertex grid[NATIVE_DRAW_LEVEL_GRID_VERTICES];
	NativeDrawLevel_LoadGrid(ctx, block, grid);

	u32 nearSet = 0;
	for (int k = 0; k < 4; k++)
	{
		if (NativeDrawLevel_CameraDepth(ctx->view, &grid[k]) < (float)ctx->lod->depthScale)
		{
			nearSet |= 1u << k;
		}
	}

	if (nearSet != 0)
	{
		NativeDrawLevel_MorphMiddle(ctx, block, grid);
		if ((NATIVE_DRAW_LEVEL_FULL_NEAR_SETS & (1u << nearSet)) != 0)
		{
			for (int face = 0; face < 4; face++)
			{
				NativeDrawLevel_EmitFace(ctx, block, grid, face);
			}
			return;
		}
	}

	u16 uv[4];
	DrawLevelOvr1P_GetTextureUv(block, texture, uv);
	for (int k = 0; k < 4; k++)
	{
		NativeDrawLevel_SetUv(&grid[k], uv[k]);
	}

	NativeDraw3DMaterial material = {0};
	material.tpage = texture->tpage;
	material.clut = texture->clut;
	material.flags = NativeDrawLevel_BaseFlags(block, texture);
	NativeDrawLevel_SetDrawOrder(&material, (s8)(block->draw_order_low & 0xff));

	if (nearSet == 0)
	{
		NativeDraw3D_AddQuad(ctx->layer, &grid[0], &grid[1], &grid[2], &grid[3], &material);
		return;
	}

	// Retail averages the low-texture UVs for the middle vertices.
	for (int i = 0; i < 5; i++)
	{
		const u8 *edge = sNativeDrawLevelMorphEdges[i];
		grid[edge[0]].u = (u8)((grid[edge[1]].u + grid[edge[2]].u) >> 1);
		grid[edge[0]].v = (u8)((grid[edge[1]].v + grid[edge[2]].v) >> 1);
	}

	for (int i = 0; i < 4; i++)
	{
		const struct NativeDrawLevelTransitionPart *part = &sNativeDrawLevelTransitionParts[nearSet][i];
		const NativeDraw3DVertex *v[4] = {&grid[part->index[0]], &grid[part->index[1]], &grid[part->index[2]], &grid[part->index[3]]};
		if ((part->triangles & NATIVE_DRAW_LEVEL_PART_PRIMARY) != 0)
		{
			NativeDraw3D_AddTriangle(ctx->layer, v[0], v[1], v[2], &material);
		}
		if ((part->triangles & NATIVE_DRAW_LEVEL_PART_SECONDARY) != 0)
		{
			NativeDraw3D_AddTriangle(ctx->layer, v[1], v[3], v[2], &material);
		}
	}
}

// Retail 0x800a2234: water darkens to black between 0xc00 and 0x1000 units
// (octagonal distance) from the camera.
static void NativeDrawLevel_WaterColor(const struct NativeDrawLevelContext *ctx, const struct LevVertex *vertex, u8 *out)
{
	s32 deltaX = (s32)vertex->pos.x - ctx->center[0];
	s32 deltaZ = (s32)vertex->pos.z - ctx->center[2];
	deltaX = deltaX < 0 ? -deltaX : deltaX;
	deltaZ = deltaZ < 0 ? -deltaZ : deltaZ;
	const s32 major = deltaX > deltaZ ? deltaX : deltaZ;
	const s32 minor = deltaX > deltaZ ? deltaZ : deltaX;
	const s32 falloff = major + (minor >> 2) - 0x1000;

	if (falloff >= 0)
	{
		out[0] = out[1] = out[2] = 0;
		return;
	}

	const s32 factor = falloff * 4 + 0x1000;
	const float keep = factor < 0 ? 1.0f : 1.0f - (float)factor / 4096.0f;
	for (int i = 0; i < 3; i++)
	{
		out[i] = (u8)((float)vertex->color_hi[i] * keep);
	}
}

static void NativeDrawLevel_EmitWater(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block)
{
	const struct TextureLayout *envMap = ctx->waterEnvMap;
	if (envMap == NULL)
	{
		return;
	}

	NativeDraw3DMaterial material = {0};
	material.tpage = envMap->tpage;
	material.clut = envMap->clut;
	material.flags = NATIVE_DRAW3D_TEXTURED | NATIVE_DRAW3D_SEMI_TRANS | NATIVE_DRAW3D_DOUBLE_SIDED;

	NativeDraw3DVertex grid[NATIVE_DRAW_LEVEL_GRID_VERTICES];
	b32 lit[NATIVE_DRAW_LEVEL_GRID_VERTICES];
	for (int i = 0; i < NATIVE_DRAW_LEVEL_GRID_VERTICES; i++)
	{
		const struct LevVertex *vertex = &ctx->vertices[block->index[i]];
		u8 color[3];
		NativeDrawLevel_WaterColor(ctx, vertex, color);
		grid[i] = NativeDrawLevel_Vertex(vertex, color);
		// AnimateWater writes the env-map UV into color_lo.
		grid[i].u = vertex->color_lo[0];
		grid[i].v = vertex->color_lo[1];
		lit[i] = (color[0] | color[1] | color[2]) != 0;
	}

	for (int face = 0; face < 4; face++)
	{
		const u8 *f = sNativeDrawLevelGridFaces[face];
		NativeDrawLevel_SetDrawOrder(&material, (s8)(block->draw_order_high >> (face * 8)));
		// Retail skips triangles whose corners have all faded out.
		if (lit[f[0]] || lit[f[1]] || lit[f[2]])
		{
			NativeDraw3D_AddTriangle(ctx->layer, &grid[f[0]], &grid[f[1]], &grid[f[2]], &material);
		}
		if (lit[f[1]] || lit[f[2]] || lit[f[3]])
		{
			NativeDraw3D_AddTriangle(ctx->layer, &grid[f[1]], &grid[f[3]], &grid[f[2]], &material);
		}
	}
}

enum NativeDrawLevelKind
{
	NATIVE_DRAW_LEVEL_HIGH,
	NATIVE_DRAW_LEVEL_LOW,
	NATIVE_DRAW_LEVEL_WATER,
};

static void NativeDrawLevel_BspList(const struct NativeDrawLevelContext *ctx, const struct VisMemBspListNode *node, const int *visFaceList,
                                    enum NativeDrawLevelKind kind)
{
	const int expandedVisibility = NativeAspect_UsesExpandedVisibility();
	for (; node != NULL; node = node->next)
	{
		const struct BSP *bsp = node->bsp;
		const struct QuadBlock *block = bsp->data.leaf.ptrQuadBlockArray;
		s32 quadCount = bsp->data.leaf.numQuads;
		struct NativeDrawLevelVisibility vis;

		if (quadCount <= 0)
		{
			continue;
		}

		NativeDrawLevel_SeedVisibility(&vis, visFaceList, block);
		for (; quadCount > 0; quadCount--, block++)
		{
			if (!NativeDrawLevel_ConsumeVisibility(&vis) && !expandedVisibility)
			{
				continue;
			}

			// Static block bounds are a cheaper conservative gate than generating
			// every subdivision and letting individual triangles reject it later.
			// Water vertices move outside their authored bounds, so keep that path.
			if (expandedVisibility && kind != NATIVE_DRAW_LEVEL_WATER && !RenderLists_BoxPassesFrustum(ctx->pushBuffer, &block->bbox))
				continue;

			switch (kind)
			{
			case NATIVE_DRAW_LEVEL_HIGH:
			{
				NativeDraw3DVertex grid[NATIVE_DRAW_LEVEL_GRID_VERTICES];
				NativeDrawLevel_LoadGrid(ctx, block, grid);
				for (int face = 0; face < 4; face++)
				{
					NativeDrawLevel_EmitFace(ctx, block, grid, face);
				}
				break;
			}
			case NATIVE_DRAW_LEVEL_LOW:
				NativeDrawLevel_EmitFullDynamic(ctx, block);
				break;
			case NATIVE_DRAW_LEVEL_WATER:
				NativeDrawLevel_EmitWater(ctx, block);
				break;
			}
		}
	}
}

static void NativeDrawLevel_BuildView(const struct PushBuffer *pb, NativeDraw3DView *view)
{
	double rotation[9];
	double translation[3];

	// Precise when the camera recorded its unrounded transform this frame.
	NativePgxp_GetTransform(&pb->matrix_ViewProj, &pb->matrix_ViewProj.m[0][0], pb->matrix_ViewProj.t, rotation, translation);

	memset(view, 0, sizeof(*view));
	for (int i = 0; i < 9; i++)
	{
		view->rotation[i] = rotation[i] / 4096.0;
	}
	for (int i = 0; i < 3; i++)
	{
		view->translation[i] = translation[i];
	}
	view->projection = (float)pb->distanceToScreen_PREV;
	// GTE OFX/OFY are rect.w << 15 and rect.h << 15 (16.16 fixed point).
	view->centerX = (float)pb->rect.w * 0.5f;
	view->centerY = (float)pb->rect.h * 0.5f;
	view->width = (float)pb->rect.w;
	view->height = (float)pb->rect.h;
	view->mirror = gNativeMirrorModeRenderActive != 0;
}

// Links a layer marker after the sky's negative-D OT slots.
static void NativeDrawLevel_LinkLayer(struct PushBuffer *pb, struct PrimMem *primMem, int layer, int otIndex)
{
	DR_PSYX_DRAW3D *marker = (DR_PSYX_DRAW3D *)primMem->cursor;
	if ((u8 *)(marker + 1) > (u8 *)primMem->end)
	{
		return;
	}

	NativeDraw3D_SetMarker(marker, layer);
	AddPrim(&pb->ptrOT[otIndex], marker);
	primMem->cursor = marker + 1;
}

// Takes the retail LevRenderList like the overlays do.
static void NativeDrawLevel_Viewport(struct PushBuffer *pb, struct PrimMem *primMem, const struct mesh_info *mesh, const void *levRenderList,
                                     const int *visFaceList, const struct TextureLayout *waterEnvMap)
{
	if (pb && NativeAspect_UsesExpandedVisibility())
		RenderLists_PrepareNativeFrustum(pb);
	const struct DrawLevelOvr1PRenderList *renderList = levRenderList;

	if ((visFaceList == NULL) || (mesh->ptrQuadBlockArray == NULL) || (mesh->ptrVertexArray == NULL))
	{
		NativeDraw3D_ReportDiagnostic(NATIVE_DRAW3D_DIAG_LEVEL_INPUT, "NativeDrawLevel", pb->cameraID);
		return;
	}

	NativeDraw3DView view;
	NativeDrawLevel_BuildView(pb, &view);
	const int layer = NativeDraw3D_BeginLayer(&view);
	if (layer < 0)
	{
		return;
	}

	struct NativeDrawLevelContext ctx;
	ctx.layer = layer;
	ctx.vertices = mesh->ptrVertexArray;
	ctx.lod = CTR_SCRATCHPAD_PTR(struct MainRenderLevelGeometryScratch, 0);
	ctx.waterEnvMap = waterEnvMap;
	ctx.view = &view;
	ctx.pushBuffer = pb;
	for (int i = 0; i < 3; i++)
	{
		ctx.center[i] = (s16)((u16)(u8)pb->data6[i * 2] | ((u16)(u8)pb->data6[i * 2 + 1] << 8));
	}

	for (int slot = 0; slot < DRAW_LEVEL_OVR1P_RENDER_LIST_SLOT_COUNT; slot++)
	{
		const enum NativeDrawLevelKind kind = (slot == RENDER_LIST_SLOT_WATER) ? NATIVE_DRAW_LEVEL_WATER : NATIVE_DRAW_LEVEL_HIGH;
		ctx.mosaic = (slot < RENDER_LIST_SLOT_WATER) ? &sNativeDrawLevelMosaicShapes[slot] : NULL;
		NativeDrawLevel_BspList(&ctx, renderList->list[slot].bspListStart, visFaceList, kind);
	}
	ctx.mosaic = NULL;
	NativeDrawLevel_BspList(&ctx, renderList->bspListStart_FullDynamic, visFaceList, NATIVE_DRAW_LEVEL_LOW);

	NativeDraw3D_EndLayer(layer);
	NativeDrawLevel_LinkLayer(pb, primMem, layer, NATIVE_DRAW_LEVEL_MARKER_OT_INDEX);
}

#endif
