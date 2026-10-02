#include <common.h>

// NOTE: Native 3D level geometry (see native_draw3d.h). Walks the same BSP
// render lists and per-quadblock visibility bits as overlay 226, and makes the
// same per-face decisions (face permutation, UV flip, winding, texture LOD,
// semi-transparency and draw-order bias), but submits the exact source
// vertices once instead of projecting them. The host clips at the near plane,
// so retail near-camera subdivision and clip records are not needed. Shares
// overlay 226's texture helpers like overlays 227-229 do.

#if NATIVE_DRAW3D_SUPPORTED

extern int gNativeMirrorModeRenderActive;

enum
{
	NATIVE_DRAW_LEVEL_GRID_VERTICES = 9,
	// Retail OT slot for a viewport's world draw environment and sky; the
	// layer marker goes right after it.
	NATIVE_DRAW_LEVEL_MARKER_OT_INDEX = 0x3fe,
	// tpage blend bits 3 mark an opaque level face.
	NATIVE_DRAW_LEVEL_TPAGE_BLEND_MASK = 0x60,
};

// Retail 4x1 face selectors: byte lanes hold the 0x14-byte scratch record
// offsets of each face's grid vertices; the R226 table word picks the lanes.
static const u32 sNativeDrawLevelFaceSelectors[4] = {0x00506478, 0x5014788c, 0x647828a0, 0x788ca03c};

// Water faces use the grid order directly.
static const u8 sNativeDrawLevelGridFaces[4][4] = {{0, 4, 5, 6}, {4, 1, 6, 7}, {5, 6, 2, 8}, {6, 7, 8, 3}};

struct NativeDrawLevelContext
{
	int layer;
	const struct LevVertex *vertices;
	const struct MainRenderLevelGeometryScratch *lod;
	const struct TextureLayout *waterEnvMap;
	const NativeDraw3DView *view;
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

static float NativeDrawLevel_CameraDepth(const NativeDraw3DView *view, const struct LevVertex *vertex)
{
	const double *r = view->rotation;
	return (float)(r[6] * vertex->pos.x + r[7] * vertex->pos.y + r[8] * vertex->pos.z + view->translation[2]);
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
// fourth entry is a texture rather than a pointer to subdivided mosaics.
static const struct TextureLayout *NativeDrawLevel_SelectMidTexture(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, int faceIndex,
                                                                    float maxDepth)
{
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
	if ((maxDepth < (float)ctx->lod->topLevelNearDepthThreshold) && !DrawLevelOvr1P_TreatAsRetailNegativeTextureWord(mosaicWord))
	{
		texture++;
	}
	return texture;
}

static void NativeDrawLevel_EmitFace(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block, int faceIndex)
{
	const u32 field = (block->draw_order_low >> (8 + faceIndex * 5)) & 0x1f;
	const u32 tableWord = R226.scratchInitTable[field];
	// Bit 7: the face is a single triangle. Bit 23: UV pairs swap.
	const int triangle = (tableWord & 0x80u) != 0;
	const int count = triangle ? 3 : 4;
	const struct LevVertex *corner[4];
	float maxDepth = 0.0f;

	for (int k = 0; k < count; k++)
	{
		const u32 shift = (tableWord >> ((3 - k) * 8)) & 0x1f;
		const u32 gridIndex = ((sNativeDrawLevelFaceSelectors[faceIndex] >> shift) & 0xff) / sizeof(struct DrawLevelOvr1PScratchVertex);
		if (gridIndex >= NATIVE_DRAW_LEVEL_GRID_VERTICES)
		{
			return;
		}
		corner[k] = &ctx->vertices[block->index[gridIndex]];
		const float depth = NativeDrawLevel_CameraDepth(ctx->view, corner[k]);
		if ((k == 0) || (depth > maxDepth))
		{
			maxDepth = depth;
		}
	}

	const struct TextureLayout *texture = NativeDrawLevel_SelectMidTexture(ctx, block, faceIndex, maxDepth);
	if (texture == NULL)
	{
		return;
	}

	u16 uv[4];
	DrawLevelOvr1P_GetTextureUv(block, texture, uv);
	static const int swapped[4] = {1, 0, 3, 2};
	const int swapUv = (tableWord & 0x00800000u) != 0;

	NativeDraw3DVertex v[4];
	for (int k = 0; k < count; k++)
	{
		v[k] = NativeDrawLevel_Vertex(corner[k], corner[k]->color_hi);
		NativeDrawLevel_SetUv(&v[k], uv[swapUv ? swapped[k] : k]);
	}

	NativeDraw3DMaterial material;
	material.tpage = texture->tpage;
	material.clut = texture->clut;
	material.flags = NativeDrawLevel_BaseFlags(block, texture);
	if ((tableWord & 0x80000000u) != 0)
	{
		material.flags |= NATIVE_DRAW3D_REVERSE_WINDING;
	}
	material.depthBias = (s8)(block->draw_order_high >> (faceIndex * 8));

	if (triangle)
	{
		NativeDraw3D_AddTriangle(ctx->layer, &v[0], &v[1], &v[2], &material);
	}
	else
	{
		NativeDraw3D_AddQuad(ctx->layer, &v[0], &v[1], &v[2], &v[3], &material);
	}
}

// Far quadblocks (retail full-dynamic list): one quad with the low texture.
static void NativeDrawLevel_EmitLowQuad(const struct NativeDrawLevelContext *ctx, const struct QuadBlock *block)
{
	const struct TextureLayout *texture = DrawLevelOvr1P_ResolveTexturePointerChecked((uintptr_t)block->ptr_texture_low);
	if (texture == NULL)
	{
		return;
	}

	u16 uv[4];
	DrawLevelOvr1P_GetTextureUv(block, texture, uv);

	NativeDraw3DVertex v[4];
	for (int k = 0; k < 4; k++)
	{
		const struct LevVertex *vertex = &ctx->vertices[block->index[k]];
		v[k] = NativeDrawLevel_Vertex(vertex, vertex->color_hi);
		NativeDrawLevel_SetUv(&v[k], uv[k]);
	}

	NativeDraw3DMaterial material;
	material.tpage = texture->tpage;
	material.clut = texture->clut;
	material.flags = NativeDrawLevel_BaseFlags(block, texture);
	material.depthBias = (s8)(block->draw_order_low & 0xff);
	NativeDraw3D_AddQuad(ctx->layer, &v[0], &v[1], &v[2], &v[3], &material);
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

	NativeDraw3DMaterial material;
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
		material.depthBias = (s8)(block->draw_order_high >> (face * 8));
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
			if (!NativeDrawLevel_ConsumeVisibility(&vis))
			{
				continue;
			}

			switch (kind)
			{
			case NATIVE_DRAW_LEVEL_HIGH:
				for (int face = 0; face < 4; face++)
				{
					NativeDrawLevel_EmitFace(ctx, block, face);
				}
				break;
			case NATIVE_DRAW_LEVEL_LOW:
				NativeDrawLevel_EmitLowQuad(ctx, block);
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

// Links a layer marker just after the viewport's draw environment and sky.
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
	const struct DrawLevelOvr1PRenderList *renderList = levRenderList;

	if ((visFaceList == NULL) || (mesh->ptrQuadBlockArray == NULL) || (mesh->ptrVertexArray == NULL))
	{
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
	for (int i = 0; i < 3; i++)
	{
		ctx.center[i] = (s16)((u16)(u8)pb->data6[i * 2] | ((u16)(u8)pb->data6[i * 2 + 1] << 8));
	}

	for (int slot = 0; slot < DRAW_LEVEL_OVR1P_RENDER_LIST_SLOT_COUNT; slot++)
	{
		const enum NativeDrawLevelKind kind = (slot == RENDER_LIST_SLOT_WATER) ? NATIVE_DRAW_LEVEL_WATER : NATIVE_DRAW_LEVEL_HIGH;
		NativeDrawLevel_BspList(&ctx, renderList->list[slot].bspListStart, visFaceList, kind);
	}
	NativeDrawLevel_BspList(&ctx, renderList->bspListStart_FullDynamic, visFaceList, NATIVE_DRAW_LEVEL_LOW);

	NativeDraw3D_EndLayer(layer);
	NativeDrawLevel_LinkLayer(pb, primMem, layer, NATIVE_DRAW_LEVEL_MARKER_OT_INDEX);
}

#endif
