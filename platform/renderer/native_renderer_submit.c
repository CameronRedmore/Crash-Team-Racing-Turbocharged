/*
 * Vertex submission: the vertex buffers the PSX submit run and the native
 * renderer share, triangle drawing and GL debug-group markers.
 *
 * Derived from REDRIVER2/PsyCross MIT source:
 * externals/PsyCross/src/render/PsyX_render.cpp
 * See THIRD_PARTY_NOTICES.md for copyright and license details.
 */

#include <macros.h>
#include <SDL3/SDL.h>

#include "platform/native_log.h"
#include "platform/native_renderer_internal.h"

// Which of the two vertex buffers the next submission writes. Only this module
// rotates it.
global_variable int s_curVertexBuffer = 0;

void NativeRenderer_UpdateVertexBuffer(const GrVertex *vertices, int num_vertices)
{
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDERER_VERTEX_UPLOAD);
	if (num_vertices <= 0)
	{
		NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_VERTEX_UPLOAD);
		return;
	}
	if ((u32)num_vertices >= MAX_VERTEX_BUFFER_SIZE)
	{
		NATIVE_RENDERER_ERROR("%s\n", "MAX_VERTEX_BUFFER_SIZE reached, expect rendering errors");
		num_vertices = MAX_VERTEX_BUFFER_SIZE;
	}

	const int bufferIndex = s_curVertexBuffer;
#ifndef __vita__
	s_curVertexBuffer = (s_curVertexBuffer + 1) % MAX_NUM_VERTEX_BUFFERS;
#endif
	s_boundVertexBuffer = bufferIndex;
	glBindVertexArray(s_glVertexArray[bufferIndex]);
	glBindBuffer(GL_ARRAY_BUFFER, s_glVertexBuffer[bufferIndex]);
#ifdef __vita__
	GrVertex *gpuVertices = NativeRenderer_AllocateVertexBuffer(num_vertices);
	if (gpuVertices == NULL)
	{
		NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_VERTEX_UPLOAD);
		return;
	}
	memcpy(gpuVertices, vertices, (size_t)num_vertices * sizeof(GrVertex));
	vglBufferData(GL_ARRAY_BUFFER, gpuVertices);
#else
	glBufferSubData(GL_ARRAY_BUFFER, 0, num_vertices * sizeof(GrVertex), vertices);
#endif

	NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_VERTEX_UPLOAD);
}

GrVertex *NativeRenderer_AllocateVertexBuffer(int count)
{
#ifdef __vita__
	if (count <= 0 || (u32)count > MAX_VERTEX_BUFFER_SIZE)
	{
		return NULL;
	}
	return (GrVertex *)vglAllocFromScratch((size_t)count * sizeof(GrVertex));
#else
	(void)count;
	return NULL;
#endif
}

void NativeRenderer_DrawTriangles(int start_vertex, int triangles)
{
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDERER_DRAW_TRIANGLES);
	glDrawArrays(GL_TRIANGLES, start_vertex, triangles * 3);
	NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_DRAW_TRIANGLES);
}

void NativeRenderer_PushDebugLabel(const char *label)
{
#if !defined(__vita__) && !defined(__EMSCRIPTEN__)
	if (!GLAD_GL_KHR_debug)
	{
		return;
	}
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0x8000, strlen(label), label);
#endif
}

void NativeRenderer_PopDebugLabel(void)
{
#if !defined(__vita__) && !defined(__EMSCRIPTEN__)
	if (!GLAD_GL_KHR_debug)
	{
		return;
	}
	glPopDebugGroup();
#endif
}
