// Standalone atlas/cache test: cc -O2 -Isrc/include this_file.c -lm -o test
// Run with a disposable assets directory containing fonts/crash-a-like.ttf.
// GPU upload is replaced by a byte checksum.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../platform/native_font.c"

static const char *assetDir;
static u32 uploadedHash;
static int uploads;
int NativeAssets_BuildPath(const char *path, char *dst, size_t size)
{
	return snprintf(dst, size, "%s/%s", assetDir, path) < (int)size;
}
FILE *NativeAssets_OpenHost(const char *path, const char *mode)
{
	char full[1024];
	return NativeAssets_BuildPath(path, full, sizeof(full)) ? fopen(full, mode) : NULL;
}
int NativeAssets_ReadBytes(const char *path, int mode, struct NativeAssetsByteBuffer *bytes)
{
	(void)mode;
	FILE *f = NativeAssets_OpenHost(path, "rb");
	if (!f)
		return 0;
	fseek(f, 0, SEEK_END);
	bytes->size = (int)ftell(f);
	rewind(f);
	bytes->data = malloc((size_t)bytes->size);
	assert(bytes->data);
	assert(fread(bytes->data, (size_t)bytes->size, 1, f) == 1);
	fclose(f);
	return 1;
}
void NativeAssets_FreeBytes(struct NativeAssetsByteBuffer *bytes)
{
	free(bytes->data);
}
void Platform_Log(const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	vprintf(fmt, a);
	va_end(a);
}
void Platform_LogError(const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	vfprintf(stderr, fmt, a);
	va_end(a);
}
u32 NativeRenderer_CreateFontAtlasTexture(int w, int h, const u8 *pixels)
{
	uploadedHash = NativeFont_Hash(2166136261u, pixels, (size_t)w * h);
	uploads++;
	return 1;
}
void NativeRenderer_DestroyFontAtlasTexture(u32 texture)
{
	(void)texture;
}
int main(int argc, char **argv)
{
	assert(argc == 2);
	assetDir = argv[1];
	char path[1024];
	assert(NativeAssets_BuildPath("fonts/crash-a-like.ttf.sdf-cache", path, sizeof(path)));
	remove(path);
	gNativeFont = NATIVE_FONT_CRASH_A_LIKE;
	clock_t start = clock();
	assert(NativeFont_IsActive());
	double cold = (double)(clock() - start) / CLOCKS_PER_SEC;
	u32 originalHash = uploadedHash;
	struct NativeFontGlyph glyphs[256];
	memcpy(glyphs, s_nativeFont.glyphs, sizeof(glyphs));
	NativeFont_ReleaseGpu();
	start = clock();
	assert(NativeFont_IsActive());
	double warm = (double)(clock() - start) / CLOCKS_PER_SEC;
	assert(originalHash == uploadedHash);
	assert(memcmp(glyphs, s_nativeFont.glyphs, sizeof(glyphs)) == 0);
	assert(uploads == 2);
	assert(NativeFont_IsActive() && uploads == 2);
	gNativeFont = NATIVE_FONT_ORIGINAL;
	assert(!NativeFont_IsActive());
	gNativeFont = NATIVE_FONT_CRASH_A_LIKE;
	assert(NativeFont_IsActive() && uploads == 2);
	struct NativeAssetsByteBuffer bytes = {0};
	assert(NativeAssets_ReadBytes("fonts/crash-a-like.ttf", 0, &bytes));
	u32 key = NativeFont_CacheKey(&s_nativeFonts[gNativeFont], &bytes);
	bytes.data[0] ^= 1;
	assert(key != NativeFont_CacheKey(&s_nativeFonts[gNativeFont], &bytes));
	NativeAssets_FreeBytes(&bytes);
	assert(!NativeFont_ReadCache("fonts/crash-a-like.ttf.sdf-cache", key ^ 1));
	FILE *f = fopen(path, "r+b");
	assert(f);
	fseek(f, -1, SEEK_END);
	int last = fgetc(f);
	fseek(f, -1, SEEK_END);
	fputc(last ^ 1, f);
	fclose(f);
	assert(!NativeFont_ReadCache("fonts/crash-a-like.ttf.sdf-cache", key));
	NativeFont_ReleaseGpu();
	assert(NativeFont_IsActive()); // Corruption regenerates the cache.
	assert(originalHash == uploadedHash);
	printf("Atlas generation: %.3fs; cache load: %.3fs; byte-identical atlas and metrics.\n", cold, warm);
	return 0;
}
