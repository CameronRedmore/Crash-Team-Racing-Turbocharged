#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <common.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Exercise the actual solver without a renderer or disc image.
#include "../platform/native_physics.c"
#include "../game/Vehicle/VehPhysSmoothed.c"
#include "../game/Vehicle/VehSteeringSmoothed.c"
#include "../platform/native_collision.c"
#include <platform/native_ai_math.h>
int gNative60FpsEnabled, gNativeForce30Fps;
int gNativeGhostReplayFpsOverride = -1;
void PhysTerrainSlope(struct Driver *d) { NativePhysics_UpdateSteeringMatrix(d); NativePhysics_CounterSteer(d); }
struct sData sdata_static;

void GAMEPAD_ShockForce1(struct Driver *d, int frame, int value) { (void)d; (void)frame; (void)value; }
void GAMEPAD_ShockFreq(struct Driver *d, int frame, int value) { (void)d; (void)frame; (void)value; }
int OtherFX_Play(u32 sound, int flags) { (void)sound; (void)flags; return 0; }
void OtherFX_Play_Echo(u32 sound, int flags, int echo) { (void)sound; (void)flags; (void)echo; }
int OtherFX_Play_LowLevel(u32 sound, u8 antiSpam, u32 flags) { (void)sound; (void)antiSpam; (void)flags; return 0; }
void Voiceline_RequestPlayDriver(u32 voice, int driver, u32 character) { (void)voice; (void)driver; (void)character; }
void GAMEPAD_JogCon1(struct Driver *d, u8 val, u16 timeMS) { (void)d; (void)val; (void)timeMS; }
void VehPhysProc_SlamWall_Init(struct Thread *t, struct Driver *d)
{
	(void)t;
	d->kartState=KS_CRASHING;
	d->speed=d->speedApprox=0;
	d->velocity=(Vec3){0};
}
void BOTS_CollideWithOtherAI(struct Driver *a, struct Driver *b) { (void)a; (void)b; }
int VehPhysCrash_Attack(struct Driver *a, struct Driver *b, b32 feedback, b32 pop) { (void)a; (void)b; (void)feedback; (void)pop; return 0; }
static void VehPhysCrash_PlayHumanFeedback(struct Thread *a, struct Thread *b, struct Driver *da, struct Driver *db, u32 feedback)
{ (void)a; (void)b; (void)da; (void)db; (void)feedback; }
#include "../game/NativeCollisionImpact.c"
#include "../game/Vehicle/VehCarCollisionSmoothed.c"

static void near(double a, double b) { assert(fabs(a - b) < 0.000001); }

static void test_collision_response(void)
{
	struct Driver d={0}, other={0};
	struct Instance inst={0};
	struct Thread t={.object=&d,.driverHitRadius=10}, ot={.object=&other,.driverHitRadius=10};
	struct ScratchpadStruct sps={0};
	struct Scrub scrub={.flags=SCRUB_FLAG_APPLY_IMPACT};
	Vec3 output;
	d.driverID=4; other.driverID=5; d.instSelf=&inst;
	d.matrixMovingDir.m[0][0]=d.matrixMovingDir.m[1][1]=d.matrixMovingDir.m[2][2]=4096;
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,0);
	NativePhysics_SetDomain(NATIVE_PHYSICS_COLLISION,1);
	sps.hit.plane.normal.x=4096;
	NativePhysics_WriteVelocity(&d,(NativePhysicsVec){-100.25,0,7.75});
	assert(NativeCollision_Impact(&d,&t,&sps,&scrub,&output)==1);
	near(NativePhysics_ReadVelocity(&d).x,0);
	near(NativePhysics_ReadVelocity(&d).z,7.75);
	// A hard crash must preserve the state initializer's stop, even when the
	// caller supplies a separate output vector rather than d.velocity.
	scrub.flags|=SCRUB_FLAG_SLAM_ON_HARD_IMPACT;
	NativePhysics_WriteVelocity(&d,(NativePhysicsVec){-10000.25,0,7.75});
	assert(NativeCollision_Impact(&d,&t,&sps,&scrub,&output)==2);
	assert(d.kartState==KS_CRASHING);
	near(NativePhysics_ReadVelocity(&d).x,0);
	near(NativePhysics_ReadVelocity(&d).z,0);
	assert(output.x==0 && output.y==0 && output.z==0);
	// Equal weights bounce symmetrically and retain fractional separation.
	d.const_CollisionWeight=other.const_CollisionWeight=1;
	NativePhysics_Move(&other,(NativePhysicsVec){19.25*256,0,0},1);
	NativePhysics_WriteVelocity(&d,(NativePhysicsVec){100.25,0,0});
	NativePhysics_WriteVelocity(&other,(NativePhysicsVec){-100.25,0,0});
	struct DriverCollisionSearch search={0}; search.bucket.th=&ot;
	NativeCollision_Cars(&t,&search,&output);
	near(NativePhysics_ReadVelocity(&d).x,-100.25/3-12);
	near(NativePhysics_ReadVelocity(&other).x,100.25/3+12);
	// The exact overlap test rejects a rounded, apparently touching pair.
	NativePhysics_Move(&other,(NativePhysicsVec){256,0,0},1);
	NativePhysicsVec before=NativePhysics_ReadVelocity(&d);
	NativeCollision_Cars(&t,&search,&output);
	near(NativePhysics_ReadVelocity(&d).x,before.x);
	NativePhysics_SetDomain(NATIVE_PHYSICS_COLLISION,0);
}

static void test_domains(void)
{
	struct Driver d={0};
	for (int mask=0;mask<16;mask++)
	{
		for (int domain=0;domain<4;domain++) NativePhysics_SetDomain((enum NativePhysicsDomain)domain,(mask>>domain)&1);
		assert(NativePhysics_ModeMask()==mask);
		NATIVE_PHYSICS_WRITE(&d,rotationSpinRate,0.25);
		NATIVE_PHYSICS_WRITE(&d,botData.aiPhysics.speedLinear,100.25);
		int size=NativePhysics_GetStateSize();
		void *snapshot=malloc(size);
		assert(snapshot && NativePhysics_CaptureState(snapshot,size));
		for (int domain=0;domain<4;domain++) NativePhysics_SetDomain((enum NativePhysicsDomain)domain,0);
		assert(NativePhysics_RestoreState(snapshot,size));
		assert(NativePhysics_ModeMask()==mask);
		near(NATIVE_PHYSICS_READ(&d,rotationSpinRate),0.25);
		near(NATIVE_PHYSICS_READ(&d,botData.aiPhysics.speedLinear),100.25);
		free(snapshot);
	}
	for (int domain=0;domain<4;domain++) NativePhysics_SetDomain((enum NativePhysicsDomain)domain,0);
	// Original AI preserves signed shifts and wrapping. Smoothed keeps fractions.
	near(NativeAI_Down(-3,1),-2);
	near(NativeAI_Add(2147483647,1),-2147483648.0);
	NativePhysics_SetDomain(NATIVE_PHYSICS_AI,1);
	near(NativeAI_Down(-3,1),-1.5);
	NATIVE_AI_WRITE(&d,botData.aiPhysics.speedLinear,100.25);
	NATIVE_AI_WRITE(&d,botData.aiPhysics.speedLinear,NativeAI_Add(NATIVE_AI_READ(&d,botData.aiPhysics.speedLinear),0.25));
	near(NATIVE_AI_READ(&d,botData.aiPhysics.speedLinear),100.5);
	near(NativeAI_Div(NativeAI_Mul(10.25,4096),100),419.84);
	NativePhysics_SetDomain(NATIVE_PHYSICS_AI,0);
	near(NATIVE_AI_READ(&d,botData.aiPhysics.speedLinear),101);
}
static void test_steering(void)
{
	struct Driver d={0};
	struct Terrain terrain={0};
	d.driverID=2;
	d.terrainMeta1=&terrain;
	d.AxisAngle1_normalVec.y=4096;
	d.simpTurnState=1;
	d.const_TurnInputDelay=3;
	terrain.turnResponseScale=128;
	sdata->gGT->elapsedTimeMS=16;
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,1);
	NativePhysics_Steer(&d);
	near(NATIVE_PHYSICS_READ(&d,rotationSpinRate),1.5);
	assert(NATIVE_PHYSICS_READ(&d,angle)>0 && NATIVE_PHYSICS_READ(&d,angle)<1);
	NativePhysics_Steer(&d);
	near(NATIVE_PHYSICS_READ(&d,rotationSpinRate),3);
	NATIVE_PHYSICS_WRITE(&d,angle,4095.75);
	NATIVE_PHYSICS_WRITE(&d,angle,NATIVE_PHYSICS_READ(&d,angle)+0.5);
	near(NATIVE_PHYSICS_READ(&d,angle),0.25);
	// Fractional rotation is orthonormal and does not lose speed on a round trip.
	NativePhysicsVec v={1.25,2.5,3.75};
	NativePhysicsVec rotated=NativePhysics_RotateDriver(&d,v,0);
	NativePhysicsVec restored=NativePhysics_RotateDriver(&d,rotated,1);
	near(restored.x,v.x);near(restored.y,v.y);near(restored.z,v.z);
	d.const_DriftSpinRateAccel=3;
	d.const_SteerVel_DriftStandard=2;
	d.const_TurnRate=4;
	d.const_DriftTurnRampFrames=1;
	d.const_DriftTurnBase=4;
	d.multDrift=256;
	d.KartStates.Drifting.driftTotalTimeMS=32;
	NATIVE_PHYSICS_WRITE(&d,rotationSpinRate,0);
	NativePhysics_DriftSteer(&d);
	near(NATIVE_PHYSICS_READ(&d,rotationSpinRate),1.5);
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,0);
}
static void test_collisions(void)
{
	NativePhysicsVec a={-10,0,-10},b={10,0,-10},c={0,0,10};
	NativeCollisionHit hit;
	assert(NativeCollision_SweepTriangle((NativePhysicsVec){0,5,0},(NativePhysicsVec){0,-5,0},1,a,b,c,&hit));
	near(hit.fraction,0.4); near(hit.normal.y,1); assert(hit.feature==0);
	// Two-sided geometry and a fast sweep through a thin triangle.
	assert(NativeCollision_SweepTriangle((NativePhysicsVec){0,-100,0},(NativePhysicsVec){0,100,0},1,a,b,c,&hit));
	near(hit.fraction,0.495);near(hit.normal.y,-1);
	// Grazing an edge and a vertex uses capsule/sphere roots, not a face-only test.
	assert(NativeCollision_SweepTriangle((NativePhysicsVec){0,5,-10.5},(NativePhysicsVec){0,-5,-10.5},1,a,b,c,&hit));
	assert(hit.feature!=0 && hit.fraction>0.4 && hit.fraction<0.5);
	assert(NativeCollision_SweepTriangle((NativePhysicsVec){-10.5,5,-10.5},(NativePhysicsVec){-10.5,-5,-10.5},1,a,b,c,&hit));
	assert(hit.feature>=4);
	assert(!NativeCollision_SweepTriangle((NativePhysicsVec){30,5,30},(NativePhysicsVec){30,-5,30},1,a,b,c,&hit));
	assert(!NativeCollision_SweepTriangle((NativePhysicsVec){0,1,0},(NativePhysicsVec){1,1,0},1,a,b,c,&hit));
	assert(!NativeCollision_SweepTriangle((NativePhysicsVec){0,5,0},(NativePhysicsVec){0,-5,0},1,a,a,a,&hit));
	double u,v;
	assert(NativeCollision_RayTriangle((NativePhysicsVec){0,0.125,0},(NativePhysicsVec){0,-0.375,0},a,b,c,&hit,&u,&v));
	near(hit.fraction,0.25);near(u+v,0.75);
}

int main(void)
{
	struct Driver d = {0}, other = {0};
	struct GameTracker gt = {0};
	struct Terrain terrain = {0};
	d.terrainMeta1 = d.terrainMeta2 = &terrain;
	terrain.groundFrictionScale = terrain.speedMultiplier = terrain.slowUntilSpeed = 256;
	sdata->gGT = &gt;
	gt.elapsedTimeMS = 16;
	d.matrixMovingDir.m[0][0] = d.matrixMovingDir.m[1][1] = d.matrixMovingDir.m[2][2] = 4096;
	assert(gNativeSmoothedPhysicsEnabled == 0);
	NativePhysics_SetEnabled(1);
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0.25, -0.25, 1.5});
	NativePhysicsVec v = NativePhysics_ReadVelocity(&d);
	near(v.x, 0.25); near(v.y, -0.25); near(v.z, 1.5);
	for (int i = 0; i < 8; i++) NativePhysics_Move(&d, NativePhysics_Step(&d, 16, 4096), 1);
	assert(d.posCurr.x == 1 && d.posCurr.y == -1 && d.posCurr.z == 6);
	// 30 and 60 FPS preserve the same displacement, including negative axes.
	other.driverID = 1;
	NativePhysics_WriteVelocity(&other, v);
	for (int i = 0; i < 4; i++) NativePhysics_Move(&other, NativePhysics_Step(&other, 32, 4096), 1);
	assert(memcmp(&other.posCurr, &d.posCurr, sizeof(Vec3)) == 0);
	// Collision fractions integrate only the accepted portion of the step.
	NativePhysics_Move(&d, (NativePhysicsVec){8, -8, 8}, 0.25);
	assert(d.posCurr.x == 3 && d.posCurr.y == -3 && d.posCurr.z == 8);
	// A collision hard stop replaces just its axis; other fractions survive.
	d.velocity.z = 0;
	v = NativePhysics_ReadVelocity(&d);
	near(v.x, 0.25); near(v.z, 0);
	// A teleport/external position write supersedes the accumulated position.
	d.posCurr.x = 1000;
	NativePhysics_Move(&d, (NativePhysicsVec){0.25, 0, 0}, 1);
	near(s_physics.drivers[0].position.x, 1000.25);
	// Speed and direction round-trip without quantizing their continuous state.
	v = (NativePhysicsVec){100.125, 50.25, -200.75};
	NativePhysics_ConvertVecToSpeed(&d, v);
	NativePhysics_ConvertSpeedToVec(&d, &d.velocity);
	NativePhysicsVec result = NativePhysics_ReadVelocity(&d);
	near(result.x, v.x); near(result.y, v.y); near(result.z, v.z);
	// Gravity retains half-unit impulses instead of rounding each frame.
	d.const_Gravity = 3;
	d.const_TerminalVelocity = 20000;
	d.const_SideSpeedClamp = 20000;
	d.const_SlopeForwardSpeedBonus = 20000;
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0, 0, 0});
	NativePhysics_Gravity(&d, &d.velocity);
	near(NativePhysics_ReadVelocity(&d).y, -1.5);
	NativePhysics_Gravity(&d, &d.velocity);
	near(NativePhysics_ReadVelocity(&d).y, -3);
	// Low-gravity surfaces apply the original 41% rule without truncation.
	struct QuadBlock quad = {0};
	quad.quadFlags = 2;
	d.underDriver = &quad;
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0, 0, 0});
	NativePhysics_Gravity(&d, &d.velocity);
	near(NativePhysics_ReadVelocity(&d).y, -0.615);
	d.underDriver = NULL;
	// Ground friction approaches zero without losing its half-unit step.
	d.actionsFlagSetPrevFrame = ACTION_TOUCH_GROUND;
	d.const_NoPedalFriction_Forward = d.const_NoPedalFriction_Perpendicular = 3;
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0, 0, 100});
	NativePhysics_Gravity(&d, &d.velocity);
	near(NativePhysics_ReadVelocity(&d).z, 98.5);
	NativePhysics_Gravity(&d, &d.velocity);
	near(NativePhysics_ReadVelocity(&d).z, 97);
	d.actionsFlagSetPrevFrame = 0;
	// Ground acceleration retains fractional impulses across solver updates.
	d.actionsFlagSet = ACTION_TOUCH_GROUND;
	d.baseSpeed = 200;
	d.const_Accel_ClassStat = 1;
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0, 0, 100});
	NativePhysics_JumpAndFriction(&d);
	near(NativePhysics_GetSpeed(&d), 100.5);
	NativePhysics_ConvertSpeedToVec(&d, &d.velocity);
	NativePhysics_JumpAndFriction(&d);
	near(NativePhysics_GetSpeed(&d), 101);
	// A forced jump applies the authored impulse and original jump flags.
	struct Level level = {0};
	gt.level1 = &level;
	d.jump_ForcedMS = 1;
	d.jump_InitialVelY = 1001;
	d.const_JumpForce = 1001;
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0, 0, 101});
	NativePhysics_JumpAndFriction(&d);
	near(NativePhysics_ReadVelocity(&d).y, 1001);
	assert(d.actionsFlagSet & ACTION_JUMP_STARTED);
	assert(d.jump_CooldownMS == VEH_PHYS_JUMP_COOLDOWN_MS);
	// Snapshots reproduce the next step exactly and restore the selected mode.
	int size = NativePhysics_GetStateSize();
	void *snapshot = malloc(size);
	assert(snapshot && NativePhysics_CaptureState(snapshot, size));
	NativePhysics_Move(&d, NativePhysics_Step(&d, 16, 4096), 1);
	Vec3 expected = d.posCurr;
	struct Driver saved = d;
	// Restore the driver's integer state alongside the native snapshot.
	NativePhysics_SetEnabled(0);
	assert(NativePhysics_RestoreState(snapshot, size));
	d.posCurr = ((struct NativePhysicsState *)snapshot)->drivers[0].exportedPosition;
	d.velocity = saved.velocity;
	NativePhysics_Move(&d, NativePhysics_Step(&d, 16, 4096), 1);
	assert(memcmp(&d.posCurr, &expected, sizeof(Vec3)) == 0);
	assert(gNativeSmoothedPhysicsEnabled == 1);
	assert(!NativePhysics_RestoreState(snapshot, size - 1));
	free(snapshot);
	NativePhysics_SetEnabled(0);
	assert(!s_physics.drivers[0].valid);
	test_domains();
	test_steering();
	test_collisions();
	test_collision_response();
	puts("Smoothed physics, AI, steering and collision checks passed");
	return 0;
}
