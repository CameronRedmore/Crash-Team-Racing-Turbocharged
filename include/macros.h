#ifndef MACROS_H
#define MACROS_H

#include <ctr_compiler.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CTR_STATIC_ASSERT(expr) _Static_assert((expr), #expr)

typedef uint64_t u64;
typedef int64_t s64;
typedef uint32_t u32;
typedef int32_t s32;
typedef s32 b32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint8_t u8;
typedef int8_t s8;
typedef float f32;
typedef double f64;

// Setting globals live in one header so game code never re-declares them.
// Included here because the CTR_NATIVE_*_ACTIVE macros below expand at call
// sites that may only see this header. native_options.h's own include of
// macros.h is a no-op by guard, and it needs only the typedefs above.
#include <platform/native_options.h>

#define AugReview 805
// TODO: Aug5 and Aug14
#define SepReview 903
#define UsaRetail 926
#define JpnTrial  1006
#define EurRetail 1020
#define JpnRetail 1111

#if BUILD == EurRetail
#define SCREEN_HEIGHT 236
#define FPS           25
#define ELAPSED_MS    40
#else
#define SCREEN_HEIGHT 216
#define FPS           30
#define ELAPSED_MS    32
#endif
#define SCREEN_WIDTH               512
#define SECOND                     (FPS * ELAPSED_MS)
#define MINUTE                     (SECOND * 60)
#define HOUR                       (MINUTE * 60)

#if defined(CTR_NATIVE) && defined(__vita__)
#define CTR_NATIVE_HAS_ADHOC       1
#define CTR_NATIVE_HAS_LEADERBOARD 1
#elif defined(CTR_NATIVE) && (defined(_WIN32) || defined(__EMSCRIPTEN__))
#define CTR_NATIVE_HAS_ADHOC       0
#define CTR_NATIVE_HAS_LEADERBOARD 1
#else
#define CTR_NATIVE_HAS_ADHOC       0
#define CTR_NATIVE_HAS_LEADERBOARD 0
#endif

#if defined(CTR_NATIVE)
#include <native_framerate.h>
#define CTR_NATIVE_60FPS 1
// gNative60FpsEnabled is declared in platform/native_options.h.
extern int gNativeForce30Fps;
extern int gNativeGhostReplayFpsOverride;
#define CTR_NATIVE_60FPS_SELECTED   ((gNativeGhostReplayFpsOverride >= 0) ? gNativeGhostReplayFpsOverride : gNative60FpsEnabled)
#define CTR_NATIVE_60FPS_ACTIVE     ((CTR_NATIVE_60FPS_SELECTED != 0) && (gNativeForce30Fps == 0))
#define CTR_FRAMES_PER_SECOND       (CTR_NATIVE_60FPS_ACTIVE ? NativeFrameRate_FromIndex(CTR_NATIVE_60FPS_SELECTED) : FPS)
#define CTR_NATIVE_FRAME_ELAPSED_MS FPS_HALF(ELAPSED_MS)
#define FPS_DOUBLE(x)               (CTR_NATIVE_60FPS_ACTIVE ? (s32)(((s64)(x) * CTR_FRAMES_PER_SECOND) / FPS) : (x))
#define FPS_HALF(x)                 (CTR_NATIVE_60FPS_ACTIVE ? (s32)(((s64)(x) * FPS) / CTR_FRAMES_PER_SECOND) : (x))
#define CTR_FRAME_STEP(x, frame)    NativeFrameRate_Step((x), (u32)(frame), CTR_FRAMES_PER_SECOND)
#define CTR_RETAIL_FRAME_TICK(frame) NativeFrameRate_Tick((u32)(frame), CTR_FRAMES_PER_SECOND)
// First rendered frame of a 30 FPS frame (TICK is the last one), and that frame's number.
#define CTR_RETAIL_FRAME_START(frame) (CTR_FRAME_STEP(1, (frame) - 1) != 0)
#define CTR_RETAIL_FRAME_INDEX(frame) FPS_HALF(frame)
#define FPS_LEFTSHIFT(x)            (CTR_NATIVE_60FPS_ACTIVE ? ((x) - 1) : (x))
#define FPS_RIGHTSHIFT(x)           (CTR_NATIVE_60FPS_ACTIVE ? ((x) + 1) : (x))
#else
#define CTR_NATIVE_60FPS            0
#define CTR_NATIVE_60FPS_ACTIVE     0
#define CTR_FRAMES_PER_SECOND       FPS
#define CTR_NATIVE_FRAME_ELAPSED_MS ELAPSED_MS
#define FPS_DOUBLE(x)               (x)
#define FPS_HALF(x)                 (x)
#define FPS_LEFTSHIFT(x)            (x)
#define FPS_RIGHTSHIFT(x)           (x)
#define CTR_FRAME_STEP(x, frame)    (x)
#define CTR_RETAIL_FRAME_TICK(frame) 1
#define CTR_RETAIL_FRAME_START(frame) 1
#define CTR_RETAIL_FRAME_INDEX(frame) (frame)
#endif

#if defined(CTR_NATIVE) && !defined(__vita__)
// The four smoothed globals are declared in platform/native_options.h.
#define CTR_NATIVE_SMOOTHED_AI_ACTIVE (gNativeSmoothedAIEnabled != 0)
#define CTR_NATIVE_SMOOTHED_COLLISION_ACTIVE (gNativeSmoothedCollisionEnabled != 0)
#define CTR_NATIVE_SMOOTHED_STEERING_ACTIVE (gNativeSmoothedSteeringEnabled != 0)
#define CTR_NATIVE_SMOOTHED_PHYSICS_ACTIVE (gNativeSmoothedPhysicsEnabled != 0)
#else
#define CTR_NATIVE_SMOOTHED_PHYSICS_ACTIVE 0
#define CTR_NATIVE_SMOOTHED_AI_ACTIVE 0
#define CTR_NATIVE_SMOOTHED_COLLISION_ACTIVE 0
#define CTR_NATIVE_SMOOTHED_STEERING_ACTIVE 0
#endif

// Max detail option: level geometry and models always use their highest LOD.
// The Native renderer always draws at max detail (its static level geometry
// depends on it); the option applies to Classic.
#if defined(CTR_NATIVE) && !defined(__vita__)
// gNativeMaxLodEnabled is declared in platform/native_options.h,
// NATIVE_DRAW3D_ACTIVE in platform/native_draw3d.h.
#define CTR_NATIVE_MAX_LOD_ACTIVE ((gNativeMaxLodEnabled != 0) || NATIVE_DRAW3D_ACTIVE())
#else
#define CTR_NATIVE_MAX_LOD_ACTIVE 0
#endif

// Pause screen backdrop: 0 = retail VRAM copy, 1 = full resolution (posterised), 2 = full resolution (smooth).
#if defined(CTR_NATIVE) && !defined(__vita__)
// gNativeHdPauseMode is declared in platform/native_options.h.
#define CTR_NATIVE_HD_PAUSE_MODE (gNativeHdPauseMode)
#else
#define CTR_NATIVE_HD_PAUSE_MODE 0
#endif

// Anti-aliasing option. FXAA filters at presentation; MSAA and SSAA change the
// main render target and are resolved to presentation resolution.
#if defined(CTR_NATIVE) && !defined(__vita__)
enum NativeAntiAliasingMode
{
	NATIVE_AA_OFF,
	NATIVE_AA_FXAA,
	NATIVE_AA_MSAA_2X,
	NATIVE_AA_MSAA_4X,
	NATIVE_AA_MSAA_8X,
	NATIVE_AA_SSAA_2X,
	NATIVE_AA_SSAA_4X,
	NATIVE_AA_MODE_COUNT,
};
// gNativeAntiAliasingMode is declared in platform/native_options.h.
#endif
#define CTR_SECONDS_TO_FRAMES(sec) ((s32)((sec) * FPS))

#define SECONDS(x)                 ((s32)(((f32)(x)) * SECOND))
#define MINUTES(x)                 ((s32)(((f32)(x)) * MINUTE))
#define HOURS(x)                   ((s32)(((f32)(x)) * HOUR))

#if defined(CTR_NATIVE)
#include <platform/native_aspect.h>
#define CTR_NATIVE_WIDESCREEN             1
#define CTR_WIDESCREEN_SCALE_X(value)   NativeAspect_ScaleX(value)
#define CTR_WIDESCREEN_EXPAND_X(value)  NativeAspect_ExpandX(value)
#else
#define CTR_NATIVE_WIDESCREEN             0
#define CTR_WIDESCREEN_SCALE_X(value)   (value)
#define CTR_WIDESCREEN_EXPAND_X(value)  (value)
#endif

#define nullptr                    ((void *)0)

#define force_inline CTR_FORCE_INLINE

#define internal                static
#define local_persist           static
#define global_variable         static

#define len(arr)                (sizeof(arr) / sizeof(arr[0]))
#define OFFSETOF(TYPE, ELEMENT) ((u32)offsetof(TYPE, ELEMENT))
#define CTR_OFFSET_OF_ARRAY(TYPE, MEMBER, INDEX) \
	(offsetof(TYPE, MEMBER) + (size_t)(INDEX) * sizeof(((TYPE *)0)->MEMBER[0]))
#define CTR_OFFSET_OF_2D_ARRAY(TYPE, MEMBER, ROW, COLUMN)                                                        \
	(offsetof(TYPE, MEMBER) + (size_t)(ROW) * sizeof(((TYPE *)0)->MEMBER[0]) + (size_t)(COLUMN) * sizeof(((TYPE *)0)->MEMBER[0][0]))

force_inline u16 CTR_ReadU16LE(const void *src)
{
	const u8 *bytes = (const u8 *)src;

	return (u16)((u16)bytes[0] | ((u16)bytes[1] << 8));
}

force_inline u32 CTR_ReadU32LE(const void *src)
{
	const u8 *bytes = (const u8 *)src;

	return (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

force_inline void CTR_WriteU16LE(void *dst, u16 value)
{
	u8 *bytes = (u8 *)dst;

	bytes[0] = (u8)value;
	bytes[1] = (u8)(value >> 8);
}

force_inline void CTR_WriteU32LE(void *dst, u32 value)
{
	u8 *bytes = (u8 *)dst;

	bytes[0] = (u8)value;
	bytes[1] = (u8)(value >> 8);
	bytes[2] = (u8)(value >> 16);
	bytes[3] = (u8)(value >> 24);
}

// Raw [3] vector array helpers. Arguments must be side-effect-free lvalues.
#define CTR_COPY_VEC3(DST, SRC) \
	do                          \
	{                           \
		(DST)[0] = (SRC)[0];    \
		(DST)[1] = (SRC)[1];    \
		(DST)[2] = (SRC)[2];    \
	} while (0)

#define CTR_SET_VEC3(DST, X, Y, Z) \
	do                             \
	{                              \
		(DST)[0] = (X);            \
		(DST)[1] = (Y);            \
		(DST)[2] = (Z);            \
	} while (0)

// Retail format strings use PsyQ `%ld` for 32-bit values. Keep call sites on
// project-width types while satisfying host printf varargs for the literal.
#define CTR_PRINTF_PSX_LONG(value) ((long)(s32)(value))

#define RGBtoBGR(color)            ((color & 0xFF0000) >> 16) | (color & 0xFF00) | ((color & 0xFF) << 16)

#define GetRed(color)              (color & 0xFF)

#define GetGreen(color)            (color & 0xFF00) >> 8

#define GetBlue(color)             (color & 0xFF0000) >> 16

#define aspectratioupsample(int)   (int * 7) / 4

#define aspectratiodownsample(int) (int * 4) / 7

#endif
