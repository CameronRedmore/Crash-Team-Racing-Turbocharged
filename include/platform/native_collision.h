#ifndef NATIVE_COLLISION_H
#define NATIVE_COLLISION_H
#include <platform/native_physics.h>
struct ScratchpadStruct;
struct BspSearchVertex;
struct Scrub;
struct Thread;
struct BucketSearchParams;
struct DriverCollisionSearch;
void NativeCollision_CarSearch(struct Driver *d, struct Thread *first, struct BucketSearchParams *search);
void NativeCollision_Cars(struct Thread *thread, struct DriverCollisionSearch *search, Vec3 *velocity);
typedef struct NativeCollisionHit
{
	double fraction;
	NativePhysicsVec point, normal;
	int feature;
} NativeCollisionHit;
int NativeCollision_SweepTriangle(NativePhysicsVec start, NativePhysicsVec end, double radius,
                                 NativePhysicsVec a, NativePhysicsVec b, NativePhysicsVec c, NativeCollisionHit *hit);
int NativeCollision_RayTriangle(NativePhysicsVec start, NativePhysicsVec end, NativePhysicsVec a,
                               NativePhysicsVec b, NativePhysicsVec c, NativeCollisionHit *hit, double *u, double *v);
void NativeCollision_BeginSweep(struct ScratchpadStruct *sps, NativePhysicsVec start, NativePhysicsVec step);
void NativeCollision_EndSweep(void);
NativePhysicsVec NativeCollision_Normal(struct ScratchpadStruct *sps);
double NativeCollision_HitFraction(struct ScratchpadStruct *sps);
void NativeCollision_MovedTriangle(struct ScratchpadStruct *sps, struct BspSearchVertex *a, struct BspSearchVertex *b, struct BspSearchVertex *c);
void NativeCollision_FixedTriangle(struct ScratchpadStruct *sps, struct BspSearchVertex *a, struct BspSearchVertex *b, struct BspSearchVertex *c);
u32 NativeCollision_Impact(struct Driver *d, struct Thread *t, struct ScratchpadStruct *sps, struct Scrub *scrub, Vec3 *velocity);
#endif
