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
#ifndef __vita__
static u32 s_frameUploadVertices, s_frameUploads;
void NativeRenderer_BeginUploadFrame(void)
{
	s_frameUploadVertices = s_frameUploads = 0;
}
void NativeRenderer_GetUploadCounts(u32 *vertices, u32 *uploads)
{
	*vertices = s_frameUploadVertices;
	*uploads = s_frameUploads;
}
#endif

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
	// Replace storage before writing the next batch: queued GL draws can keep
	// the previous allocation rather than stalling this upload on its reuse.
	// Size it to the batch, not MAX_VERTEX_BUFFER_SIZE (18 MB): GL on D3D12
	// allocates a new resource per orphan, and a frame rarely needs 1 MB.
	// Rounding to a power of two keeps the sizes few, so allocations recycle.
	GLsizeiptr capacity = 64 * 1024;
	const GLsizeiptr batchBytes = (GLsizeiptr)num_vertices * (GLsizeiptr)sizeof(GrVertex);
	while (capacity < batchBytes)
	{
		capacity *= 2;
	}
	glBufferData(GL_ARRAY_BUFFER, capacity, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, batchBytes, vertices);
	s_frameUploadVertices += (u32)num_vertices;
	s_frameUploads++;
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

#if NATIVE_DRAW3D_SUPPORTED
// Face culling last pushed to GL: 0 off, 1 clockwise front faces, 2
// counter-clockwise. Cached because toggling it around every object draw is a
// pipeline-state change per draw on GL-on-D3D12. Only this module draws.
// The front face stays GL's default counter-clockwise and the culled side
// changes instead, so gl_FrontFacing means the same thing in every draw (the
// shader culls GPU-transformed triangles with it, see gNativeShaderCullEnabled).
global_variable u32 s_cullMode = 0;

internal void NativeRenderer_SetCullMode(u32 cullMode)
{
	if (s_cullMode == cullMode)
	{
		return;
	}
	if (cullMode)
	{
		if (!s_cullMode)
		{
			glEnable(GL_CULL_FACE);
		}
		// Projection flips screen Y, so retail front faces are clockwise in GL:
		// clockwise fronts cull the counter-clockwise (GL front) side.
		glCullFace(cullMode == 1 ? GL_FRONT : GL_BACK);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}
	s_cullMode = cullMode;
}
#endif

internal void NativeRenderer_IssueTriangles(int start_vertex, int triangles)
{
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDERER_DRAW_TRIANGLES);
	glDrawArrays(GL_TRIANGLES, start_vertex, triangles * 3);
	NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_DRAW_TRIANGLES);
}

void NativeRenderer_DrawTriangles(int start_vertex, int triangles)
{
#if NATIVE_DRAW3D_SUPPORTED
	// Legacy and utility passes never inherit an object's culling.
	NativeRenderer_SetCullMode(0);
#endif
	NativeRenderer_IssueTriangles(start_vertex, triangles);
}

#if NATIVE_DRAW3D_SUPPORTED
static GLuint s_glStaticVertexArray;
static GLuint s_glStaticVertexBuffer;

internal void NativeRenderer_RestoreVertexArray(void)
{
	glBindVertexArray(s_boundVertexBuffer >= 0 ? s_glVertexArray[s_boundVertexBuffer] : 0);
}

void NativeRenderer_UploadStaticVertices(const GrVertex *vertices, int count)
{
	if (count <= 0 || vertices == NULL)
	{
		if (s_glStaticVertexBuffer)
		{
			glBindBuffer(GL_ARRAY_BUFFER, s_glStaticVertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
		}
		return;
	}
	if (!s_glStaticVertexArray)
	{
		glGenVertexArrays(1, &s_glStaticVertexArray);
		glGenBuffers(1, &s_glStaticVertexBuffer);
		glBindVertexArray(s_glStaticVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, s_glStaticVertexBuffer);
		NativeRenderer_SetupVertexAttributes();
	}
	glBindBuffer(GL_ARRAY_BUFFER, s_glStaticVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)count * (GLsizeiptr)sizeof(GrVertex), vertices, GL_STATIC_DRAW);
	NativeRenderer_RestoreVertexArray();
}

void NativeRenderer_DrawStaticObjectTriangles(const s32 *firstVertex, const s32 *vertexCount, int draws, u32 cullMode)
{
	if (!s_glStaticVertexArray || draws <= 0)
		return;
	NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDERER_DRAW_TRIANGLES);
	glBindVertexArray(s_glStaticVertexArray);
	NativeRenderer_SetCullMode(cullMode);
	glMultiDrawArrays(GL_TRIANGLES, (const GLint *)firstVertex, (const GLsizei *)vertexCount, draws);
	NativeRenderer_RestoreVertexArray();
	NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDERER_DRAW_TRIANGLES);
}

void NativeRenderer_DrawObjectTriangles(int startVertex, int triangles, u32 cullMode)
{
	// Culling stays set for the next object draw; NativeRenderer_DrawTriangles
	// turns it off before any legacy or utility draw.
	NativeRenderer_SetCullMode(cullMode);
	NativeRenderer_IssueTriangles(startVertex, triangles);
}
#endif

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
