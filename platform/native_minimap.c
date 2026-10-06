#include <common.h>
#include "platform/native_minimap.h"
#include "platform/native_pgxp.h"
#include "platform/native_assets.h"
#include "platform/native_disc_image.h"
#include "platform/native_renderer.h"
#include "platform/native_path.h"
#include <SDL3/SDL.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void NativeMinimap_Project(const struct UIMap *map, double x, double z, double *mapX, double *mapY)
{
	const double rangeX = (double)map->worldEndX - map->worldStartX;
	const double rangeZ = (double)map->worldEndY - map->worldStartY;
	if (rangeX == 0 || rangeZ == 0)
	{
		*mapX = *mapY = 0;
		return;
	}
	if (map->mode == 0 || map->mode == 2)
	{
		const double sign = map->mode == 0 ? 1.0 : -1.0;
		*mapX = (sign * x * map->iconSizeX / rangeX);
		*mapY = (sign * z * map->iconSizeY * 2 / rangeZ);
	}
	else
	{
		*mapX = ((map->mode == 1 ? -1.0 : 1.0) * z * map->iconSizeX / rangeZ);
		*mapY = ((map->mode == 1 ? 1.0 : -1.0) * x * map->iconSizeY * 2 / rangeX);
	}
}

int NativeMinimap_GetHubRoutePosition(int hubLevelID, int triggerID, s32 worldPosition[3])
{
	if (!worldPosition)
		return 0;
	// Hub triggers sit inside the visible road ends. Anchor each arrow to the
	// midpoint of the corresponding outer ground edge so it marks the route
	// leaving the hub. These X/Z points are measured from the high-detail LEV
	// triangles and remain valid for every map orientation and HUD placement.
	static const struct
	{
		s16 levelID, triggerID;
		s16 x, z;
	} routeEnds[] = {
	    {GEM_STONE_VALLEY, 1, -14698, -583}, {GEM_STONE_VALLEY, 2, -1536, 17664}, {N_SANITY_BEACH, 1, -10858, 5561}, {N_SANITY_BEACH, 2, -6144, -8448},
	    {THE_LOST_RUINS, 1, -9216, 16128},   {THE_LOST_RUINS, 2, 9658, 7337},     {GLACIER_PARK, 1, -12288, -12257}, {GLACIER_PARK, 2, 10752, 14592},
	    {GLACIER_PARK, 3, 5117, -15906},     {CITADEL_CITY, 1, 1277, -9762},
	};
	if (triggerID < 1 || triggerID > 3)
		return 0;
	for (int i = 0; i < (int)(sizeof(routeEnds) / sizeof(routeEnds[0])); i++)
	{
		if (routeEnds[i].levelID != hubLevelID || routeEnds[i].triggerID != triggerID)
			continue;
		worldPosition[0] = routeEnds[i].x;
		worldPosition[1] = 0;
		worldPosition[2] = routeEnds[i].z;
		return 1;
	}
	return 0;
}

#ifndef __vita__
// Eight samples per legacy HUD pixel in each direction. Texture coordinates
// use a virtual 254x254 extent; the native override permits larger textures.
#define NATIVE_MINIMAP_SAMPLES     8
#define NATIVE_MINIMAP_MAX_TEXTURE 1024
#define NATIVE_MINIMAP_UV_EXTENT   254
#define NATIVE_MINIMAP_LEVEL_COUNT (CITADEL_CITY + 1)

struct NativeMinimapImage
{
	u32 texture;
	float left, top, width, height;
	int pixelWidth, pixelHeight;
	u32 outlineTexture;
};

// Versioned, compressed RGBA files are shared by previews and loaded geometry.
#define NATIVE_MINIMAP_CACHE_VERSION 4u
#define NATIVE_MINIMAP_CACHE_MAGIC   0x50414d4eu
struct NativeMinimapCacheHeader
{
	u32 magic, version;
	u64 key, checksum;
	float left, top, width, height;
	u32 pixelWidth, pixelHeight;
};
CTR_STATIC_ASSERT(sizeof(struct NativeMinimapCacheHeader) == 48);
struct NativeMinimapCacheIndex
{
	u32 version, reserved;
	u64 source, key;
};

static u64 NativeMinimap_Hash(u64 hash, const void *data, size_t size)
{
	const u8 *bytes = data;
	for (size_t i = 0; i < size; i++)
		hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
	return hash;
}

static u64 NativeMinimap_GeometryKey(const struct mesh_info *mesh, const struct UIMap *map)
{
	struct UIMap projection = *map;
	projection.iconStartX = projection.iconStartY = 0;
	u64 hash = NativeMinimap_Hash(UINT64_C(14695981039346656037), &projection, sizeof(projection));
	for (int q = 0; q < mesh->numQuadBlock; q++)
	{
		const struct QuadBlock *block = &P32_GET(struct QuadBlock *const, mesh->ptrQuadBlockArray)[q];
		if (!(block->quadFlags & QUADBLOCK_FLAG_GROUND) ||
		    (block->quadFlags & (QUADBLOCK_FLAG_NO_COLLISION_RESPONSE | QUADBLOCK_FLAG_KILL_PLANE | QUADBLOCK_FLAG_TRIGGER)))
			continue;
		const u32 hasSecondTriangle = block->index[2] != block->index[3];
		hash = NativeMinimap_Hash(hash, &hasSecondTriangle, sizeof(hasSecondTriangle));
		for (int v = 0; v < 9; v++)
		{
			if (block->index[v] >= mesh->numVertex)
				return 0;
			hash = NativeMinimap_Hash(hash, &P32_GET(struct LevVertex *const, mesh->ptrVertexArray)[block->index[v]].pos, sizeof(SVec3));
		}
	}
	return hash;
}

static int NativeMinimap_CachePath(const char *name, char path[1024], int create)
{
	char directory[1024];
	if (!NativePath_Join(directory, sizeof(directory), NativeStr8_FromCString(NativeAssets_GetBaseDir()), NATIVE_STR8_LIT(".cache/modern-map")))
		return 0;
	if (create && !SDL_CreateDirectory(directory))
		return 0;
	return NativePath_Join(path, 1024, NativeStr8_FromCString(directory), NativeStr8_FromCString(name));
}

static u8 *NativeMinimap_LoadCache(u64 key, struct NativeMinimapImage *image)
{
	char name[48], path[1024];
	snprintf(name, sizeof(name), "%016llx.map", (unsigned long long)key);
	if (!key || !NativeMinimap_CachePath(name, path, 0))
		return NULL;
	FILE *file = fopen(path, "rb");
	if (!file)
		return NULL;
	struct NativeMinimapCacheHeader header;
	u8 *pixels = NULL;
	if (fread(&header, sizeof(header), 1, file) != 1 || header.magic != NATIVE_MINIMAP_CACHE_MAGIC || header.version != NATIVE_MINIMAP_CACHE_VERSION ||
	    header.key != key || !header.pixelWidth || !header.pixelHeight || header.pixelWidth > NATIVE_MINIMAP_MAX_TEXTURE ||
	    header.pixelHeight > NATIVE_MINIMAP_MAX_TEXTURE || !isfinite(header.left) || !isfinite(header.top) || !isfinite(header.width) ||
	    !isfinite(header.height) || header.width <= 0 || header.height <= 0)
		goto done;
	const size_t count = (size_t)header.pixelWidth * header.pixelHeight;
	pixels = malloc(count * 4);
	if (!pixels)
		goto done;
	size_t written = 0;
	while (written < count)
	{
		u32 run[2];
		if (fread(run, sizeof(run), 1, file) != 1 || !run[0] || run[0] > count - written)
			goto invalid;
		for (u32 i = 0; i < run[0]; i++)
			memcpy(pixels + (written + i) * 4, &run[1], 4);
		written += run[0];
	}
	if (fgetc(file) != EOF || ferror(file) || NativeMinimap_Hash(UINT64_C(14695981039346656037), pixels, count * 4) != header.checksum)
		goto invalid;
	image->left = header.left;
	image->top = header.top;
	image->width = header.width;
	image->height = header.height;
	image->pixelWidth = (int)header.pixelWidth;
	image->pixelHeight = (int)header.pixelHeight;
	goto done;
invalid:
	free(pixels);
	pixels = NULL;
done:
	fclose(file);
	return pixels;
}

static int NativeMinimap_SaveCache(u64 key, const struct NativeMinimapImage *image, const u8 *pixels)
{
	char name[48], path[1024], temporary[1040];
	snprintf(name, sizeof(name), "%016llx.map", (unsigned long long)key);
	if (!NativeMinimap_CachePath(name, path, 1))
		return 0;
	snprintf(temporary, sizeof(temporary), "%s.tmp", path);
	FILE *file = fopen(temporary, "wb");
	if (!file)
		return 0;
	const size_t count = (size_t)image->pixelWidth * image->pixelHeight;
	const struct NativeMinimapCacheHeader header = {NATIVE_MINIMAP_CACHE_MAGIC,
	                                                NATIVE_MINIMAP_CACHE_VERSION,
	                                                key,
	                                                NativeMinimap_Hash(UINT64_C(14695981039346656037), pixels, count * 4),
	                                                image->left,
	                                                image->top,
	                                                image->width,
	                                                image->height,
	                                                (u32)image->pixelWidth,
	                                                (u32)image->pixelHeight};
	int ok = fwrite(&header, sizeof(header), 1, file) == 1;
	for (size_t i = 0; ok && i < count;)
	{
		u32 run[2] = {1, 0};
		memcpy(&run[1], pixels + i * 4, 4);
		while (i + run[0] < count && memcmp(pixels + i * 4, pixels + (i + run[0]) * 4, 4) == 0)
			run[0]++;
		ok = fwrite(run, sizeof(run), 1, file) == 1;
		i += run[0];
	}
	if (fclose(file) != 0)
		ok = 0;
	if (ok)
	{
		SDL_RemovePath(path);
		ok = SDL_RenamePath(temporary, path);
	}
	if (!ok)
		SDL_RemovePath(temporary);
	return ok;
}

// Fast preview lookup: source timestamps are nanoseconds, supplied by SDL.
static u64 NativeMinimap_SourceKey(void)
{
	char path[1024];
	SDL_PathInfo info;
	const char *source = "BIGFILE.BIG";
	if (!NativeAssets_ResolvePath(source, path, sizeof(path)))
		source = "ctr-u.bin";
	if (!NativeAssets_ResolvePath(source, path, sizeof(path)) || !SDL_GetPathInfo(path, &info))
		return 0;
	u64 key = NativeMinimap_Hash(UINT64_C(14695981039346656037), source, strlen(source));
	key = NativeMinimap_Hash(key, &info.size, sizeof(info.size));
	return NativeMinimap_Hash(key, &info.modify_time, sizeof(info.modify_time));
}

static u64 NativeMinimap_ReadIndex(int levelID, int lod, u64 source)
{
	char name[48], path[1024];
	snprintf(name, sizeof(name), "level-%02d-%d.index", levelID, lod);
	if (!source || !NativeMinimap_CachePath(name, path, 0))
		return 0;
	FILE *file = fopen(path, "rb");
	if (!file)
		return 0;
	struct NativeMinimapCacheIndex index;
	const int ok = fread(&index, sizeof(index), 1, file) == 1 && index.version == NATIVE_MINIMAP_CACHE_VERSION && index.source == source;
	fclose(file);
	return ok ? index.key : 0;
}

static void NativeMinimap_WriteIndex(int levelID, int lod, u64 source, u64 key)
{
	char name[48], path[1024];
	snprintf(name, sizeof(name), "level-%02d-%d.index", levelID, lod);
	if (!source || !NativeMinimap_CachePath(name, path, 1))
		return;
	FILE *file = fopen(path, "wb");
	if (!file)
		return;
	const struct NativeMinimapCacheIndex index = {NATIVE_MINIMAP_CACHE_VERSION, 0, source, key};
	fwrite(&index, sizeof(index), 1, file);
	fclose(file);
}

static struct NativeMinimapImage s_nativeMinimapLive;
static struct NativeMinimapImage s_nativeMinimapPreviews[NATIVE_MINIMAP_LEVEL_COUNT];
static u8 s_nativeMinimapPreviewAttempted[NATIVE_MINIMAP_LEVEL_COUNT];
static const struct mesh_info *s_nativeMinimapMesh;
static struct UIMap s_nativeMinimapProjection;
static u32 s_nativeMinimapEpoch = 1;
static u32 s_nativeMinimapBuiltEpoch;

// Matches COLL_FIXED_QUADBLK_TestTriangles' high-detail collision topology,
// including the four-triangle case where index[2] == index[3].
static const u8 s_nativeMinimapTriangles[8][3] = {
    {0, 4, 5}, {4, 6, 5}, {6, 4, 1}, {5, 6, 2}, {8, 6, 7}, {7, 3, 8}, {1, 7, 6}, {2, 6, 8},
};

static int NativeMinimap_IsGround(const struct QuadBlock *block)
{
	return (block->quadFlags & QUADBLOCK_FLAG_GROUND) &&
	       !(block->quadFlags & (QUADBLOCK_FLAG_NO_COLLISION_RESPONSE | QUADBLOCK_FLAG_KILL_PLANE | QUADBLOCK_FLAG_TRIGGER));
}

static double NativeMinimap_Edge(double ax, double ay, double bx, double by, double x, double y)
{
	return (bx - ax) * (y - ay) - (by - ay) * (x - ax);
}

struct NativeMinimapSample
{
	float height, dx, dy, lower;
};

// Subpixel distance to an edge. Eight-neighbour sweeps also carry the height
// of a bridge edge so its stroke only extends into the upper road.
static void NativeMinimap_DistanceField(float *distance, float *edgeHeight, int width, int height)
{
	for (int pass = 0; pass < 2; pass++)
	{
		const int step = pass ? -1 : 1;
		for (int y = pass ? height - 1 : 0; y >= 0 && y < height; y += step)
			for (int x = pass ? width - 1 : 0; x >= 0 && x < width; x += step)
			{
				const int i = y * width + x;
				const int neighbourX[4] = {x - step, x - step, x, x + step};
				const int neighbourY[4] = {y, y - step, y - step, y - step};
				for (int n = 0; n < 4; n++)
				{
					const int nx = neighbourX[n], ny = neighbourY[n];
					if (nx < 0 || nx >= width || ny < 0 || ny >= height)
						continue;
					const int j = ny * width + nx;
					const float candidate = distance[j] + ((n == 1 || n == 3) ? 1.41421356f : 1.0f);
					if (candidate < distance[i])
					{
						distance[i] = candidate;
						if (edgeHeight)
							edgeHeight[i] = edgeHeight[j];
					}
				}
			}
	}
}

static int NativeMinimap_HeightEdge(const struct NativeMinimapSample *upper, const struct NativeMinimapSample *other, int dx, int dy)
{
	const float delta = other->height - upper->height;
	// Compare the neighbouring planes, not just elevation: a steep continuous
	// slope must not acquire stripes. Only the upper side receives the stroke.
	return upper->lower != -FLT_MAX && upper->height - upper->lower > 48.0f && other->height != -FLT_MAX && delta < -48.0f &&
	       fabsf(delta - upper->dx * dx - upper->dy * dy) > 48.0f && fabsf(delta - other->dx * dx - other->dy * dy) > 48.0f;
}

static void NativeMinimap_RasterTriangle(const struct LevVertex *a, const struct LevVertex *b, const struct LevVertex *c, const struct UIMap *map,
                                         const struct NativeMinimapImage *image, struct NativeMinimapSample *samples)
{
	// Reject walls and nearly vertical faces, even if flagged as ground.
	const double abX = b->pos.x - a->pos.x, abY = b->pos.y - a->pos.y, abZ = b->pos.z - a->pos.z;
	const double acX = c->pos.x - a->pos.x, acY = c->pos.y - a->pos.y, acZ = c->pos.z - a->pos.z;
	const double nx = abY * acZ - abZ * acY, ny = abZ * acX - abX * acZ, nz = abX * acY - abY * acX;
	if (ny * ny < 0.0225 * (nx * nx + ny * ny + nz * nz))
		return;
	float px[3], py[3];
	const struct LevVertex *vertices[3] = {a, b, c};
	for (int i = 0; i < 3; i++)
	{
		double x, y;
		NativeMinimap_Project(map, vertices[i]->pos.x, vertices[i]->pos.z, &x, &y);
		px[i] = (float)(x - image->left) * image->pixelWidth / image->width;
		py[i] = (float)(y - image->top) * image->pixelHeight / image->height;
	}
	const double area = NativeMinimap_Edge(px[0], py[0], px[1], py[1], px[2], py[2]);
	if (fabs(area) < 1e-8)
		return;
	const double slopeX = ((b->pos.y - a->pos.y) * (py[2] - py[0]) - (c->pos.y - a->pos.y) * (py[1] - py[0])) / area;
	const double slopeY = ((px[1] - px[0]) * (c->pos.y - a->pos.y) - (px[2] - px[0]) * (b->pos.y - a->pos.y)) / area;
	int x0 = (int)floor(fmin(px[0], fmin(px[1], px[2])));
	int y0 = (int)floor(fmin(py[0], fmin(py[1], py[2])));
	int x1 = (int)ceil(fmax(px[0], fmax(px[1], px[2])));
	int y1 = (int)ceil(fmax(py[0], fmax(py[1], py[2])));
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 >= image->pixelWidth)
		x1 = image->pixelWidth - 1;
	if (y1 >= image->pixelHeight)
		y1 = image->pixelHeight - 1;
	for (int y = y0; y <= y1; y++)
		for (int x = x0; x <= x1; x++)
		{
			const double u = NativeMinimap_Edge(px[1], py[1], px[2], py[2], x + 0.5, y + 0.5) / area;
			const double v = NativeMinimap_Edge(px[2], py[2], px[0], py[0], x + 0.5, y + 0.5) / area;
			const double w = 1.0 - u - v;
			if (u < -1e-7 || v < -1e-7 || w < -1e-7)
				continue;
			const float height = (float)(u * a->pos.y + v * b->pos.y + w * c->pos.y);
			struct NativeMinimapSample *sample = &samples[y * image->pixelWidth + x];
			if (height > sample->height)
			{
				if (sample->height != -FLT_MAX && height - sample->height > 48)
					sample->lower = fmaxf(sample->lower, sample->height);
				sample->height = height;
				sample->dx = (float)slopeX;
				sample->dy = (float)slopeY;
			}
			else if (sample->height - height > 48)
			{
				sample->lower = fmaxf(sample->lower, height);
			}
		}
}

// CPU-only generation: highest eligible ground at each sample is equivalent
// to a downward ray against the ground triangles. Scenery roofs do not hide
// tunnel roads because they are not eligible collision ground.
static u8 *NativeMinimap_BuildPixels(const struct mesh_info *mesh, const struct UIMap *map, struct NativeMinimapImage *image)
{
	if (!mesh || !P32_GET(struct QuadBlock *const, mesh->ptrQuadBlockArray) || !P32_GET(struct LevVertex *const, mesh->ptrVertexArray) ||
	    mesh->numQuadBlock <= 0 || mesh->numQuadBlock > 65536 || mesh->numVertex <= 0 || mesh->numVertex > 65536 || map->worldEndX == map->worldStartX ||
	    map->worldEndY == map->worldStartY || map->iconSizeX <= 0 || map->iconSizeY <= 0)
		return NULL;
	float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
	for (int q = 0; q < mesh->numQuadBlock; q++)
	{
		const struct QuadBlock *block = &P32_GET(struct QuadBlock *const, mesh->ptrQuadBlockArray)[q];
		if (!NativeMinimap_IsGround(block))
			continue;
		for (int i = 0; i < 9; i++)
		{
			if (block->index[i] >= mesh->numVertex)
				return NULL;
			const struct LevVertex *vertex = &P32_GET(struct LevVertex *const, mesh->ptrVertexArray)[block->index[i]];
			double x, y;
			NativeMinimap_Project(map, vertex->pos.x, vertex->pos.z, &x, &y);
			left = fminf(left, x);
			right = fmaxf(right, x);
			top = fminf(top, y);
			bottom = fmaxf(bottom, y);
		}
	}
	if (left == FLT_MAX || right <= left || bottom <= top)
		return NULL;
	image->left = floorf(left) - 3;
	image->top = floorf(top) - 3;
	image->width = ceilf(right) + 3 - image->left;
	image->height = ceilf(bottom) + 3 - image->top;
	const float samples = fminf(NATIVE_MINIMAP_SAMPLES, NATIVE_MINIMAP_MAX_TEXTURE / fmaxf(image->width, image->height));
	image->pixelWidth = (int)ceilf(image->width * samples);
	image->pixelHeight = (int)ceilf(image->height * samples);
	const int count = image->pixelWidth * image->pixelHeight;
	struct NativeMinimapSample *heights = calloc((size_t)count, sizeof(*heights));
	u8 *pixels = calloc((size_t)count, 4);
	if (!heights || !pixels)
	{
		free(heights);
		free(pixels);
		return NULL;
	}
	for (int i = 0; i < count; i++)
		heights[i].height = heights[i].lower = -FLT_MAX;
	for (int q = 0; q < mesh->numQuadBlock; q++)
	{
		const struct QuadBlock *block = &P32_GET(struct QuadBlock *const, mesh->ptrQuadBlockArray)[q];
		if (!NativeMinimap_IsGround(block))
			continue;
		const int triangles = block->index[2] == block->index[3] ? 4 : 8;
		for (int t = 0; t < triangles; t++)
		{
			const u8 *indices = s_nativeMinimapTriangles[t];
			NativeMinimap_RasterTriangle(&P32_GET(struct LevVertex *const, mesh->ptrVertexArray)[block->index[indices[0]]],
			                             &P32_GET(struct LevVertex *const, mesh->ptrVertexArray)[block->index[indices[1]]],
			                             &P32_GET(struct LevVertex *const, mesh->ptrVertexArray)[block->index[indices[2]]], map, image, heights);
		}
	}
	float minHeight = FLT_MAX, maxHeight = -FLT_MAX;
	for (int i = 0; i < count; i++)
		if (heights[i].height != -FLT_MAX)
		{
			minHeight = fminf(minHeight, heights[i].height);
			maxHeight = fmaxf(maxHeight, heights[i].height);
		}
	if (minHeight == FLT_MAX)
	{
		free(heights);
		free(pixels);
		return NULL;
	}
	float *outside = malloc((size_t)count * sizeof(float));
	float *edges = malloc((size_t)count * sizeof(float));
	float *edgeHeight = calloc((size_t)count, sizeof(float));
	if (!outside || !edges || !edgeHeight)
	{
		free(outside);
		free(edges);
		free(edgeHeight);
		free(heights);
		free(pixels);
		return NULL;
	}
	for (int y = 0; y < image->pixelHeight; y++)
		for (int x = 0; x < image->pixelWidth; x++)
		{
			const int i = y * image->pixelWidth + x;
			outside[i] = heights[i].height != -FLT_MAX ? 0 : 1e10f;
			edges[i] = 1e10f;
			if (heights[i].height == -FLT_MAX)
				continue;
			const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
			for (int n = 0; n < 4; n++)
			{
				const int nx = x + dx[n], ny = y + dy[n];
				if (nx >= 0 && nx < image->pixelWidth && ny >= 0 && ny < image->pixelHeight &&
				    NativeMinimap_HeightEdge(&heights[i], &heights[ny * image->pixelWidth + nx], dx[n], dy[n]))
				{
					edges[i] = 0;
					edgeHeight[i] = heights[i].height;
					break;
				}
			}
		}
	NativeMinimap_DistanceField(outside, NULL, image->pixelWidth, image->pixelHeight);
	NativeMinimap_DistanceField(edges, edgeHeight, image->pixelWidth, image->pixelHeight);
	for (int i = 0; i < count; i++)
	{
		if (heights[i].height != -FLT_MAX)
		{
			const float shade = maxHeight > minHeight ? 192 + 63 * (heights[i].height - minHeight) / (maxHeight - minHeight) : 255;
			const float line = fabsf(heights[i].height - edgeHeight[i]) < 96 ? fmaxf(0, fminf(1, samples * 0.6f + 0.5f - edges[i])) : 0;
			pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = (u8)(shade * (1 - line));
			pixels[i * 4 + 3] = 255;
		}
		else
		{
			pixels[i * 4 + 3] = (u8)(255 * fmaxf(0, fminf(1, samples * 0.7f + 0.5f - outside[i])));
		}
	}
	free(outside);
	free(edges);
	free(edgeHeight);
	free(heights);
	return pixels;
}

static u8 *NativeMinimap_GetPixels(const struct mesh_info *mesh, const struct UIMap *map, struct NativeMinimapImage *image, int generate)
{
	const u64 key = NativeMinimap_GeometryKey(mesh, map);
	if (!key)
		return NULL;
	u8 *pixels = NativeMinimap_LoadCache(key, image);
	if (!pixels && generate)
	{
		pixels = NativeMinimap_BuildPixels(mesh, map, image);
		if (pixels)
			NativeMinimap_SaveCache(key, image, pixels);
	}
	return pixels;
}

static int NativeMinimap_Build(const struct mesh_info *mesh, const struct UIMap *map, struct NativeMinimapImage *image)
{
	// Drawing never rasterises collision data, including during startup.
	u8 *pixels = NativeMinimap_GetPixels(mesh, map, image, 0);
	if (!pixels)
		return 0;
	image->texture = NativeRenderer_CreateMinimapTexture(image->pixelWidth, image->pixelHeight, pixels);
	free(pixels);
	return image->texture != 0;
}

// Read an isolated LEV for previews. Do not use the game's CD queue or
// relocate pointers into its memory packs. All disk offsets are range checked.
static u8 *NativeMinimap_ReadLevelPixels(int levelID, int lod, struct NativeMinimapImage *image, int generate)
{
	const u64 source = NativeMinimap_SourceKey();
	const u64 cachedKey = NativeMinimap_ReadIndex(levelID, lod, source);
	u8 *cached = NativeMinimap_LoadCache(cachedKey, image);
	if (cached || !generate)
		return cached;
	const int index = LOAD_GetBigfileIndex((u32)levelID, lod, LVI_LEV);
	const size_t headerSize = LOAD_BIGFILE_HEADER_SECTORS * LOAD_CD_DATA_SECTOR_SIZE;
	u8 *headerBytes = malloc(headerSize), *bytes = NULL;
	FILE *file = NULL;
	u8 *result = NULL;
	struct NativeDiscImageFile disc;
	if (!headerBytes)
		return NULL;
	file = NativeAssets_OpenHostBigfile("rb");
	if (file)
	{
		if (fread(headerBytes, 1, headerSize, file) != headerSize)
			goto done;
	}
	else if (!NativeDiscImage_FindFile("BIGFILE.BIG", &disc) || !NativeDiscImage_ReadDataSectors(&disc, 0, LOAD_BIGFILE_HEADER_SECTORS, headerBytes))
		goto done;
	const struct BigHeader *header = (const struct BigHeader *)headerBytes;
	if (index < 0 || index >= header->numEntry || header->numEntry < 0 || (size_t)header->numEntry > (headerSize - sizeof(*header)) / sizeof(struct BigEntry))
		goto done;
	const struct BigEntry *entries = BIG_GETENTRY(header);
	const struct BigEntry *entry = &entries[index];
	if (entry->offset < 0 || entry->size < 4 + (int)sizeof(struct Level) || entry->size > 32 * 1024 * 1024)
		goto done;
	const u32 sectors = ((u32)entry->size + LOAD_CD_DATA_SECTOR_ROUND_MASK) >> LOAD_CD_DATA_SECTOR_SHIFT;
	bytes = malloc((size_t)sectors * LOAD_CD_DATA_SECTOR_SIZE);
	if (!bytes)
		goto done;
	if (file)
	{
		const u32 byteOffset = (u32)entry->offset * LOAD_CD_DATA_SECTOR_SIZE;
		if ((u32)entry->offset > 0x7fffffffu / LOAD_CD_DATA_SECTOR_SIZE || fseek(file, (long)byteOffset, SEEK_SET) != 0 ||
		    fread(bytes, 1, (size_t)entry->size, file) != (size_t)entry->size)
			goto done;
	}
	else if (!NativeDiscImage_ReadDataSectors(&disc, (u32)entry->offset, sectors, bytes))
		goto done;
	u8 *body = bytes + 4;
	const size_t size = (size_t)entry->size - 4;
	const u32 meshOffset = CTR_ReadU32LE(body + offsetof(struct Level, ptr_mesh_info));
	const u32 spawnOffset = CTR_ReadU32LE(body + offsetof(struct Level, ptrSpawnType1));
	if (meshOffset == 0 || meshOffset > size || sizeof(struct mesh_info) > size - meshOffset || spawnOffset == 0 || spawnOffset > size ||
	    8 > size - spawnOffset)
		goto done;
	struct mesh_info mesh;
	memcpy(&mesh, body + meshOffset, sizeof(mesh));
	const u32 quadOffset = CTR_ReadU32LE(body + meshOffset + offsetof(struct mesh_info, ptrQuadBlockArray));
	const u32 vertexOffset = CTR_ReadU32LE(body + meshOffset + offsetof(struct mesh_info, ptrVertexArray));
	const u32 mapOffset = CTR_ReadU32LE(body + spawnOffset + 4);
	if (CTR_ReadU32LE(body + spawnOffset) < 1 || mesh.numQuadBlock <= 0 || mesh.numQuadBlock > 65536 || mesh.numVertex <= 0 || mesh.numVertex > 65536 ||
	    quadOffset == 0 || quadOffset > size || (size_t)mesh.numQuadBlock > (size - quadOffset) / sizeof(struct QuadBlock) || vertexOffset == 0 ||
	    vertexOffset > size || (size_t)mesh.numVertex > (size - vertexOffset) / sizeof(struct LevVertex) || mapOffset == 0 || mapOffset > size ||
	    sizeof(struct UIMap) > size - mapOffset)
		goto done;
	mesh.ptrQuadBlockArray = (struct QuadBlock *)(body + quadOffset);
	mesh.ptrVertexArray = (struct LevVertex *)(body + vertexOffset);
	struct UIMap map;
	memcpy(&map, body + mapOffset, sizeof(map));
	const u64 key = NativeMinimap_GeometryKey(&mesh, &map);
	result = NativeMinimap_GetPixels(&mesh, &map, image, generate);
	if (result && key)
		NativeMinimap_WriteIndex(levelID, lod, source, key);
done:
	if (file)
		fclose(file);
	free(bytes);
	free(headerBytes);
	return result;
}

static u32 NativeMinimap_CreateOutline(const struct NativeMinimapImage *image, const u8 *pixels)
{
	const int count = image->pixelWidth * image->pixelHeight;
	float *distance = malloc((size_t)count * sizeof(float));
	u8 *outline = malloc((size_t)count * 4);
	if (!distance || !outline)
	{
		free(distance);
		free(outline);
		return 0;
	}
	for (int i = 0; i < count; i++)
		distance[i] = pixels[i * 4 + 3] >= 128 ? 0 : 1e10f;
	NativeMinimap_DistanceField(distance, NULL, image->pixelWidth, image->pixelHeight);
	const float radius = 0.7f * fminf(image->pixelWidth / image->width, image->pixelHeight / image->height);
	for (int i = 0; i < count; i++)
	{
		outline[i * 4] = outline[i * 4 + 1] = outline[i * 4 + 2] = 255;
		outline[i * 4 + 3] = (u8)(255 * fmaxf(0, fminf(1, radius + 0.5f - distance[i])));
	}
	const u32 texture = NativeRenderer_CreateMinimapTexture(image->pixelWidth, image->pixelHeight, outline);
	free(distance);
	free(outline);
	return texture;
}

static int NativeMinimap_Draw(const struct NativeMinimapImage *image, float left, float top, float width, float height, struct PrimMem *primMem, u32 *ot,
                              u32 colorID)
{
	if (!image->texture || !primMem || !ot)
		return 0;
	const size_t packetSize = sizeof(DR_PSYX_TEX) * 2 + sizeof(POLY_FT4);
	if ((uintptr_t)P32_GET(void *, primMem->cursor) > (uintptr_t)P32_GET(void *, primMem->end) ||
	    (uintptr_t)P32_GET(void *, primMem->end) - (uintptr_t)P32_GET(void *, primMem->cursor) < packetSize)
		return 0;
	DR_PSYX_TEX *set = P32_GET(void *, primMem->cursor);
	POLY_FT4 *p = (POLY_FT4 *)(set + 1);
	DR_PSYX_TEX *reset = (DR_PSYX_TEX *)(p + 1);
	memset(p, 0, sizeof(*p));
	const u32 color = colorID == 2 ? 0 : colorID == 3 ? 0x402000 : 0x808080;
	CtrGpu_WriteColorCode(&p->r0, color);
	setPolyFT4(p);
	p->x0 = p->x2 = (s16)left;
	p->x1 = p->x3 = (s16)(left + width);
	p->y0 = p->y1 = (s16)top;
	p->y2 = p->y3 = (s16)(top + height);
	p->u1 = p->u3 = NATIVE_MINIMAP_UV_EXTENT;
	p->v2 = p->v3 = NATIVE_MINIMAP_UV_EXTENT;
	NativePgxp_SetScreenXY(&p->x0, left, top);
	NativePgxp_SetScreenXY(&p->x1, left + width, top);
	NativePgxp_SetScreenXY(&p->x2, left, top + height);
	NativePgxp_SetScreenXY(&p->x3, left + width, top + height);
	SetPsyXTexture(set, image->texture, NATIVE_MINIMAP_UV_EXTENT, NATIVE_MINIMAP_UV_EXTENT);
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

float NativeMinimap_GetAnchorOffsetX(const struct UIMap *map)
{
	if (!map || !NativeAspect_IsActive() || !sdata || !P32_GET(struct GameTracker *, sdata->gGT) ||
	    (P32_GET(struct GameTracker *, sdata->gGT)->gameMode1 & MAIN_MENU))
		return 0.0f;
	const double origin = map->iconStartX - (P32_GET(struct GameTracker *, sdata->gGT)->numPlyrCurrGame == 3 ? 60 : 0);
	return (float)((SCREEN_WIDTH - origin) * (1.0 - NativeAspect_GetScaleX()));
}

int NativeMinimap_DrawLive(struct PrimMem *primMem, u32 *ot, u32 colorID)
{
	if (!gNativeModernMapEnabled || !sdata || !P32_GET(struct GameTracker *, sdata->gGT))
		return 0;
	const struct GameTracker *gt = P32_GET(struct GameTracker *, sdata->gGT);
	if ((gt->gameMode1 & (MAIN_MENU | GAME_CUTSCENE)) || gt->levelID < 0 || gt->levelID >= NATIVE_MINIMAP_LEVEL_COUNT ||
	    !P32_GET(struct Level *const, gt->level1) || !P32_GET(struct SpawnType1 *, P32_GET(struct Level *const, gt->level1)->ptrSpawnType1))
		return 0;
	void **pointers = ST1_GETPOINTERS(P32_GET(struct Level *const, gt->level1)->ptrSpawnType1);
	const struct UIMap *map = pointers[ST1_MAP];
	const struct mesh_info *mesh = P32_GET(struct mesh_info *, P32_GET(struct Level *const, gt->level1)->ptr_mesh_info);
	if (!map || !mesh)
		return 0;
	// Translation changes (including 3P) do not require regenerating the mask.
	struct UIMap projection = *map;
	projection.iconStartX = projection.iconStartY = 0;
	if (s_nativeMinimapBuiltEpoch != s_nativeMinimapEpoch || mesh != s_nativeMinimapMesh ||
	    memcmp(&projection, &s_nativeMinimapProjection, sizeof(projection)) != 0)
	{
		NativeRenderer_DestroyStreamingTexture(s_nativeMinimapLive.texture);
		memset(&s_nativeMinimapLive, 0, sizeof(s_nativeMinimapLive));
		s_nativeMinimapMesh = mesh;
		s_nativeMinimapProjection = projection;
		s_nativeMinimapBuiltEpoch = s_nativeMinimapEpoch;
		NativeMinimap_Build(mesh, map, &s_nativeMinimapLive);
	}
	float aspect = 1;
#if CTR_NATIVE_WIDESCREEN
	aspect = (float)NativeAspect_GetScaleX();
#endif
	const float x = map->iconStartX + NativeMinimap_GetAnchorOffsetX(map) + s_nativeMinimapLive.left * aspect - (gt->numPlyrCurrGame == 3 ? 60 : 0);
	const float y = map->iconStartY + s_nativeMinimapLive.top - 16 + (gt->numPlyrCurrGame == 3 ? 10 : 0);
	return NativeMinimap_Draw(&s_nativeMinimapLive, x, y, s_nativeMinimapLive.width * aspect, s_nativeMinimapLive.height, primMem, ot, colorID);
}

int NativeMinimap_DrawPreview(int levelID, int right, int bottom, int width, int height, struct PrimMem *primMem, u32 *ot, u32 colorID)
{
	if (!gNativeModernMapEnabled || levelID < 0 || levelID >= NATIVE_MINIMAP_LEVEL_COUNT || width <= 0 || height <= 0)
		return 0;
	struct NativeMinimapImage *image = &s_nativeMinimapPreviews[levelID];
	if (!s_nativeMinimapPreviewAttempted[levelID])
	{
		s_nativeMinimapPreviewAttempted[levelID] = 1;
		u8 *pixels = NativeMinimap_ReadLevelPixels(levelID, LOAD_LEVEL_LOD_1P, image, 0);
		if (pixels)
		{
			image->texture = NativeRenderer_CreateMinimapTexture(image->pixelWidth, image->pixelHeight, pixels);
			image->outlineTexture = NativeMinimap_CreateOutline(image, pixels);
			free(pixels);
		}
	}
	if (!image->texture)
		return 0;
	const float scale = fminf(width / image->width, height / image->height);
	float aspect = 1;
#if CTR_NATIVE_WIDESCREEN
	aspect = (float)NativeAspect_GetScaleX();
#endif
	const float w = image->width * scale * aspect, h = image->height * scale;
	const float left = right - width * 0.5f - w * 0.5f, top = bottom - height * 0.5f - h * 0.5f;
	if (!primMem || !ot || (uintptr_t)P32_GET(void *, primMem->end) < (uintptr_t)P32_GET(void *, primMem->cursor) ||
	    (uintptr_t)P32_GET(void *, primMem->end) - (uintptr_t)P32_GET(void *, primMem->cursor) < 3 * (sizeof(DR_PSYX_TEX) * 2 + sizeof(POLY_FT4)))
		return 0;
	// OT insertion reverses submission: shadow, blue silhouette, then body.
	const int drawn = NativeMinimap_Draw(image, left, top, w, h, primMem, ot, colorID);
	if (image->outlineTexture)
	{
		struct NativeMinimapImage silhouette = *image;
		silhouette.texture = image->outlineTexture;
		NativeMinimap_Draw(&silhouette, left, top, w, h, primMem, ot, 3);
		NativeMinimap_Draw(&silhouette, left + 2.0f * aspect, top + 1.5f, w, h, primMem, ot, 2);
	}
	return drawn;
}

void NativeMinimap_PrepareLive(void)
{
	if (!gNativeModernMapEnabled || !sdata || !P32_GET(struct GameTracker *, sdata->gGT))
		return;
	const struct GameTracker *gt = P32_GET(struct GameTracker *, sdata->gGT);
	if ((gt->gameMode1 & (MAIN_MENU | GAME_CUTSCENE)) || gt->levelID < 0 || gt->levelID >= NATIVE_MINIMAP_LEVEL_COUNT ||
	    !P32_GET(struct Level *const, gt->level1) || !P32_GET(struct SpawnType1 *, P32_GET(struct Level *const, gt->level1)->ptrSpawnType1) ||
	    !P32_GET(struct mesh_info *, P32_GET(struct Level *const, gt->level1)->ptr_mesh_info))
		return;
	void **pointers = ST1_GETPOINTERS(P32_GET(struct Level *const, gt->level1)->ptrSpawnType1);
	const struct UIMap *map = pointers[ST1_MAP];
	if (!map)
		return;
	struct NativeMinimapImage image = {0};
	u8 *pixels = NativeMinimap_GetPixels(P32_GET(struct mesh_info *, P32_GET(struct Level *const, gt->level1)->ptr_mesh_info), map, &image, 1);
	free(pixels);
}

void NativeMinimap_Prepare(void)
{
	if (!gNativeModernMapEnabled)
		return;
	// All standard race, split-screen, relic and hub geometry is prepared only
	// by the explicit toggle. Cache hits do not read the LEV or rasterise it.
	const int lods[] = {LOAD_LEVEL_LOD_1P, LOAD_LEVEL_LOD_2P, LOAD_LEVEL_LOD_4P, LOAD_LEVEL_LOD_RELIC};
	const Uint64 started = SDL_GetTicks();
	for (int level = 0; level < NATIVE_MINIMAP_LEVEL_COUNT; level++)
	{
		if (level >= NITRO_COURT && level < GEM_STONE_VALLEY)
			continue;
		for (int l = 0; l < (level >= GEM_STONE_VALLEY ? 1 : 4); l++)
		{
			struct NativeMinimapImage image = {0};
			u8 *pixels = NativeMinimap_ReadLevelPixels(level, lods[l], &image, 1);
			free(pixels);
		}
	}
	NativeMinimap_InvalidateLive();
	NativeMinimap_PrepareLive();
	for (int i = 0; i < NATIVE_MINIMAP_LEVEL_COUNT; i++)
		if (!s_nativeMinimapPreviews[i].texture)
			s_nativeMinimapPreviewAttempted[i] = 0;
	printf("[CTR Native] Modern Map cache prepared in %llu ms\n", (unsigned long long)(SDL_GetTicks() - started));
}

void NativeMinimap_InvalidateLive(void)
{
	s_nativeMinimapEpoch++;
}

void NativeMinimap_ReleaseGpu(void)
{
	NativeRenderer_DestroyStreamingTexture(s_nativeMinimapLive.texture);
	memset(&s_nativeMinimapLive, 0, sizeof(s_nativeMinimapLive));
	for (int i = 0; i < NATIVE_MINIMAP_LEVEL_COUNT; i++)
	{
		NativeRenderer_DestroyStreamingTexture(s_nativeMinimapPreviews[i].texture);
		NativeRenderer_DestroyStreamingTexture(s_nativeMinimapPreviews[i].outlineTexture);
		memset(&s_nativeMinimapPreviews[i], 0, sizeof(s_nativeMinimapPreviews[i]));
	}
	memset(s_nativeMinimapPreviewAttempted, 0, sizeof(s_nativeMinimapPreviewAttempted));
	NativeMinimap_InvalidateLive();
}
#else
int NativeMinimap_DrawLive(struct PrimMem *primMem, u32 *ot, u32 colorID)
{
	(void)primMem;
	(void)ot;
	(void)colorID;
	return 0;
}
int NativeMinimap_DrawPreview(int levelID, int right, int bottom, int width, int height, struct PrimMem *primMem, u32 *ot, u32 colorID)
{
	(void)levelID;
	(void)right;
	(void)bottom;
	(void)width;
	(void)height;
	(void)primMem;
	(void)ot;
	(void)colorID;
	return 0;
}
void NativeMinimap_Prepare(void)
{
}
void NativeMinimap_PrepareLive(void)
{
}
void NativeMinimap_InvalidateLive(void)
{
}
void NativeMinimap_ReleaseGpu(void)
{
}
#endif
