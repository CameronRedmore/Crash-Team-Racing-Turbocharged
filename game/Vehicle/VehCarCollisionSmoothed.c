#include <common.h>
#include <math.h>
#include <vehicle_physics_constants.h>
#if defined(CTR_NATIVE) && !defined(__vita__)
static NativePhysicsVec NativeCar_Position(struct Driver *d)
{
	NativePhysicsVec pos = NativePhysics_ReadPosition(d);
	return (NativePhysicsVec){pos.x / 256.0, pos.y / 256.0, pos.z / 256.0};
}
static double NativeCar_DistanceSq(NativePhysicsVec a, NativePhysicsVec b)
{
	double x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
	return x * x + y * y + z * z;
}
void NativeCollision_CarSearch(struct Driver *d, struct Thread *first, struct BucketSearchParams *search)
{
	NativePhysicsVec position = NativeCar_Position(d);
	double best = P32_GET(struct Thread *, search->th)
	                  ? NativeCar_DistanceSq(position, NativeCar_Position(P32_GET(void *, P32_GET(struct Thread *, search->th)->object)))
	                  : 2147483647.0;
	for (struct Thread *th = first; th; th = P32_GET(struct Thread *, th->siblingThread))
	{
		if (P32_GET(void *, th->object) == d || P32_GET(void *, th->object) == NULL || (th->flags & (THREAD_FLAG_DEAD | THREAD_FLAG_DISABLE_COLLISION)))
			continue;
		NativePhysicsVec other = NativeCar_Position(P32_GET(void *, th->object));
		double distance = NativeCar_DistanceSq(position, other);
		if (distance >= best)
			continue;
		best = distance;
		search->th = th;
		search->bestDistSq = (s32)fmin(2147483647.0, ceil(distance));
		search->dist = (SVec3){.x = (s16)round(position.x - other.x), .y = (s16)round(position.y - other.y), .z = (s16)round(position.z - other.z)};
	}
}
static NativePhysicsVec NativeCar_Velocity(struct Driver *d)
{
	if (!(d->actionsFlagSet & ACTION_BOT))
		return NativePhysics_ReadVelocity(d);
	if (!CTR_NATIVE_SMOOTHED_AI_ACTIVE)
		return (NativePhysicsVec){d->xSpeed + d->botData.aiPhysics.accel.x, d->ySpeed + d->botData.aiPhysics.accel.y, d->zSpeed + d->botData.aiPhysics.accel.z};
	double speed = NATIVE_PHYSICS_READ(d, botData.aiPhysics.speedLinear);
	double yaw = NATIVE_PHYSICS_READ(d, botData.aiRot.y) * (6.2831853071795864769 / 4096.0);
	return (NativePhysicsVec){speed * sin(yaw) + NATIVE_PHYSICS_READ(d, botData.aiPhysics.accel.x),
	                          NATIVE_PHYSICS_READ(d, botData.aiPhysics.speedY) + NATIVE_PHYSICS_READ(d, botData.aiPhysics.accel.y),
	                          speed * cos(yaw) + NATIVE_PHYSICS_READ(d, botData.aiPhysics.accel.z)};
}
static void NativeCar_Apply(struct Driver *d, NativePhysicsVec v)
{
	NativePhysics_WriteVelocity(d, v);
	if (!(d->actionsFlagSet & ACTION_BOT))
	{
		NativePhysics_ConvertVecToSpeed(d, v);
		return;
	}
	// Project onto the AI nav tangent in floating point; path and damage flags
	// continue to use the original game state machine.
	double pitch = P32_GET(struct NavFrame *, d->botData.botNavFrame)->rot[0] * (6.2831853071795864769 / 256.0);
	double yaw = P32_GET(struct NavFrame *, d->botData.botNavFrame)->rot[1] * (6.2831853071795864769 / 256.0);
	NativePhysicsVec forward = {sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)};
	double speed = forward.x * v.x + forward.y * v.y + forward.z * v.z;
	NATIVE_PHYSICS_WRITE(d, botData.aiPhysics.speedLinear, speed);
	NATIVE_PHYSICS_WRITE(d, botData.aiPhysics.accel.x, v.x - forward.x * speed);
	NATIVE_PHYSICS_WRITE(d, botData.aiPhysics.accel.z, v.z - forward.z * speed);
	d->botData.botFlags |= BOT_FLAG_FREE_PHYSICS;
}
static NativePhysicsVec NativeCar_Bounce(NativePhysicsVec v, NativePhysicsVec center, NativePhysicsVec normal, int other)
{
	NativePhysicsVec delta = {v.x - center.x, v.y - center.y, v.z - center.z};
	double dot = delta.x * normal.x + delta.y * normal.y + delta.z * normal.z;
	if (other ? dot <= 0 : dot >= 0)
		return v;
	sdata->vehicleCollisionImpactStrength = (s32)fmax(sdata->vehicleCollisionImpactStrength, fabs(dot));
	// Original restitution removes 4/3 of the normal relative velocity.
	NativePhysicsVec result = {v.x - normal.x * dot * (4.0 / 3.0), v.y - normal.y * dot * (4.0 / 3.0), v.z - normal.z * dot * (4.0 / 3.0)};
	if (result.y > v.y && result.y > VEH_PHYS_CRASH_BOUNCE_Y_CLAMP)
		result.y = VEH_PHYS_CRASH_BOUNCE_Y_CLAMP;
	return result;
}
void NativeCollision_Cars(struct Thread *thread, struct DriverCollisionSearch *search, Vec3 *output)
{
	struct Thread *otherThread = P32_GET(struct Thread *, search->bucket.th);
	struct Driver *self = P32_GET(void *, thread->object), *other = P32_GET(void *, otherThread->object);
	NativePhysicsVec a = NativeCar_Position(self), b = NativeCar_Position(other);
	double distance = sqrt(NativeCar_DistanceSq(a, b));
	double strength = thread->driverHitRadius + otherThread->driverHitRadius - distance;
	if (strength <= 0)
		return;
	NativePhysicsVec normal =
	    distance > 1e-12 ? (NativePhysicsVec){(a.x - b.x) / distance, (a.y - b.y) / distance, (a.z - b.z) / distance} : (NativePhysicsVec){0, 0, 1};
	search->hitDir = (SVec3){.x = (s16)round(normal.x * 4096), .y = (s16)round(normal.y * 4096), .z = (s16)round(normal.z * 4096)};
	NativePhysicsVec va = NativeCar_Velocity(self), vb = NativeCar_Velocity(other);
	double wa = self->const_CollisionWeight, wb = other->const_CollisionWeight, total = wa + wb;
	if (total <= 0)
	{
		wa = wb = 1;
		total = 2;
	}
	NativePhysicsVec center = {(va.x * wa + vb.x * wb) / total, (va.y * wa + vb.y * wb) / total, (va.z * wa + vb.z * wb) / total};
	sdata->vehicleCollisionImpactStrength = 0;
	va = NativeCar_Bounce(va, center, normal, 0);
	vb = NativeCar_Bounce(vb, center, normal, 1);
	double separationImpulse = strength * 16 * NativePhysics_FrameScale();
	va.x += normal.x * separationImpulse;
	va.y += normal.y * separationImpulse;
	va.z += normal.z * separationImpulse;
	vb.x -= normal.x * separationImpulse;
	vb.y -= normal.y * separationImpulse;
	vb.z -= normal.z * separationImpulse;
	NativeCar_Apply(self, va);
	NativeCar_Apply(other, vb);
	*output = self->velocity;
	if (self->actionsFlagSet & ACTION_BOT)
	{
		if (other->actionsFlagSet & ACTION_BOT)
			BOTS_CollideWithOtherAI(self, other);
		return;
	}
	u32 feedback = ((u32)CTR_MipsSubLo(P32_GET(struct GameTracker *, sdata->gGT)->frameTimer_MainFrame_ResetDB, sdata->audioDefaults[8]) >=
	                VEH_PHYS_CRASH_FEEDBACK_COOLDOWN_FRAMES);
	VehPhysCrash_PlayHumanFeedback(thread, otherThread, self, other, feedback);
	int attack = VehPhysCrash_Attack(self, other, feedback, 0);
	VehPhysCrash_Attack(other, self, attack, 1);
}
#endif
