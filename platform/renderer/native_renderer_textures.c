/*
 * Texture objects: the white and LUT textures, streaming (video) textures,
 * font-atlas and minimap textures, and the PSX colour packing helpers.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>

#include "platform/native_log.h"
#include "platform/native_options.h"
#include "platform/native_renderer_internal.h"

TextureID NativeRenderer_GetWhiteTexture(void)
{
	return s_whiteTexture;
}

TextureID NativeRenderer_CreateStreamingTexture(int width, int height)
{
	TextureID texture = 0;

	if ((width <= 0) || (height <= 0))
	{
		return 0;
	}

	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	NativeRenderer_InvalidateTextureBinding();
	return texture;
}

void NativeRenderer_UpdateStreamingTexture(TextureID texture, int width, int height, const u8 *rgbaPixels)
{
	if ((texture == 0) || (width <= 0) || (height <= 0) || (rgbaPixels == NULL))
	{
		return;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);
	NativeRenderer_InvalidateTextureBinding();
}

void NativeRenderer_DestroyStreamingTexture(TextureID texture)
{
	if (texture == 0)
	{
		return;
	}

	NativeRenderer_InvalidateTextureBindingIfCurrent(texture);
	glDeleteTextures(1, &texture);
}

//--------------------------------------------------------------------------------------------

internal void NativeRenderer_GenerateCommonTextures(void)
{
	u32 whitePixelData = 0xFFFFFFFF;

	glGenTextures(1, &s_whiteTexture);
	{
		glBindTexture(GL_TEXTURE_2D, s_whiteTexture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePixelData);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

#ifndef __vita__
	glGenTextures(1, &s_rgLutTexture);
	{
		// Texture unit 1 is reserved for the immutable PSX color lookup table.
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, s_rgLutTexture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LUT_WIDTH, LUT_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgLUT);

		glActiveTexture(GL_TEXTURE0);
	}
#endif

#ifdef __vita__
	for (u16 y = 0; y < LUT_HEIGHT; y++)
	{
		u8 *row = rgLUT + y * (LUT_WIDTH * 4);
		for (u16 x = 0; x < LUT_WIDTH; x++)
		{
			const u16 c = (y << 8) | x;
			const u8 r5 = (u8)(c & 31);
			const u8 g5 = (u8)((c >> 5) & 31);
			const u8 b5 = (u8)((c >> 10) & 31);
			u8 *pixel = row + x * 4;
			pixel[0] = (u8)((r5 << 3) | (r5 >> 2));
			pixel[1] = (u8)((g5 << 3) | (g5 >> 2));
			pixel[2] = (u8)((b5 << 3) | (b5 >> 2));
			pixel[3] = (c & 0x8000) ? 255 : 0;
		}
	}

	glGenTextures(1, &s_presentLutTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, s_presentLutTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LUT_WIDTH, LUT_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgLUT);
	glActiveTexture(GL_TEXTURE0);
#endif
}

internal void NativeRenderer_DestroyTexture(TextureID texture)
{
	if (texture == (TextureID)-1)
	{
		return;
	}

	glDeleteTextures(1, &texture);
}

internal u16 NativeRenderer_PackRGB24ToPSX15(u8 r, u8 g, u8 b)
{
	return (u16)(((r >> 3) & 0x1f) | (((g >> 3) & 0x1f) << 5) | (((b >> 3) & 0x1f) << 10));
}
internal float NativeRenderer_PSXColorComponentFloat(u8 value)
{
	const u8 psx8 = (u8)((value >> 3) << 3);
	return (float)psx8 / 255.0f;
}

internal TextureID NativeRenderer_CreateGhostReplayTexture(int width, int height, const u8 *pixels)
{
	TextureID texture = 0;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	NativeRenderer_InvalidateTextureBinding();
	return texture;
}

#ifndef __vita__
// Collision maps retain their coverage when scaled down in the track menu.
u32 NativeRenderer_CreateMinimapTexture(int width, int height, const u8 *pixels)
{
	TextureID texture = NativeRenderer_CreateGhostReplayTexture(width, height, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	return texture;
}

// Signed distance field atlas for Enhancements > Font (native_font.c).
u32 NativeRenderer_CreateFontAtlasTexture(int width, int height, const u8 *pixels)
{
	TextureID texture = 0;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glGenerateMipmap(GL_TEXTURE_2D);
	NativeRenderer_InvalidateTextureBinding();
	return texture;
}

void NativeRenderer_DestroyFontAtlasTexture(u32 texture)
{
	TextureID id = texture;
	glDeleteTextures(1, &id);
	NativeRenderer_InvalidateTextureBinding();
}
#endif
