#ifndef NATIVE_AI_MATH_H
#define NATIVE_AI_MATH_H

// Floating point AI arithmetic is selected independently from player physics.
// The Original branch retains MIPS wrapping and arithmetic-shift semantics.
#include <math.h>
#if defined(CTR_NATIVE) && !defined(__vita__)
#define NATIVE_AI_READ(d, field) (CTR_NATIVE_SMOOTHED_AI_ACTIVE ? NATIVE_PHYSICS_READ(d, field) : (double)(d)->field)
#define NATIVE_AI_WRITE(d, field, value)           \
	do                                             \
	{                                              \
		if (CTR_NATIVE_SMOOTHED_AI_ACTIVE)         \
			NATIVE_PHYSICS_WRITE(d, field, value); \
		else                                       \
			(d)->field = (s32)(value);             \
	} while (0)
#else
#define NATIVE_AI_READ(d, field)         ((double)(d)->field)
#define NATIVE_AI_WRITE(d, field, value) ((d)->field = (s32)(value))
#endif
static inline double NativeAI_FrameStep(double value, unsigned int frame)
{
#if defined(CTR_NATIVE) && !defined(__vita__)
	if (CTR_NATIVE_SMOOTHED_AI_ACTIVE)
		return value * NativePhysics_FrameScale();
#endif
	return CTR_FRAMES_PER_SECOND > 60 ? CTR_FRAME_STEP((s32)value, frame) : value;
}
static inline double NativeAI_HalfDecay(double value, unsigned int frame)
{
#if defined(CTR_NATIVE) && !defined(__vita__)
	if (CTR_NATIVE_SMOOTHED_AI_ACTIVE)
		return value * pow(0.5, NativePhysics_FrameScale());
#endif
	if (CTR_FRAMES_PER_SECOND > 60 && !CTR_RETAIL_FRAME_TICK(frame))
		return value;
	return CTR_MipsSra((s32)value, 1);
}
static inline double NativeAI_Add(double a, double b)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? a + b : CTR_MipsAddLo((s32)a, (s32)b);
}
static inline double NativeAI_Sub(double a, double b)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? a - b : CTR_MipsSubLo((s32)a, (s32)b);
}
static inline double NativeAI_Mul(double a, double b)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? a * b : CTR_MipsMulLo((s32)a, (s32)b);
}
static inline double NativeAI_Div(double a, double b)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? (b == 0 ? 0 : a / b) : CTR_MipsDiv((s32)a, (s32)b);
}
static inline double NativeAI_Down(double a, int bits)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? ldexp(a, -bits) : CTR_MipsSra((s32)a, bits);
}
static inline double NativeAI_Up(double a, int bits)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? ldexp(a, bits) : CTR_MipsSll((s32)a, bits);
}
static inline double NativeAI_Neg(double a)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? -a : CTR_MipsNegLo((s32)a);
}
static inline double NativeAI_Wrap(double a)
{
#if defined(CTR_NATIVE) && !defined(__vita__)
	if (CTR_NATIVE_SMOOTHED_AI_ACTIVE)
		return NativePhysics_WrapAngle(a);
#endif
	return (s32)a & 0xfff;
}
static inline double NativeAI_Sin(double angle)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? sin(angle * (6.2831853071795864769 / 4096.0)) * 4096.0 : MATH_Sin((int)angle);
}
static inline double NativeAI_Cos(double angle)
{
	return CTR_NATIVE_SMOOTHED_AI_ACTIVE ? cos(angle * (6.2831853071795864769 / 4096.0)) * 4096.0 : MATH_Cos((int)angle);
}
#endif
