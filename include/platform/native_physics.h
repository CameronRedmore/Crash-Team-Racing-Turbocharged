#ifndef NATIVE_PHYSICS_H
#define NATIVE_PHYSICS_H

#include <macros.h>
#include <ctr_math.h>
#include <psx/libgte.h>

struct Driver;
// Continuous state uses double precision; PS1 layouts remain fixed point.
typedef struct NativePhysicsVec
{
	double x, y, z;
} NativePhysicsVec;
enum NativePhysicsDomain
{
	NATIVE_PHYSICS_PLAYER,
	NATIVE_PHYSICS_AI,
	NATIVE_PHYSICS_COLLISION,
	NATIVE_PHYSICS_STEERING,
};
extern int gNativeSmoothedPhysicsEnabled;
extern int gNativeSmoothedAIEnabled;
extern int gNativeSmoothedCollisionEnabled;
extern int gNativeSmoothedSteeringEnabled;
void NativePhysics_SetDomain(enum NativePhysicsDomain domain, int enabled);
double NativePhysics_ReadScalar(struct Driver *d, size_t offset, size_t size);
void NativePhysics_WriteScalar(struct Driver *d, size_t offset, size_t size, double value);
#define NATIVE_PHYSICS_READ(d, field) NativePhysics_ReadScalar(d, offsetof(struct Driver, field), sizeof((d)->field))
#define NATIVE_PHYSICS_WRITE(d, field, value) NativePhysics_WriteScalar(d, offsetof(struct Driver, field), sizeof((d)->field), value)
double NativePhysics_WrapAngle(double value);
double NativePhysics_FrameScale(void);
double NativePhysics_ElapsedMS(double elapsedMS);
void NativePhysics_Steer(struct Driver *d);
void NativePhysics_DriftSteer(struct Driver *d);
void NativePhysics_LerpRotation(struct Driver *d, double target);
void NativePhysics_CounterSteer(struct Driver *d);
NativePhysicsVec NativePhysics_ReadPosition(struct Driver *d);
void NativePhysics_WritePosition(struct Driver *d, NativePhysicsVec position);
void NativePhysics_SetEnabled(int enabled);
void NativePhysics_Reset(void);
void NativePhysics_ResetDriver(struct Driver *d);
NativePhysicsVec NativePhysics_ReadVelocity(struct Driver *d);
void NativePhysics_WriteVelocity(struct Driver *d, NativePhysicsVec value);
void NativePhysics_UpdateSteeringMatrix(struct Driver *d);
NativePhysicsVec NativePhysics_RotateDriver(struct Driver *d, NativePhysicsVec value, int transpose);
NativePhysicsVec NativePhysics_Rotate(const MATRIX *matrix, NativePhysicsVec value, int transpose);
double NativePhysics_GetSpeed(struct Driver *d);
void NativePhysics_SetSpeed(struct Driver *d, double value);
void NativePhysics_ConvertVecToSpeed(struct Driver *d, NativePhysicsVec value);
void NativePhysics_ConvertSpeedToVec(struct Driver *d, Vec3 *velocity);
void NativePhysics_Gravity(struct Driver *d, Vec3 *velocity);
void NativePhysics_JumpAndFriction(struct Driver *d);
void NativePhysics_SurfacePushback(struct Driver *d);
NativePhysicsVec NativePhysics_Step(struct Driver *d, double elapsedMS, double multiplier);
void NativePhysics_Move(struct Driver *d, NativePhysicsVec step, double fraction);
int NativePhysics_GetStateSize(void);
int NativePhysics_CaptureState(void *dst, int size);
int NativePhysics_RestoreState(const void *src, int size);

#endif
