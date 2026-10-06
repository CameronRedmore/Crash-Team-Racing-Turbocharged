#include <common.h>
#include <platform/native_physics.h>
#include <math.h>
#include <string.h>

int gNativeSmoothedPhysicsEnabled = 0;
int gNativeSmoothedAIEnabled = 0;
#if defined(__vita__)
int gNativeSmoothedCollisionEnabled = 0;
#else
int gNativeSmoothedCollisionEnabled = 1;
#endif
int gNativeSmoothedSteeringEnabled = 0;

static int NativePhysics_ModeMask(void)
{
	return (gNativeSmoothedPhysicsEnabled != 0) | ((gNativeSmoothedAIEnabled != 0) << 1) | ((CTR_NATIVE_SMOOTHED_COLLISION_ACTIVE) << 2) |
	       ((gNativeSmoothedSteeringEnabled != 0) << 3);
}
struct NativePhysicsScalar
{
	u32 offset, size;
	s32 exported;
	double value;
};

// Slots use driver IDs, rather than pointers, so snapshots survive relocation.
#define NATIVE_PHYSICS_DRIVER_COUNT 256
struct NativePhysicsDriverState
{
	int valid;
	struct NativePhysicsScalar scalars[64];
	NativePhysicsVec velocity;
	NativePhysicsVec position;
	Vec3 exportedVelocity;
	Vec3 exportedPosition;
	double speed, yaw, pitch;
	s16 exportedSpeed, exportedYaw, exportedPitch;
	int turboPadBoosted;
	double turboPadAbsentMS;
};
struct NativePhysicsState
{
	int enabled;
	struct NativePhysicsDriverState drivers[NATIVE_PHYSICS_DRIVER_COUNT];
};
static struct NativePhysicsState s_physics;
static struct NativePhysicsDriverState *NativePhysics_Driver(struct Driver *d);

double NativePhysics_FrameScale(void)
{
	return 30.0 / CTR_FRAMES_PER_SECOND;
}

double NativePhysics_ElapsedMS(double elapsedMS)
{
	// The shared integer clock distributes milliseconds over successive frames.
	// Continuous solvers keep the exact duration of an ordinary high-rate step;
	// unusual elapsed times (pause, slow motion or replay) retain their input.
	if (CTR_FRAMES_PER_SECOND > 60 && elapsedMS == CTR_FRAME_STEP(32, P32_GET(struct GameTracker *, sdata->gGT)->timer))
		return 32.0 * NativePhysics_FrameScale();
	return elapsedMS;
}

void NativePhysics_Reset(void)
{
	memset(&s_physics, 0, sizeof(s_physics));
	s_physics.enabled = NativePhysics_ModeMask();
}

void NativePhysics_SetDomain(enum NativePhysicsDomain domain, int enabled)
{
	int *modes[] = {&gNativeSmoothedPhysicsEnabled, &gNativeSmoothedAIEnabled, &gNativeSmoothedCollisionEnabled, &gNativeSmoothedSteeringEnabled};
	if ((unsigned)domain >= sizeof(modes) / sizeof(modes[0]))
		return;
	*modes[domain] = enabled != 0;
	NativePhysics_Reset();
}

void NativePhysics_SetEnabled(int enabled)
{
	NativePhysics_SetDomain(NATIVE_PHYSICS_PLAYER, enabled);
}

void NativePhysics_ResetDriver(struct Driver *d)
{
	memset(&s_physics.drivers[d->driverID], 0, sizeof(s_physics.drivers[0]));
}

void NativePhysics_UpdateTurboPadContact(struct Driver *d, u32 stepFlags)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	if (stepFlags & COLL_STEP_TRIGGER_TURBO_PAD_MASK)
		state->turboPadAbsentMS = 0;
	else
	{
		state->turboPadAbsentMS += NativePhysics_ElapsedMS(P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS);
		// Require a full retail step away from the pad to rearm. Short contact
		// gaps at high FPS are still part of the same crossing.
		if (state->turboPadAbsentMS >= 32.0 - 1e-9)
			state->turboPadBoosted = 0;
	}
}

int NativePhysics_ConsumeTurboPadEntry(struct Driver *d)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	int first = !state->turboPadBoosted;
	state->turboPadBoosted = 1;
	return first;
}

static struct NativePhysicsDriverState *NativePhysics_Driver(struct Driver *d)
{
	if (s_physics.enabled != NativePhysics_ModeMask())
		NativePhysics_Reset();
	struct NativePhysicsDriverState *state = &s_physics.drivers[d->driverID];
	if (!state->valid)
	{
		state->velocity = (NativePhysicsVec){d->velocity.x, d->velocity.y, d->velocity.z};
		state->position = (NativePhysicsVec){d->posCurr.x, d->posCurr.y, d->posCurr.z};
		state->exportedVelocity = d->velocity;
		state->exportedPosition = d->posCurr;
		state->speed = d->speed;
		state->yaw = d->axisRotationX;
		state->pitch = d->axisRotationY;
		state->exportedSpeed = d->speed;
		state->exportedYaw = d->axisRotationX;
		state->exportedPitch = d->axisRotationY;
		state->valid = 1;
	}
	return state;
}

// An integer collision/weapon write overrides its axis, including a hard stop.
// Values we exported ourselves retain their fractions across frames.
NativePhysicsVec NativePhysics_ReadVelocity(struct Driver *d)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	if (d->velocity.x != state->exportedVelocity.x)
		state->velocity.x = d->velocity.x;
	if (d->velocity.y != state->exportedVelocity.y)
		state->velocity.y = d->velocity.y;
	if (d->velocity.z != state->exportedVelocity.z)
		state->velocity.z = d->velocity.z;
	state->exportedVelocity = d->velocity;
	return state->velocity;
}

static s32 NativePhysics_Export(double value)
{
	if (!isfinite(value))
		return 0;
	return (s32)fmax(-2147483648.0, fmin(2147483647.0, round(value)));
}

void NativePhysics_WriteVelocity(struct Driver *d, NativePhysicsVec value)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	state->velocity = value;
	d->velocity = (Vec3){.x = NativePhysics_Export(value.x), .y = NativePhysics_Export(value.y), .z = NativePhysics_Export(value.z)};
	state->exportedVelocity = d->velocity;
}

NativePhysicsVec NativePhysics_Rotate(const MATRIX *matrix, NativePhysicsVec value, int transpose)
{
	const MATRIX *m = matrix;
	double v[3] = {value.x, value.y, value.z};
	double result[3] = {0, 0, 0};
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			result[i] += v[j] * (transpose ? m->m[j][i] : m->m[i][j]) / 4096.0;
	return (NativePhysicsVec){result[0], result[1], result[2]};
}

static void NativePhysics_SteeringMatrix(struct Driver *d, double m[3][3])
{
	const double radians = 6.2831853071795864769 / 4096.0;
	double nx = d->AxisAngle1_normalVec.x / 4096.0, ny = d->AxisAngle1_normalVec.y / 4096.0, nz = d->AxisAngle1_normalVec.z / 4096.0;
	double length = sqrt(nx * nx + ny * ny + nz * nz);
	if (length < 1e-12)
	{
		nx = 0;
		ny = 1;
		nz = 0;
	}
	else
	{
		nx /= length;
		ny /= length;
		nz /= length;
	}
	double angle = NATIVE_PHYSICS_READ(d, angle) * radians, sine = sin(angle), cosine = cos(angle), denominator = nx * nx + nz * nz;
	double x = sine * ny, z = cosine * ny;
	if (denominator > 1e-12)
	{
		x += ((sine - sine * ny) * nz * nz - (cosine - cosine * ny) * nx * nz) / denominator;
		z += ((cosine - cosine * ny) * nx * nx - (sine - sine * ny) * nx * nz) / denominator;
	}
	else if (ny < 0)
		x = -x;
	double y = -(sine * nx + cosine * nz);
	m[0][1] = nx;
	m[1][1] = ny;
	m[2][1] = nz;
	m[0][2] = x;
	m[1][2] = y;
	m[2][2] = z;
	m[0][0] = ny * z - nz * y;
	m[1][0] = nz * x - nx * z;
	m[2][0] = nx * y - ny * x;
}
void NativePhysics_UpdateSteeringMatrix(struct Driver *d)
{
	double m[3][3];
	NativePhysics_SteeringMatrix(d, m);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			d->matrixMovingDir.m[i][j] = (s16)round(m[i][j] * 4096);
}
NativePhysicsVec NativePhysics_RotateDriver(struct Driver *d, NativePhysicsVec value, int transpose)
{
	if (!CTR_NATIVE_SMOOTHED_STEERING_ACTIVE)
		return NativePhysics_Rotate(&d->matrixMovingDir, value, transpose);
	double m[3][3], v[3] = {value.x, value.y, value.z}, out[3] = {0, 0, 0};
	NativePhysics_SteeringMatrix(d, m);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			out[i] += v[j] * (transpose ? m[j][i] : m[i][j]);
	return (NativePhysicsVec){out[0], out[1], out[2]};
}
void NativePhysics_CounterSteer(struct Driver *d)
{
	NativePhysicsVec acceleration = {0, 0, 0};
	if (abs(d->speedApprox) > 768 && !(d->actionsFlagSet & ACTION_WARP) && d->kartState != KS_CRASHING && !d->wallRubTimer &&
	    (d->actionsFlagSet & ACTION_TOUCH_GROUND) && P32_GET(struct Terrain *, d->terrainMeta1)->counterSteerRatio)
	{
		double delta = NATIVE_PHYSICS_READ(d, turnAngleCurr) - d->turnAnglePrev;
		double limit = (u8)d->const_ModelTurnCounterSteerStrength;
		delta = fmax(-limit, fmin(limit, delta));
		double impulse = P32_GET(struct Terrain *, d->terrainMeta1)->counterSteerRatio * -8000.0 / 256.0 * sin(delta * (6.2831853071795864769 / 4096.0));
		acceleration = NativePhysics_RotateDriver(d, (NativePhysicsVec){impulse, 0, 0}, 0);
	}
	NATIVE_PHYSICS_WRITE(d, accel.x, acceleration.x);
	NATIVE_PHYSICS_WRITE(d, accel.y, acceleration.y);
	NATIVE_PHYSICS_WRITE(d, accel.z, acceleration.z);
}

double NativePhysics_GetSpeed(struct Driver *d)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	if (state->exportedSpeed != d->speed)
		state->speed = d->speed;
	state->exportedSpeed = d->speed;
	return state->speed;
}

void NativePhysics_SetSpeed(struct Driver *d, double value)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	state->speed = fmax(0, fmin(32767, value));
	d->speed = (s16)NativePhysics_Export(state->speed);
	state->exportedSpeed = d->speed;
}

void NativePhysics_ConvertVecToSpeed(struct Driver *d, NativePhysicsVec value)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	const double angleScale = 4096.0 / 6.2831853071795864769;
	NativePhysics_SetSpeed(d, sqrt(value.x * value.x + value.y * value.y + value.z * value.z));
	state->yaw = atan2(value.x, value.z) * angleScale;
	state->pitch = atan2(value.y, hypot(value.x, value.z)) * angleScale;
	d->axisRotationX = (s16)NativePhysics_Export(state->yaw);
	d->axisRotationY = (s16)NativePhysics_Export(state->pitch);
	state->exportedYaw = d->axisRotationX;
	state->exportedPitch = d->axisRotationY;
	if (CTR_NATIVE_SMOOTHED_STEERING_ACTIVE)
		NATIVE_PHYSICS_WRITE(d, axisRotationX, state->yaw);
	NativePhysicsVec local = NativePhysics_Rotate(&d->matrixMovingDir, value, 1);
	d->jumpHeightCurr = (s16)NativePhysics_Export(local.y);
	NativePhysicsVec vertical = NativePhysics_Rotate(&d->matrixMovingDir, (NativePhysicsVec){0, local.y, 0}, 0);
	NativePhysicsVec tangent = {value.x - vertical.x, value.y - vertical.y, value.z - vertical.z};
	double speed = sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
	double forward = tangent.x * d->matrixMovingDir.m[0][2] + tangent.y * d->matrixMovingDir.m[1][2] + tangent.z * d->matrixMovingDir.m[2][2];
	d->speedApprox = (s16)NativePhysics_Export(fmin(32767, speed) * (forward < 0 ? -1 : 1));
	NATIVE_PHYSICS_WRITE(d, speedApprox, fmin(32767, speed) * (forward < 0 ? -1 : 1));
}

void NativePhysics_ConvertSpeedToVec(struct Driver *d, Vec3 *output)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	const double radians = 6.2831853071795864769 / 4096.0;
	if (CTR_NATIVE_SMOOTHED_STEERING_ACTIVE)
		state->yaw = NATIVE_PHYSICS_READ(d, axisRotationX);
	else if (state->exportedYaw != d->axisRotationX)
		state->yaw = d->axisRotationX;
	state->exportedYaw = d->axisRotationX;
	if (state->exportedPitch != d->axisRotationY)
		state->pitch = d->axisRotationY;
	state->exportedPitch = d->axisRotationY;
	double speed = NativePhysics_GetSpeed(d);
	double horizontal = speed * cos(state->pitch * radians);
	NativePhysicsVec value = {horizontal * sin(state->yaw * radians), speed * sin(state->pitch * radians), horizontal * cos(state->yaw * radians)};
	// ConvertSpeedToVecOut can also be used to query an impulse. Only publish
	// continuous state when the destination is the driver's velocity itself.
	Vec3 *out = output;
	*out = (Vec3){.x = NativePhysics_Export(value.x), .y = NativePhysics_Export(value.y), .z = NativePhysics_Export(value.z)};
	if (out == &d->velocity)
		NativePhysics_WriteVelocity(d, value);
}

NativePhysicsVec NativePhysics_Step(struct Driver *d, double elapsedMS, double multiplier)
{
	NativePhysicsVec v = NativePhysics_ReadVelocity(d);
	double scale = NativePhysics_ElapsedMS(elapsedMS) / 32.0 * multiplier / 4096.0;
	return (NativePhysicsVec){v.x * scale, v.y * scale, v.z * scale};
}

void NativePhysics_WritePosition(struct Driver *d, NativePhysicsVec position)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	state->position = position;
	d->posCurr = (Vec3){.x = NativePhysics_Export(position.x), .y = NativePhysics_Export(position.y), .z = NativePhysics_Export(position.z)};
	state->exportedPosition = d->posCurr;
}

void NativePhysics_Move(struct Driver *d, NativePhysicsVec step, double fraction)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	if (d->posCurr.x != state->exportedPosition.x)
		state->position.x = d->posCurr.x;
	if (d->posCurr.y != state->exportedPosition.y)
		state->position.y = d->posCurr.y;
	if (d->posCurr.z != state->exportedPosition.z)
		state->position.z = d->posCurr.z;
	state->position.x += step.x * fraction;
	state->position.y += step.y * fraction;
	state->position.z += step.z * fraction;
	d->posCurr =
	    (Vec3){.x = NativePhysics_Export(state->position.x), .y = NativePhysics_Export(state->position.y), .z = NativePhysics_Export(state->position.z)};
	state->exportedPosition = d->posCurr;
}

double NativePhysics_WrapAngle(double value)
{
	value = fmod(value, 4096.0);
	return value < 0 ? value + 4096.0 : value;
}

static s32 NativePhysics_ReadInteger(const void *ptr, size_t size)
{
	s32 value = 0;
	if (size == 2)
	{
		s16 v;
		memcpy(&v, ptr, 2);
		value = v;
	}
	else if (size == 4)
		memcpy(&value, ptr, 4);
	return value;
}

static struct NativePhysicsScalar *NativePhysics_Scalar(struct Driver *d, size_t offset, size_t size)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	for (unsigned i = 0; i < sizeof(state->scalars) / sizeof(state->scalars[0]); i++)
	{
		struct NativePhysicsScalar *scalar = &state->scalars[i];
		if (scalar->size == 0)
		{
			scalar->offset = (u32)offset;
			scalar->size = (u32)size;
			scalar->value = scalar->exported = NativePhysics_ReadInteger((u8 *)d + offset, size);
		}
		if (scalar->offset == offset && scalar->size == size)
		{
			s32 value = NativePhysics_ReadInteger((u8 *)d + offset, size);
			if (value != scalar->exported)
				scalar->value = value;
			scalar->exported = value;
			return scalar;
		}
	}
	return NULL;
}

double NativePhysics_ReadScalar(struct Driver *d, size_t offset, size_t size)
{
	struct NativePhysicsScalar *scalar = NativePhysics_Scalar(d, offset, size);
	return scalar ? scalar->value : NativePhysics_ReadInteger((u8 *)d + offset, size);
}

void NativePhysics_WriteScalar(struct Driver *d, size_t offset, size_t size, double value)
{
	if (offset == offsetof(struct Driver, angle) || offset == offsetof(struct Driver, axisRotationX))
		value = NativePhysics_WrapAngle(value);
	struct NativePhysicsScalar *scalar = NativePhysics_Scalar(d, offset, size);
	s32 output = NativePhysics_Export(value);
	if (size == 2)
	{
		s16 shortValue = (s16)output;
		memcpy((u8 *)d + offset, &shortValue, 2);
		output = shortValue;
	}
	else if (size == 4)
		memcpy((u8 *)d + offset, &output, 4);
	if (scalar)
	{
		scalar->value = value;
		scalar->exported = output;
	}
	if (offset == offsetof(struct Driver, axisRotationX))
	{
		struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
		state->yaw = value;
		state->exportedYaw = d->axisRotationX;
	}
}

NativePhysicsVec NativePhysics_ReadPosition(struct Driver *d)
{
	struct NativePhysicsDriverState *state = NativePhysics_Driver(d);
	if (d->posCurr.x != state->exportedPosition.x)
		state->position.x = d->posCurr.x;
	if (d->posCurr.y != state->exportedPosition.y)
		state->position.y = d->posCurr.y;
	if (d->posCurr.z != state->exportedPosition.z)
		state->position.z = d->posCurr.z;
	state->exportedPosition = d->posCurr;
	return state->position;
}

int NativePhysics_GetStateSize(void)
{
	return sizeof(s_physics);
}
int NativePhysics_CaptureState(void *dst, int size)
{
	if (dst == NULL || size != sizeof(s_physics))
		return 0;
	s_physics.enabled = NativePhysics_ModeMask();
	memcpy(dst, &s_physics, sizeof(s_physics));
	return 1;
}
int NativePhysics_RestoreState(const void *src, int size)
{
	if (src == NULL || size != sizeof(s_physics))
		return 0;
	memcpy(&s_physics, src, sizeof(s_physics));
	gNativeSmoothedPhysicsEnabled = (s_physics.enabled & 1) != 0;
	gNativeSmoothedAIEnabled = (s_physics.enabled & 2) != 0;
	gNativeSmoothedCollisionEnabled = (s_physics.enabled & 4) != 0;
	gNativeSmoothedSteeringEnabled = (s_physics.enabled & 8) != 0;
	return 1;
}
