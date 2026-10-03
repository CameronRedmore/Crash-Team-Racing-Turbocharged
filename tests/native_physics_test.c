#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <common.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Exercise the actual solver without a renderer or disc image.
#include "../platform/native_ptr32.c"
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

void GhostTape_WriteBoosts(int reserves, u8 type, int cap) { (void)reserves; (void)type; (void)cap; }
void VehTurbo_ThTick(struct Thread *t) { (void)t; }
void VehTurbo_ThDestroy(struct Thread *t) { (void)t; }
struct Instance *INSTANCE_Birth3D(struct Model *model, const char *name, struct Thread *t)
{ (void)model; (void)name; (void)t; assert(0); return NULL; }
struct Instance *INSTANCE_BirthWithThread(int model, const char *name, int pool, int bucket, void *tick, int size, struct Thread *parent)
{ (void)model; (void)name; (void)pool; (void)bucket; (void)tick; (void)size; (void)parent; assert(0); return NULL; }
#include "../game/Vehicle/VehFire.c"

static void near(double a, double b) { assert(fabs(a - b) < 0.000001); }

// Objects whose address is stored in a game pointer field must be static: on
// 64-bit builds those fields are CtrPtr32 handles, which cannot reach the stack.

static void test_frame_rates(void)
{
	struct Driver d={.driverID=8};
	static struct Terrain terrain={.turnResponseScale=128};
	NativePhysics_SetEnabled(1);
	NativePhysics_SetDomain(NATIVE_PHYSICS_AI,1);
	for (int option=0;option<NATIVE_FRAME_RATE_COUNT;option++)
	{
		gNative60FpsEnabled=option;
		int rate=CTR_FRAMES_PER_SECOND;
		NativePhysics_ResetDriver(&d);
		P32_SET(d.terrainMeta1, &terrain);
		d.posCurr=(Vec3){0};
		NativePhysics_WriteVelocity(&d,(NativePhysicsVec){1,-1.25,2.5});
		double acceleration=0, damping=1024;
		for (int frame=0;frame<rate;frame++)
		{
			P32_GET(struct GameTracker *, sdata->gGT)->timer=frame;
			P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
			NativePhysics_Move(&d,NativePhysics_Step(&d,P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS,4096),1);
			acceleration+=NativeAI_FrameStep(3.25,frame);
			damping=NativeAI_HalfDecay(damping,frame);
		}
		NativePhysicsVec p=NativePhysics_ReadPosition(&d);
		near(p.x,30); near(p.y,-37.5); near(p.z,75);
		near(acceleration,97.5);
		near(damping/ldexp(1024,-30),1);
		NativePhysics_ResetDriver(&d);
		d.matrixMovingDir.m[0][0]=d.matrixMovingDir.m[1][1]=d.matrixMovingDir.m[2][2]=4096;
		d.const_Gravity=3; d.const_TerminalVelocity=30000;
		NativePhysics_WriteVelocity(&d,(NativePhysicsVec){0});
		for (int frame=0;frame<rate;frame++)
		{
			P32_GET(struct GameTracker *, sdata->gGT)->timer=frame;
			P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
			NativePhysics_Gravity(&d,&d.velocity);
		}
		near(NativePhysics_ReadVelocity(&d).y,-90);
		// Oxide Station's low-gravity quads retain the same 41% gravity at
		// every simulation rate, independently of the adhesion multiplier.
		static struct QuadBlock lowGravity;
		lowGravity=(struct QuadBlock){.quadFlags=QUADBLOCK_FLAG_LOW_GRAVITY};
		P32_SET(d.underDriver, &lowGravity);
		NativePhysics_WriteVelocity(&d,(NativePhysicsVec){0});
		for (int frame=0;frame<rate;frame++)
		{
			P32_GET(struct GameTracker *, sdata->gGT)->timer=frame;
			P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
			NativePhysics_Gravity(&d,&d.velocity);
		}
		near(NativePhysics_ReadVelocity(&d).y,-36.9);
		// A single steering impulse uses the same retail time scale at every rate.
		memset(&d,0,sizeof(d)); d.driverID=8;
		P32_SET(d.terrainMeta1, &terrain); d.AxisAngle1_normalVec.y=4096;
		d.simpTurnState=1; d.const_TurnInputDelay=3;
		NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,1);
		P32_GET(struct GameTracker *, sdata->gGT)->timer=0;
		P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,0);
		NativePhysics_Steer(&d);
		near(NATIVE_PHYSICS_READ(&d,rotationSpinRate),1.5*30/rate);
		NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,0);
		static struct Driver a, b;
		static struct Thread ta, tb;
		a=(struct Driver){.driverID=9,.const_CollisionWeight=1}; b=(struct Driver){.driverID=10,.const_CollisionWeight=1};
		ta=(struct Thread){.driverHitRadius=10}; tb=(struct Thread){.driverHitRadius=10};
		P32_SET(ta.object, &a); P32_SET(tb.object, &b);
		struct DriverCollisionSearch search={0}; P32_SET(search.bucket.th, &tb);
		Vec3 output;
		NativePhysics_SetDomain(NATIVE_PHYSICS_COLLISION,1);
		NativePhysics_WritePosition(&b,(NativePhysicsVec){19.25*256,0,0});
		NativeCollision_Cars(&ta,&search,&output);
		near(NativePhysics_ReadVelocity(&a).x,-12.0*30/rate);
		near(NativePhysics_ReadVelocity(&b).x,12.0*30/rate);
		NativePhysics_SetDomain(NATIVE_PHYSICS_COLLISION,0);
		// Forced 30 FPS and ghost overrides take precedence over the menu rate.
		gNativeForce30Fps=1;
		near(NativePhysics_FrameScale(),1);
		gNativeForce30Fps=0;
		gNativeGhostReplayFpsOverride=1;
		near(NativePhysics_FrameScale(),0.5);
		gNativeGhostReplayFpsOverride=-1;
	}
	gNative60FpsEnabled=0;
	NativePhysics_SetEnabled(0);
	NativePhysics_SetDomain(NATIVE_PHYSICS_AI,0);
}

static void test_mud_drag(void)
{
	// Isolate the speed-dependent mud rules from ordinary constant friction.
	static struct Terrain terrain={.flags=TERRAIN_FLAG_MUD_PHYSICS,.groundFrictionScale=512,.speedMultiplier=256};
	NativePhysics_SetEnabled(1);
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,0);
	for (int option=0;option<NATIVE_FRAME_RATE_COUNT;option++)
	{
		gNative60FpsEnabled=option;
		int rate=CTR_FRAMES_PER_SECOND;
		for (int direction=-1;direction<=1;direction+=2)
		{
			struct Driver d={.driverID=12,
				.actionsFlagSet=ACTION_TOUCH_GROUND,.actionsFlagSetPrevFrame=ACTION_TOUCH_GROUND,
				.baseSpeed=direction*4096,.const_SlopeForwardSpeedBonus=30000,
				.const_SideSpeedClamp=30000,.const_TerminalVelocity=30000};
			P32_SET(d.terrainMeta1, &terrain); P32_SET(d.terrainMeta2, &terrain);
			d.matrixMovingDir.m[0][0]=d.matrixMovingDir.m[1][1]=d.matrixMovingDir.m[2][2]=4096;
			// Also exercise braking against the direction of travel.
			for (int opposing=0;opposing<=1;opposing++)
			{
				d.baseSpeed=direction*(opposing ? -4096 : 4096);
				NativePhysics_ResetDriver(&d);
				NativePhysics_WriteVelocity(&d,(NativePhysicsVec){direction*8192,0,direction*8192});
				for (int frame=0;frame<rate;frame++)
				{
					P32_GET(struct GameTracker *, sdata->gGT)->timer=frame;
					P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
					d.speedApprox=d.velocity.z;
					NativePhysics_Gravity(&d,&d.velocity);
				}
				NativePhysicsVec v=NativePhysics_ReadVelocity(&d);
				near(v.x,direction*8192*pow(0.875,30));
				near(v.z,direction*(opposing ? 8192*pow(0.5,30) : 4096+4096*pow(0.5,30)));
			}
			// Mask protection bypasses mud damping.
			d.actionsFlagSet|=ACTION_MASK_WEAPON;
			NativePhysics_WriteVelocity(&d,(NativePhysicsVec){direction*8192,0,direction*8192});
			NativePhysics_Gravity(&d,&d.velocity);
			near(NativePhysics_ReadVelocity(&d).x,direction*8192);
			near(NativePhysics_ReadVelocity(&d).z,direction*8192);
		}
	}
	gNative60FpsEnabled=0;
	NativePhysics_SetEnabled(0);
}

static void test_surface_forces(void)
{
	struct Driver d={.driverID=11, .actionsFlagSet=ACTION_TOUCH_GROUND, .baseSpeed=4096};
	static struct Terrain terrain={.slowUntilSpeed=256};
	static struct QuadBlock quad={.mulNormVecY=-127}; // Sewer Speedway's authored adhesion.
	P32_SET(d.terrainMeta1, &terrain);
	P32_SET(d.underDriver, &quad);
	// A banked ramp: adhesion has both a sideways and a downward component.
	d.matrixMovingDir.m[0][0]=d.matrixMovingDir.m[1][1]=3276;
	d.matrixMovingDir.m[0][1]=2457;
	d.matrixMovingDir.m[1][0]=-2457;
	d.matrixMovingDir.m[2][2]=4096;
	NativePhysics_SetEnabled(1);
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING,0);
	for (int option=0;option<NATIVE_FRAME_RATE_COUNT;option++)
	{
		gNative60FpsEnabled=option;
		int rate=CTR_FRAMES_PER_SECOND;
		for (int direction=-1;direction<=1;direction+=2)
		{
			NativePhysicsVec total={0};
			for (int frame=0;frame<rate;frame++)
			{
				P32_GET(struct GameTracker *, sdata->gGT)->timer=frame;
				P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
				// Hold approach speed constant to measure the authored force,
				// independently of acceleration and subsequent contact responses.
				d.speedApprox=direction*4096;
				NativePhysics_WriteVelocity(&d,(NativePhysicsVec){0,0,direction*4096});
				NativePhysics_JumpAndFriction(&d);
				NativePhysicsVec v=NativePhysics_ReadVelocity(&d);
				total.x+=v.x; total.y+=v.y;
				near(v.z,direction*4096);
				assert(!(d.actionsFlagSet & ACTION_JUMP_STARTED));
			}
			near(total.x,-2032.0*2457/4096*30);
			near(total.y,-2032.0*3276/4096*30);
		}
		// Tiger Temple has three quads using the same rule at -64 strength.
		quad.mulNormVecY=-64;
		d.speedApprox=4096;
		NativePhysics_WriteVelocity(&d,(NativePhysicsVec){0,0,4096});
		NativePhysics_JumpAndFriction(&d);
		near(NativePhysics_ReadVelocity(&d).x,-1024.0*2457/4096*30/rate);
		near(NativePhysics_ReadVelocity(&d).y,-1024.0*3276/4096*30/rate);
		quad.mulNormVecY=-127;
		// Persistent penetration recovery produces the same total impulse.
		d.collisionFlags=DRIVER_COLL_FLAG_SURFACE_PUSHBACK;
		d.spsNormalVec=(SVec3){.x=4096};
		d.spsHitPos=(SVec3){.x=10,.y=20,.z=30};
		NativePhysics_WritePosition(&d,(NativePhysicsVec){9.5*256,21.25*256,28.5*256});
		NativePhysics_WriteVelocity(&d,(NativePhysicsVec){0.25,-0.5,0.75});
		for (int frame=0;frame<rate;frame++)
		{
			P32_GET(struct GameTracker *, sdata->gGT)->timer=frame;
			P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
			NativePhysics_SurfacePushback(&d);
		}
		NativePhysicsVec v=NativePhysics_ReadVelocity(&d);
		near(v.x,0.25-32*30); near(v.y,-0.5+80*30); near(v.z,0.75-96*30);
		// No correction without a contact, or once on the allowed side.
		d.collisionFlags=0;
		NativePhysics_SurfacePushback(&d);
		near(NativePhysics_ReadVelocity(&d).x,v.x);
		d.collisionFlags=DRIVER_COLL_FLAG_SURFACE_PUSHBACK;
		NativePhysics_WritePosition(&d,(NativePhysicsVec){11*256,21.25*256,28.5*256});
		NativePhysics_SurfacePushback(&d);
		near(NativePhysics_ReadVelocity(&d).x,v.x);
	}
	// The ramp force ceases when the kart leaves the surface.
	d.actionsFlagSet=0;
	NativePhysics_WriteVelocity(&d,(NativePhysicsVec){0,0,4096});
	NativePhysics_JumpAndFriction(&d);
	near(NativePhysics_ReadVelocity(&d).x,0); near(NativePhysics_ReadVelocity(&d).y,0);
	gNative60FpsEnabled=0;
	NativePhysics_SetEnabled(0);
}

static void test_collision_response(void)
{
	static struct Driver d, other;
	static struct Instance inst;
	static struct Thread t={.driverHitRadius=10}, ot={.driverHitRadius=10};
	P32_SET(t.object, &d); P32_SET(ot.object, &other);
	struct ScratchpadStruct sps={0};
	struct Scrub scrub={.flags=SCRUB_FLAG_APPLY_IMPACT};
	Vec3 output;
	d.driverID=4; other.driverID=5; P32_SET(d.instSelf, &inst);
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
	struct DriverCollisionSearch search={0}; P32_SET(search.bucket.th, &ot);
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
	static struct Terrain terrain;
	d.driverID=2;
	P32_SET(d.terrainMeta1, &terrain);
	d.AxisAngle1_normalVec.y=4096;
	d.simpTurnState=1;
	d.const_TurnInputDelay=3;
	terrain.turnResponseScale=128;
	P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=16;
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

static void test_pad_contact(void)
{
	static struct QuadBlock pad={.quadFlags=QUADBLOCK_FLAG_TRIGGER,.terrain_type=COLL_STEP_TRIGGER_TURBO_PAD};
	static struct BspSearchVertex a={.pos={.x=-10,.y=0,.z=-10}}, b={.pos={.x=10,.y=0,.z=-10}}, c={.pos={.x=0,.y=0,.z=10}};
	for (int option=0;option<NATIVE_FRAME_RATE_COUNT;option++)
	{
		gNative60FpsEnabled=option;
		int rate=CTR_FRAMES_PER_SECOND;
		// Stay on the pad for a second while moving parallel to its face.
		for (int frame=0;frame<rate;frame++)
		{
			struct ScratchpadStruct sps={0};
			P32_SET(sps.candidate.ptrQuadblock, &pad);
			sps.Input1.hitRadius=1;
			sps.hitFraction=4096;
			NativeCollision_BeginSweep(&sps,(NativePhysicsVec){-1+2.0*frame/rate,1,0},(NativePhysicsVec){2.0/rate,0,0});
			NativeCollision_MovedTriangle(&sps,&a,&b,&c);
			assert(sps.collision.stepFlags==COLL_STEP_TRIGGER_TURBO_PAD);
			assert(sps.boolDidTouchQuadblock==0 && sps.hitFraction==4096);
			NativeCollision_EndSweep();
		}
	}
	struct ScratchpadStruct sps={0};
	P32_SET(sps.candidate.ptrQuadblock, &pad);
	sps.Input1.hitRadius=1;
	NativeCollision_BeginSweep(&sps,(NativePhysicsVec){0,1,0},(NativePhysicsVec){0});
	NativeCollision_MovedTriangle(&sps,&a,&b,&c);
	assert(sps.collision.stepFlags==COLL_STEP_TRIGGER_TURBO_PAD);
	NativeCollision_EndSweep();
	sps.collision.stepFlags=0;
	NativeCollision_BeginSweep(&sps,(NativePhysicsVec){0,2,0},(NativePhysicsVec){0});
	NativeCollision_MovedTriangle(&sps,&a,&b,&c);
	assert(sps.collision.stepFlags==0);
	NativeCollision_EndSweep();
}

static void test_pad_boost_counter(void)
{
	struct GameTracker *gt=P32_GET(struct GameTracker *, sdata->gGT);
	static struct Thread player={.modelIndex=DYNAMIC_PLAYER}, turboThread;
	static struct Instance inst, flame1, flame2;
	static struct Driver d={.driverID=12,.numTurbos=3,.kartState=KS_NORMAL};
	static struct Turbo turbo;
	P32_SET(inst.thread, &player); P32_SET(d.instSelf, &inst);
	P32_SET(turbo.driver, &d); P32_SET(turbo.inst, &flame2);
	P32_SET(turboThread.object, &turbo); P32_SET(turboThread.inst, &flame1);
	struct Thread *saved=P32_GET(struct Thread *, gt->threadBuckets[TURBO].thread);
	P32_SET(gt->threadBuckets[TURBO].thread, &turboThread);
	for (int option=0;option<NATIVE_FRAME_RATE_COUNT;option++)
	{
		gNative60FpsEnabled=option;
		NativePhysics_ResetDriver(&d);
		d.numTurbos=3;
		for (int frame=0;frame<CTR_FRAMES_PER_SECOND;frame++)
		{
			gt->timer=frame;
			gt->elapsedTimeMS=CTR_FRAME_STEP(32,frame);
			// A missed contact every other high-rate step must not rearm entry.
			u32 flags=frame%2 && CTR_FRAMES_PER_SECOND>30 ? 0 : COLL_STEP_TRIGGER_TURBO_PAD;
			NativePhysics_UpdateTurboPadContact(&d,flags);
			if (!flags) continue;
			// Exercise the actual boost function even if its action flag was lost.
			d.actionsFlagSet=d.actionsFlagSetPrevFrame=0;
			d.stepFlagSet=flags;
			VehFire_Increment(&d,960,TURBO_PAD|FREEZE_RESERVES_ON_TURBO_PAD,256);
			assert(d.numTurbos==4);
		}
		int size=NativePhysics_GetStateSize();
		void *snapshot=malloc(size);
		assert(snapshot && NativePhysics_CaptureState(snapshot,size));
		NativePhysics_ResetDriver(&d);
		assert(NativePhysics_ConsumeTurboPadEntry(&d));
		assert(NativePhysics_RestoreState(snapshot,size));
		assert(!NativePhysics_ConsumeTurboPadEntry(&d));
		free(snapshot);
		gt->elapsedTimeMS=32;
		NativePhysics_UpdateTurboPadContact(&d,0);
		NativePhysics_UpdateTurboPadContact(&d,COLL_STEP_TRIGGER_TURBO_PAD);
		VehFire_Increment(&d,960,TURBO_PAD|FREEZE_RESERVES_ON_TURBO_PAD,256);
		assert(d.numTurbos==5);
	}
	P32_SET(gt->threadBuckets[TURBO].thread, saved);
}

static void test_road_seam_normals(void)
{
	static struct QuadBlock road={.quadFlags=QUADBLOCK_FLAG_GROUND};
	// Road triangle beneath Roo's Tubes pad 935, from the installed 1P level.
	static struct BspSearchVertex a={.pos={.x=1724,.y=-788,.z=-12493}},
	                      b={.pos={.x=1879,.y=-794,.z=-12606}},
	                      c={.pos={.x=1835,.y=-810,.z=-12338}};
	NativePhysicsVec av=NC_Vertex(&a.pos),bv=NC_Vertex(&b.pos),cv=NC_Vertex(&c.pos);
	NativePhysicsVec n=NC_Normalize(NC_Cross(NC_Sub(bv,av),NC_Sub(cv,av)));
	if (n.y<0) n=NC_Scale(n,-1);
	a.plane.normal=NC_Export(NC_Scale(n,4096));
	NativePhysicsVec middle=NC_Scale(NC_Add(av,bv),0.5), edge=NC_Normalize(NC_Sub(bv,av));
	NativePhysicsVec across=NC_Normalize(NC_Cross(edge,n));
	if (NC_Dot(across,NC_Sub(cv,middle))<0) across=NC_Scale(across,-1);
	for (int inward=0;inward<2;inward++)
	{
		struct ScratchpadStruct s={0};
		P32_SET(s.candidate.ptrQuadblock, &road); s.Input1.hitRadius=25; s.hitFraction=4096;
		NativePhysicsVec start=NC_Add(NC_Sub(middle,NC_Scale(across,40)),NC_Scale(n,24));
		NativeCollision_BeginSweep(&s,start,NC_Sub(NC_Scale(across,80),NC_Scale(n,inward)));
		NativeCollision_MovedTriangle(&s,&a,&b,&c);
		if (!inward) assert(s.boolDidTouchQuadblock==0);
		else
		{
			assert(s.boolDidTouchQuadblock==1);
			NativePhysicsVec normal=NativeCollision_Normal(&s);
			assert(NC_Dot(NC_Normalize(normal),n)>0.999999);
		}
		NativeCollision_EndSweep();
	}
}

static void test_slope_contact_pushback(void)
{
	// A kart resting on an uphill road touches it at the start of each step.
	// Surface pushback must not read the step itself as penetration; when the
	// contact point was exported, speed grew by a quarter per 30 FPS frame.
	static struct QuadBlock road={.quadFlags=QUADBLOCK_FLAG_GROUND};
	NativePhysicsVec n=NC_Normalize((NativePhysicsVec){-734,4024,204});
	NativePhysicsVec side=NC_Normalize(NC_Cross(n,(NativePhysicsVec){0,0,1})), fwd=NC_Cross(side,n);
	NativePhysicsVec origin={-13766,17,-1578};
	static struct BspSearchVertex a,b,c;
	a.pos=NC_Export(NC_Add(origin,NC_Scale(fwd,-2000)));
	b.pos=NC_Export(NC_Add(origin,NC_Add(NC_Scale(fwd,2000),NC_Scale(side,2000))));
	c.pos=NC_Export(NC_Add(origin,NC_Add(NC_Scale(fwd,2000),NC_Scale(side,-2000))));
	NativePhysicsVec av=NC_Vertex(&a.pos),bv=NC_Vertex(&b.pos),cv=NC_Vertex(&c.pos);
	NativePhysicsVec face=NC_Normalize(NC_Cross(NC_Sub(bv,av),NC_Sub(cv,av)));
	if (face.y<0) face=NC_Scale(face,-1);
	a.plane.normal=NC_Export(NC_Scale(face,4096));
	NativePhysicsVec onPlane=NC_Sub(origin,NC_Scale(face,NC_Dot(NC_Sub(origin,av),face)));
	for (int option=0;option<NATIVE_FRAME_RATE_COUNT;option++)
	{
		gNative60FpsEnabled=option;
		double elapsed=32.0*30/CTR_FRAMES_PER_SECOND;
		struct Driver d={.driverID=7};
		NativePhysicsVec velocity={12812,1609,-3908};
		NativePhysicsVec start=NC_Add(onPlane,NC_Scale(face,25));
		struct ScratchpadStruct s={0};
		P32_SET(s.candidate.ptrQuadblock, &road); s.Input1.hitRadius=25; s.hitFraction=4096;
		NativeCollision_BeginSweep(&s,start,NC_Scale(velocity,elapsed/32/256));
		NativeCollision_MovedTriangle(&s,&a,&b,&c);
		assert(s.boolDidTouchQuadblock==1 && NativeCollision_HitFraction(&s)<1e-9);
		NativeCollision_EndSweep();
		// The impact keeps tangential motion, which then completes the step.
		NativePhysicsVec tangent=NC_Sub(velocity,NC_Scale(face,NC_Dot(velocity,face)));
		NativePhysicsVec end=NC_Add(start,NC_Scale(tangent,elapsed/32/256));
		NativePhysics_WritePosition(&d,NC_Scale(NC_Sub(end,NC_Scale(face,25)),256));
		NativePhysics_WriteVelocity(&d,tangent);
		d.collisionFlags=DRIVER_COLL_FLAG_SURFACE_PUSHBACK;
		d.spsHitPos=s.hit.hitPos;
		d.spsNormalVec=s.hit.plane.normal;
		d.quadBlockHeight=(s32)(onPlane.y*256);
		P32_GET(struct GameTracker *, sdata->gGT)->timer=0;
		P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS=(s32)elapsed;
		NativePhysics_SurfacePushback(&d);
		NativePhysicsVec after=NativePhysics_ReadVelocity(&d);
		// Only integer hit position rounding may remain, as in retail.
		assert(sqrt(NC_Dot(NC_Sub(after,tangent),NC_Sub(after,tangent)))<64*elapsed/32*1.0);
	}
	gNative60FpsEnabled=0;
}

int main(void)
{
	struct Driver d = {0}, other = {0};
	static struct GameTracker gt;
	static struct Terrain terrain;
	P32_SET(d.terrainMeta1, &terrain);
	P32_SET(d.terrainMeta2, &terrain);
	terrain.groundFrictionScale = terrain.speedMultiplier = terrain.slowUntilSpeed = 256;
	P32_SET(sdata->gGT, &gt);
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
	static struct QuadBlock quad;
	quad.quadFlags = 2;
	P32_SET(d.underDriver, &quad);
	NativePhysics_WriteVelocity(&d, (NativePhysicsVec){0, 0, 0});
	NativePhysics_Gravity(&d, &d.velocity);
	near(NativePhysics_ReadVelocity(&d).y, -0.615);
	P32_SET(d.underDriver, NULL);
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
	static struct Level level;
	P32_SET(gt.level1, &level);
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
	test_pad_contact();
	test_pad_boost_counter();
	test_road_seam_normals();
	test_slope_contact_pushback();
	test_collision_response();
	test_surface_forces();
	test_mud_drag();
	test_frame_rates();
	puts("Smoothed physics, AI, steering and collision checks passed");
	return 0;
}
