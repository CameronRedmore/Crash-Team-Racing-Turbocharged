#include <common.h>
#include <vehicle_physics_constants.h>
#include <math.h>

#if defined(CTR_NATIVE) && !defined(__vita__)
static double NativeSteering_Map(double value, double low, double high, double from, double to)
{
	if (value <= low)
		return from;
	if (value >= high)
		return to;
	return from + (value - low) * (to - from) / (high - low);
}
static double NativeSteering_Approach(double value, double step, double target)
{
	return value < target ? fmin(value + step, target) : fmax(value - step, target);
}
static double NativeSteering_Accel(double frame, double stage2, double stage2Length, double stage4, double minimum, double maximum)
{
	if (frame < stage2)
		return NativeSteering_Map(frame, 0, stage2, minimum, maximum);
	if (frame > stage2 + stage2Length)
		return NativeSteering_Map(frame, stage2 + stage2Length, stage4, maximum, 0);
	return maximum;
}
static double NativeSteering_QuarterStrength(double current, double desired);
static double NativeSteering_LerpForwards(struct Driver *d, double currentAngle, double currentVelocity, double targetAngle);

void NativePhysics_Steer(struct Driver *driver)
{
	double speedApprox;
	double elapsedTimeMS;
	double classSpeed_original;
	double driverSpeed;
	double classSpeed_halved;
	struct Terrain *terrain;
	double rotCurrW_original;
	double rotCurrWAbs;
	double angle;
	double turnResistMinBitshift;
	double turnResistMaxBitshift;
	double driftAngleCurr_Final;
	double turnResistMax;
	double turnResistMin;
	u32 actionsFlagSet;
	b32 interpLessThanOriginal;
	b32 wInterpLessThanZero;
	double forwardDir;
	double rotCurrW_interp;
	s8 simpTurnState;
	double driftAngleCurr_og;

	NativePhysics_LerpRotation(driver, 0);

	elapsedTimeMS = NativePhysics_ElapsedMS(P32_GET(struct GameTracker *, sdata->gGT)->elapsedTimeMS);
	actionsFlagSet = driver->actionsFlagSet;
	forwardDir = driver->forwardDir;
	simpTurnState = driver->simpTurnState;
	speedApprox = NATIVE_PHYSICS_READ(driver, speedApprox);
	rotCurrW_interp = ldexp(simpTurnState, 8);
	if (speedApprox < 1)
	{
		if (driver->baseSpeed < 0)
		{
			forwardDir = -1;
			driver->forwardDir = -1;
		}
		if (-1 < speedApprox)
		{
			goto LAB_8005fd74;
		}
	}
	else
	{
	LAB_8005fd74:
		if (-1 < driver->baseSpeed)
		{
			forwardDir = 1;
			driver->forwardDir = 1;
		}
	}
	if (forwardDir < 0)
	{
		rotCurrW_interp = (-ldexp(simpTurnState, 8));
		actionsFlagSet ^= ACTION_STEER_LEFT;
	}
	if (speedApprox < 0)
	{
		speedApprox = (-speedApprox);
	}
	if (((actionsFlagSet & ACTION_TOUCH_GROUND) != 0) && ((driver->stepFlagSet & COLL_STEP_TRIGGER_TURBO_PAD_MASK) == 0))
	{
		rotCurrW_interp = NativeSteering_Map(speedApprox, VEH_PHYS_ANGULAR_STICK_MIN_SPEED, VEH_PHYS_ANGULAR_STEER_SPEED_THRESHOLD, 0, rotCurrW_interp);
	}
	terrain = P32_GET(struct Terrain *, driver->terrainMeta1);
	rotCurrW_original = NATIVE_PHYSICS_READ(driver, rotationSpinRate);
	if (rotCurrW_interp == 0)
	{
		double rate =
		    ldexp(((driver->const_TurnInputDelay + ((s8)driver->turnConst * VEH_PHYS_ANGULAR_TURN_RESPONSE_COAST_SCALE)) * terrain->turnResponseScale), -(8));

		rotCurrW_interp = NativeSteering_Approach(rotCurrW_original, (rate * NativePhysics_FrameScale()), 0);

		forwardDir = rotCurrW_interp;
	}
	else
	{
		wInterpLessThanZero = rotCurrW_interp < 0;
		if (wInterpLessThanZero)
		{
			rotCurrW_interp = (-rotCurrW_interp);
			rotCurrW_original = (-rotCurrW_original);
		}
		if (rotCurrW_original < rotCurrW_interp)
		{
			double rate = ldexp(
			    ((driver->const_TurnInputDelay + ((s8)driver->turnConst * VEH_PHYS_ANGULAR_TURN_RESPONSE_ACCEL_SCALE)) * terrain->turnResponseScale), -(8));
			rotCurrW_original = (rotCurrW_original + rate * NativePhysics_FrameScale());

			interpLessThanOriginal = rotCurrW_interp < rotCurrW_original;
		LAB_8005fee4:
			if (interpLessThanOriginal)
			{
				rotCurrW_original = rotCurrW_interp;
			}
		}
		else if (rotCurrW_interp < rotCurrW_original)
		{
			double rate = ldexp(
			    ((driver->const_TurnInputDelay + ((s8)driver->turnConst * VEH_PHYS_ANGULAR_TURN_RESPONSE_DECEL_SCALE)) * terrain->turnResponseScale), -(8));
			rotCurrW_original = (rotCurrW_original - rate * NativePhysics_FrameScale());

			interpLessThanOriginal = rotCurrW_original < rotCurrW_interp;
			goto LAB_8005fee4;
		}
		forwardDir = rotCurrW_original;
		if (wInterpLessThanZero)
		{
			forwardDir = (-forwardDir);
		}
	}

	rotCurrW_original = forwardDir;
	NATIVE_PHYSICS_WRITE(driver, rotationSpinRate, forwardDir);

	rotCurrW_interp = driver->timeUntilDriftSpinout;
	if (rotCurrW_interp != 0)
	{
		classSpeed_halved = (rotCurrW_interp - elapsedTimeMS);
		rotCurrW_interp = NativeSteering_Map(rotCurrW_interp, 0, VEH_PHYS_ANGULAR_DRIFT_SPINOUT_TIME, 0, driver->previousFrameMultDrift);
		rotCurrW_original = (rotCurrW_original + rotCurrW_interp);
		if (classSpeed_halved < 0)
		{
			classSpeed_halved = 0;
		}
		driver->timeUntilDriftSpinout = classSpeed_halved;
	}

	classSpeed_halved = ldexp(driver->const_Speed_ClassStat, VEH_PHYS_ANGULAR_CLASS_SPEED_SHIFT);
	classSpeed_original = ldexp(classSpeed_halved, -(VEH_PHYS_ANGULAR_CLASS_SPEED_SHIFT));
	turnResistMax = ((u8)driver->const_turnResistMax * classSpeed_original);
	turnResistMin = ((u8)driver->const_turnResistMin * classSpeed_original);
	forwardDir = NATIVE_PHYSICS_READ(driver, turnAngleLerpVel);
	rotCurrW_interp = driver->const_modelRotVelMax;
	turnResistMaxBitshift = ldexp(turnResistMax, -(8));
	turnResistMinBitshift = ldexp(turnResistMin, -(8));

	// gas and brake together
	if ((actionsFlagSet & ACTION_BRAKE_WITH_ACCEL) != 0)
	{
		turnResistMaxBitshift = ldexp(turnResistMax, -(9));
		if (VEH_PHYS_ANGULAR_STEER_SPEED_THRESHOLD < speedApprox)
		{
			// driver is leaving skids
			driver->actionsFlagSet |= ACTION_BACK_SKID;
		}
		turnResistMinBitshift = ldexp(turnResistMin, -(9));
		if (driver->baseSpeed == 0)
		{
			rotCurrW_interp = driver->const_modelRotVelMin;
		}
		else
		{
			turnResistMax = NativePhysics_GetSpeed(driver);
			if (turnResistMax < 0)
			{
				turnResistMax = (-turnResistMax);
			}
			// Rotate the model to exaggerate steering above the steering speed threshold.
			rotCurrW_interp =
			    NativeSteering_Map(turnResistMax, VEH_PHYS_ANGULAR_STEER_SPEED_THRESHOLD, ldexp(classSpeed_halved, -(VEH_PHYS_ANGULAR_CLASS_SPEED_HALF_SHIFT)),
				                   driver->const_modelRotVelMin, rotCurrW_interp);
		}
	}
	driverSpeed = NativePhysics_GetSpeed(driver);
	if (driverSpeed < 0)
	{
		driverSpeed = (-driverSpeed);
	}

	// this prevents you from steering sharp at low speeds
	turnResistMin = ldexp(((u8)driver->const_TurnRate + ldexp((s8)driver->turnConst, 1) / 5), 8);
	turnResistMax = NativeSteering_Map(driverSpeed, turnResistMinBitshift, turnResistMaxBitshift, turnResistMin, 0);

	classSpeed_halved = 0;
	if (turnResistMinBitshift <= speedApprox)
	{
		rotCurrWAbs = rotCurrW_original;
		if (rotCurrW_original < 0)
		{
			rotCurrWAbs = (-rotCurrW_original);
		}
		if (turnResistMax < rotCurrWAbs)
		{
			classSpeed_halved = driver->fireSpeed;
			if (classSpeed_halved < 0)
			{
				classSpeed_halved = (-classSpeed_halved);
			}
			classSpeed_halved = NativeSteering_Map(classSpeed_halved, turnResistMinBitshift, turnResistMaxBitshift, 0, rotCurrW_interp);
			classSpeed_halved = NativeSteering_Map(rotCurrWAbs, turnResistMax, turnResistMin, 0, classSpeed_halved);
			if (rotCurrW_original < 0)
			{
				classSpeed_halved = (-classSpeed_halved);
			}
		}
	}

	driftAngleCurr_og = NATIVE_PHYSICS_READ(driver, turnAngleCurr);

	// spins camera from side of driver, to back of driver,
	// when the drifting ends. "LerpToForwards"
#if CTR_NATIVE_60FPS
	if (CTR_RETAIL_FRAME_TICK(P32_GET(struct GameTracker *, sdata->gGT)->timer))
#endif
		NATIVE_PHYSICS_WRITE(driver, turnAngleLerpVel, NativeSteering_LerpForwards(driver, driftAngleCurr_og, forwardDir, classSpeed_halved));

	classSpeed_halved = NATIVE_PHYSICS_READ(driver, turnAngleLerpVel);

	if (terrain->turnAngleScale != VEH_PHYS_ANGULAR_TERRAIN_SCALE_NEUTRAL)
	{
		classSpeed_halved = ldexp((terrain->turnAngleScale * classSpeed_halved), -(8));
	}
	driftAngleCurr_Final = (driftAngleCurr_og + ldexp((classSpeed_halved * elapsedTimeMS), -(VEH_PHYS_ANGULAR_TURN_INTEGRATION_SHIFT)));
	NATIVE_PHYSICS_WRITE(driver, turnAngleCurr, driftAngleCurr_Final);
	turnResistMinBitshift = rotCurrW_original;
	if ((VEH_PHYS_ANGULAR_STEER_ACCEL_COMPARE_SPEED < speedApprox) && ((actionsFlagSet & ACTION_TOUCH_GROUND) != 0))
	{
		turnResistMaxBitshift = NativeSteering_Accel((driver->numFramesSpentSteering * NativePhysics_FrameScale()), driver->const_SteerAccel_Stage2_FirstFrame,
		                                             driver->const_SteerAccel_Stage2_FrameLength, driver->const_SteerAccel_Stage4_FirstFrame,
		                                             driver->const_SteerAccel_Stage1_MinSteer, driver->const_SteerAccel_Stage1_MaxSteer);
		if (rotCurrW_original < 0)
		{
			turnResistMinBitshift = (-rotCurrW_original);
		}

		turnResistMinBitshift = ldexp((driver->const_SteerAccelTurnVelScale * turnResistMinBitshift), -(8));

		driver->numFramesSpentSteering = (driver->numFramesSpentSteering + 1);

		// the higher the value of turnResistMaxBitshift the more steering is "locked up"
		// try setting mov r3, xxxx at 80060170 for proof
		if (turnResistMinBitshift < turnResistMaxBitshift)
		{
			turnResistMaxBitshift = turnResistMinBitshift;
		}

		// steering left or right
		if ((actionsFlagSet & ACTION_STEER_LEFT) != 0)
		{
			turnResistMaxBitshift = (-turnResistMaxBitshift);
		}

		turnResistMax = driver->const_SteerAccelTurnVelLimit;

		if ((rotCurrW_original < 1) || (turnResistMinBitshift = (-turnResistMax), turnResistMinBitshift <= (rotCurrW_original + turnResistMaxBitshift)))
		{
			if (rotCurrW_original < 0)
			{
				turnResistMinBitshift = (rotCurrW_original + turnResistMaxBitshift);
				if (turnResistMax < (rotCurrW_original + turnResistMaxBitshift))
				{
					turnResistMinBitshift = turnResistMax;
				}
			}
			else
			{
				turnResistMinBitshift = (rotCurrW_original + turnResistMaxBitshift);
			}
		}
	}
	turnResistMax = NATIVE_PHYSICS_READ(driver, turnWobbleAngle);
	turnResistMaxBitshift = driver->turnWobbleTimer;
	rotCurrW_original = NATIVE_PHYSICS_READ(driver, turnWobbleVelocity);
	if (((terrain->flags & TERRAIN_FLAG_SKIP_TURN_ASSIST) == 0) && ((actionsFlagSet & ACTION_TOUCH_GROUND) != 0))
	{
		turnResistMin = driftAngleCurr_Final;
		if (driftAngleCurr_Final < 0)
		{
			turnResistMin = (-driftAngleCurr_Final);
		}
		if (ldexp((ldexp(rotCurrW_interp, 1) + rotCurrW_interp), -(2)) < turnResistMin)
		{
			rotCurrW_interp = classSpeed_halved;
			if (classSpeed_halved < 0)
			{
				rotCurrW_interp = (-classSpeed_halved);
			}
			if (rotCurrW_interp < VEH_PHYS_ANGULAR_TURN_ASSIST_MIN_DELTA)
			{
				rotCurrW_interp = turnResistMax;
				if (turnResistMax < 0)
				{
					rotCurrW_interp = (-turnResistMax);
				}
				if (rotCurrW_interp < VEH_PHYS_ANGULAR_TURN_WOBBLE_MIN_DELTA)
				{
					turnResistMaxBitshift = VEH_PHYS_ANGULAR_TURN_WOBBLE_TIMER;
					rotCurrW_original = VEH_PHYS_ANGULAR_TURN_WOBBLE_VELOCITY;
					if (driftAngleCurr_Final < 0)
					{
						rotCurrW_original = -VEH_PHYS_ANGULAR_TURN_WOBBLE_VELOCITY;
					}
				}
			}
			goto LAB_80060284;
		}
	}
	turnResistMaxBitshift = 0;
LAB_80060284:
	rotCurrW_interp = turnResistMax;
	if (turnResistMax < 0)
	{
		rotCurrW_interp = (-turnResistMax);
	}
	if (VEH_PHYS_ANGULAR_TURN_WOBBLE_DISABLE_ANGLE < rotCurrW_interp)
	{
		turnResistMaxBitshift = 0;
	}
	if (turnResistMaxBitshift == 0)
	{
		rotCurrW_original = VEH_PHYS_ANGULAR_TURN_WOBBLE_MIN_DELTA;
		if (0 < turnResistMax)
		{
			rotCurrW_original = (-VEH_PHYS_ANGULAR_TURN_WOBBLE_MIN_DELTA);
		}
		rotCurrW_interp = rotCurrW_original;
		if (rotCurrW_original < 0)
		{
			rotCurrW_interp = (-rotCurrW_original);
		}
		rotCurrW_interp = NativeSteering_Approach(turnResistMax, (rotCurrW_interp * NativePhysics_FrameScale()), 0);
		forwardDir = rotCurrW_interp;
	}
	else
	{
		turnResistMaxBitshift = (turnResistMaxBitshift - 1);
		forwardDir = (NATIVE_PHYSICS_READ(driver, turnWobbleAngle) + rotCurrW_original);
	}
	angle = NATIVE_PHYSICS_READ(driver, angle);
	driver->turnWobbleTimer = turnResistMaxBitshift;
	NATIVE_PHYSICS_WRITE(driver, turnWobbleAngle, forwardDir);
	NATIVE_PHYSICS_WRITE(driver, turnWobbleVelocity, rotCurrW_original);
	rotCurrW_interp = NativeSteering_Map(speedApprox, 0, VEH_PHYS_ANGULAR_AIR_TURN_SPEED_MAX, classSpeed_halved, 0);
	rotCurrW_original = ldexp((rotCurrW_interp * elapsedTimeMS), -(VEH_PHYS_ANGULAR_TURN_INTEGRATION_SHIFT));
	rotCurrW_interp = rotCurrW_original;
	if (rotCurrW_original < 0)
	{
		rotCurrW_interp = (-rotCurrW_original);
	}
	if (1 < rotCurrW_interp)
	{
		angle = ((angle - rotCurrW_original));
	}
	NATIVE_PHYSICS_WRITE(driver, ampTurnState, turnResistMinBitshift);

	angle = ((angle + ldexp((turnResistMinBitshift * elapsedTimeMS), -(VEH_PHYS_ANGULAR_AXIS_INTEGRATION_SHIFT))));
	NATIVE_PHYSICS_WRITE(driver, angle, angle);

	NATIVE_PHYSICS_WRITE(driver, rotCurr.y, ((angle + driftAngleCurr_Final) + forwardDir));

	if (((actionsFlagSet & ACTION_ACCEL_PREVENTION) == 0) && (driver->accelTapCount < DRIVER_ACCEL_TAP_STEER_COUNT))
	{
		if (terrain->turnLeanScale != VEH_PHYS_ANGULAR_TERRAIN_SCALE_NEUTRAL)
		{
			turnResistMinBitshift = ldexp((turnResistMinBitshift * terrain->turnLeanScale), -(8));
		}
	}
	else
	{
		turnResistMinBitshift = ldexp((turnResistMinBitshift * VEH_PHYS_ANGULAR_BRAKE_LEAN_SCALE), -(8));
	}

	NATIVE_PHYSICS_WRITE(
	    driver, axisRotationX,
	    ((NATIVE_PHYSICS_READ(driver, axisRotationX) + ldexp((turnResistMinBitshift * elapsedTimeMS), -(VEH_PHYS_ANGULAR_AXIS_INTEGRATION_SHIFT)))));

	PhysTerrainSlope(driver);
}

static double NativeSteering_QuarterStrength(double current, double desired)
{
	if (desired != 0)
	{
		desired = ldexp(desired, -(2));

		if (desired == 0)
		{
			desired = 1;
		}
	}

	if (desired <= current)
	{
		current = desired;
	}

	return current;
}

static double NativeSteering_LerpForwards(struct Driver *d, double currentAngle, double currentVelocity, double targetAngle)
{
	b32 mirrored = false;
	double desiredVelocity = 0;

	NATIVE_PHYSICS_WRITE(d, turnAngleLerpTarget, 0);
	if ((targetAngle < 0) || ((targetAngle == 0 && (currentAngle < 0))))
	{
		mirrored = true;
		currentAngle = (-currentAngle);
		currentVelocity = (-currentVelocity);
		targetAngle = (-targetAngle);
	}

	if (d->wallRubTimer != DRIVER_WALL_RUB_TIMER_START)
	{
		if (targetAngle < currentAngle)
		{
			u32 lerpStrength;

			if (d->const_modelRotVelMax < currentAngle)
			{
				lerpStrength = (ldexp((u8)d->const_ModelTurnReturnStrength, 4) - (u8)d->const_ModelTurnReturnStrength);
			}
			else
			{
				lerpStrength = (u8)d->const_ModelTurnReturnStrength;
			}
			desiredVelocity = NativeSteering_QuarterStrength(lerpStrength, (currentAngle - targetAngle));
			desiredVelocity = (-desiredVelocity);
		}
		else
		{
			if (currentAngle < targetAngle)
			{
				if (currentAngle < 0)
				{
					desiredVelocity = NativeSteering_QuarterStrength((u8)d->const_ModelTurnNegativeReturnStrength, (targetAngle - currentAngle));
				}
				else
				{
					desiredVelocity = NativeSteering_QuarterStrength((u8)d->const_ModelTurnCounterSteerStrength, (targetAngle - currentAngle));
					NATIVE_PHYSICS_WRITE(d, turnAngleLerpTarget, targetAngle);
				}
			}
		}
	}

	// Interpolate rotation by speed
	desiredVelocity = NativeSteering_Approach(currentVelocity, d->const_ModelTurnVelocityLerp, desiredVelocity);
	if (mirrored)
	{
		desiredVelocity = (-desiredVelocity);
	}
	return desiredVelocity;
}

void NativePhysics_DriftSteer(struct Driver *driver)
{
	struct GameTracker *gGT = sdata->gGT;

	double axisAngleDelta = (NativePhysics_WrapAngle(((NATIVE_PHYSICS_READ(driver, axisRotationX) - NATIVE_PHYSICS_READ(driver, angle)) + ANG_PI)) - ANG_PI);
	if (axisAngleDelta != 0)
	{
		// decrease by 1/8
		// val = val * 7/8
		double axisAngleStep = ldexp(axisAngleDelta, -(VEH_PHYS_PROC_DRIFT_ANGLE_LERP_SHIFT));

		if (axisAngleStep == 0)
		{
			axisAngleStep = 1;
		}

		double axisAngleStepLimit =
		    ldexp(ldexp(NativePhysics_ElapsedMS(gGT->elapsedTimeMS), VEH_PHYS_PROC_DRIFT_AXIS_STEP_MS_SHIFT), -(VEH_PHYS_PROC_DRIFT_MS_SCALE_SHIFT));

		if (axisAngleStep > axisAngleStepLimit)
		{
			axisAngleStep = axisAngleStepLimit;
		}

		double minAxisAngleStep = (-axisAngleStepLimit);
		if (axisAngleStep < minAxisAngleStep)
		{
			axisAngleStep = minAxisAngleStep;
		}

		// change player rotation
		NATIVE_PHYSICS_WRITE(driver, angle, (NATIVE_PHYSICS_READ(driver, angle) + axisAngleStep));

		NATIVE_PHYSICS_WRITE(driver, axisRotationX, NativePhysics_WrapAngle((NATIVE_PHYSICS_READ(driver, axisRotationX) - axisAngleStep)));
	}

	// positive cam spin rate
	double cameraSpinRate = driver->const_Drifting_CameraSpinRate;

	if (driver->multDrift < 0)
	{
		// negative cam spin rate
		cameraSpinRate = (-cameraSpinRate);
	}

	NativePhysics_LerpRotation(driver, cameraSpinRate);

	// turning rate
	double currentSpinRate = NATIVE_PHYSICS_READ(driver, rotationSpinRate);

	// drift direction
	double driftDirection = driver->multDrift;

	b32 spinRateNegated = false;

	double steerInput = (s8)driver->simpTurnState;
	double steerInputScaled = ldexp(steerInput, FRACTIONAL_BITS_8);
	double steerVelLimit;

	if (driftDirection < 0)
	{
		// if steering to the right
		if (steerInputScaled < 1)
		{
			steerInputScaled = (-ldexp(steerInput, FRACTIONAL_BITS_8));

			// const_SteerVel_DriftStandard
			steerVelLimit = (-(s8)driver->const_SteerVel_DriftStandard);
		}

		// if steering to the left
		else
		{
			// const_SteerVel_DriftSwitchWay
			steerVelLimit = (-(s8)driver->const_SteerVel_DriftSwitchWay);
		}
	}

	// if drifting to the left
	else
	{
		// if steering to the right
		if (steerInputScaled < 0)
		{
			steerInputScaled = (-ldexp(steerInput, FRACTIONAL_BITS_8));

			// const_SteerVel_DriftSwitchWay
			steerVelLimit = (s8)driver->const_SteerVel_DriftSwitchWay;
		}

		// if steering to the left
		else
		{
			// const_SteerVel_DriftStandard
			steerVelLimit = (s8)driver->const_SteerVel_DriftStandard;
		}
	}

	// Map "simpTurnState" from [0, const_TurnRate] to [0, driftDirection]
	double desiredSpinRate = NativeSteering_Map(
	    steerInputScaled, 0,
	    ldexp((driver->const_TurnRate + ldexp((s8)driver->turnConst, VEH_PHYS_PROC_STEER_TURN_CONST_SHIFT) / VEH_PHYS_PROC_STEER_TURN_CONST_DIVISOR),
		      FRACTIONAL_BITS_8),
	    0, ldexp(steerVelLimit, FRACTIONAL_BITS_8));

	b32 clampSpinRate;
	if (desiredSpinRate < 0)
	{
		spinRateNegated = true;
		desiredSpinRate = (-desiredSpinRate);
		currentSpinRate = (-currentSpinRate);
		driftDirection = (-driftDirection);
		clampSpinRate = desiredSpinRate < currentSpinRate;
	}
	else
	{
		clampSpinRate = desiredSpinRate < currentSpinRate;
		if ((desiredSpinRate == 0) && (currentSpinRate < 0))
		{
			spinRateNegated = true;
			currentSpinRate = (-currentSpinRate);
			driftDirection = (-driftDirection);
			clampSpinRate = desiredSpinRate < currentSpinRate;
		}
	}

	// 0x464 and 0x466 impact turning somehow

	if (clampSpinRate)
	{
		currentSpinRate =
		    (currentSpinRate - ldexp((driver->const_DriftSpinRateDecel * NativePhysics_ElapsedMS(gGT->elapsedTimeMS)), -(VEH_PHYS_PROC_DRIFT_MS_SCALE_SHIFT)));
		clampSpinRate = currentSpinRate < desiredSpinRate;
	}
	else
	{
		currentSpinRate =
		    (currentSpinRate + ldexp((driver->const_DriftSpinRateAccel * NativePhysics_ElapsedMS(gGT->elapsedTimeMS)), -(VEH_PHYS_PROC_DRIFT_MS_SCALE_SHIFT)));
		clampSpinRate = desiredSpinRate < currentSpinRate;
	}

	if (clampSpinRate)
	{
		currentSpinRate = desiredSpinRate;
	}

	// if not holding a drift direction,
	// interpolate to "neutral" drift
	if ((desiredSpinRate == 0) || (driftDirection == 0))
	{
#if CTR_NATIVE_60FPS
		if (CTR_RETAIL_FRAME_TICK(gGT->timer))
#endif
		{
			driver->KartStates.Drifting.numFramesDrifting = NativeSteering_Approach(driver->KartStates.Drifting.numFramesDrifting, 1, 0);
		}
	}

	// if holding a drift
	else
	{
		// if drifting right
		if (driftDirection < 1)
		{
#if CTR_NATIVE_60FPS
			if (CTR_RETAIL_FRAME_TICK(gGT->timer))
#endif
				driver->KartStates.Drifting.numFramesDrifting = (driver->KartStates.Drifting.numFramesDrifting - 1);

			if (driver->KartStates.Drifting.numFramesDrifting > 0)
			{
				driver->KartStates.Drifting.numFramesDrifting = 0;
			}
		}

		// if drifting left
		else
		{
#if CTR_NATIVE_60FPS
			if (CTR_RETAIL_FRAME_TICK(gGT->timer))
#endif
				driver->KartStates.Drifting.numFramesDrifting = (driver->KartStates.Drifting.numFramesDrifting + 1);

			if (driver->KartStates.Drifting.numFramesDrifting < 0)
			{
				driver->KartStates.Drifting.numFramesDrifting = 0;
			}
		}
	}
	if (spinRateNegated)
	{
		currentSpinRate = (-currentSpinRate);
		driftDirection = (-driftDirection);
	}

	// Map value from [oldMin, oldMax] to [newMin, newMax]
	// inverting newMin and newMax will give an inverse range mapping
	double driftTurnInput =
	    NativeSteering_Map(driver->KartStates.Drifting.driftTotalTimeMS, 0, ldexp((u8)driver->const_DriftTurnRampFrames, VEH_PHYS_PROC_FRAME_TIME_SHIFT),
		                   ldexp(((s8)driver->const_DriftTurnStartupScale * driver->multDrift), -(FRACTIONAL_BITS_8)), driftDirection);

	double newSpinRate = currentSpinRate;
	if (-1 < driftTurnInput)
	{
		if (currentSpinRate < (-driftTurnInput))
		{
			currentSpinRate = (-driftTurnInput);
		}
		newSpinRate = currentSpinRate;
	}
	if (driftTurnInput <= 0)
	{
		if ((-driftTurnInput) < currentSpinRate)
		{
			newSpinRate = (-driftTurnInput);
		}
	}

	double driftTurnInputAbs = driftTurnInput;
	if (driftTurnInput < 0)
	{
		driftTurnInputAbs = (-driftTurnInput);
	}
	NATIVE_PHYSICS_WRITE(driver, rotationSpinRate, newSpinRate);
	double signedSpinRate = newSpinRate;

	// Map value from [oldMin, oldMax] to [newMin, newMax]
	// inverting newMin and newMax will give an inverse range mapping
	double driftTurnAngleBase = NativeSteering_Map(
	    driftTurnInputAbs, 0,
	    ((s8)driver->const_DriftTurnBase + ldexp((s8)driver->turnConst, VEH_PHYS_PROC_DRIFT_TURN_CONST_SHIFT) / VEH_PHYS_PROC_STEER_TURN_CONST_DIVISOR), 0,
	    driver->const_DriftTurnAngleScale);

	double spinRateAbs = signedSpinRate;
	if (signedSpinRate < 0)
	{
		spinRateAbs = (-signedSpinRate);
	}

	// drift input and current spin have different signs
	double driftTurnAngleLimit = driver->const_DriftTurnOppositeDirectionAngle;
	double driftSteerVelLimit = (s8)driver->const_SteerVel_DriftSwitchWay;

	// if both numbers have same sign,
	// either both < 0, or both >= 0
	if ((driftTurnInput < 0) == (signedSpinRate < 0))
	{
		driftTurnAngleLimit = driver->const_DriftTurnSameDirectionAngle;
		driftSteerVelLimit = (s8)driver->const_SteerVel_DriftStandard;
	}

	if (driftTurnInput < 0)
	{
		driftTurnAngleBase = (-driftTurnAngleBase);
		driftTurnAngleLimit = (-driftTurnAngleLimit);
	}

	// Map value from [oldMin, oldMax] to [newMin, newMax]
	// inverting newMin and newMax will give an inverse range mapping
	double driftTurnAngleAssist = NativeSteering_Map(spinRateAbs, 0, ldexp(driftSteerVelLimit, FRACTIONAL_BITS_8), 0, driftTurnAngleLimit);

	double turnAngleDelta = ((driftTurnAngleBase + driftTurnAngleAssist) - NATIVE_PHYSICS_READ(driver, turnAngleCurr));

	double turnAngleStep;
#if CTR_NATIVE_60FPS
	if (CTR_NATIVE_60FPS_ACTIVE)
	{
		if (CTR_FRAMES_PER_SECOND > 60)
			turnAngleStep = turnAngleDelta * (1.0 - pow(1.0 - ldexp(1.0, -VEH_PHYS_PROC_DRIFT_ANGLE_LERP_SHIFT), NativePhysics_FrameScale()));
		else if ((gGT->timer & 1) != 0)
			turnAngleStep = ldexp(turnAngleDelta, -(VEH_PHYS_PROC_DRIFT_ANGLE_LERP_SHIFT + 1));
		else
			turnAngleStep = ldexp((turnAngleDelta * 16) / 15, -(VEH_PHYS_PROC_DRIFT_ANGLE_LERP_SHIFT));
	}
	else
	{
		turnAngleStep = ldexp(turnAngleDelta, -(VEH_PHYS_PROC_DRIFT_ANGLE_LERP_SHIFT));
	}
#else
	turnAngleStep = ldexp(turnAngleDelta, -(VEH_PHYS_PROC_DRIFT_ANGLE_LERP_SHIFT));
#endif

	double turnAngleStepSigned = turnAngleStep;
	if (turnAngleDelta != 0)
	{
		if (turnAngleStep == 0)
		{
			turnAngleStepSigned = 1;
		}
		NATIVE_PHYSICS_WRITE(driver, turnAngleCurr, (NATIVE_PHYSICS_READ(driver, turnAngleCurr) + turnAngleStepSigned));
	}

	double numFramesDriftingAbs = driver->KartStates.Drifting.numFramesDrifting;

	if (numFramesDriftingAbs < 0)
	{
		numFramesDriftingAbs = (-numFramesDriftingAbs);
	}

	// get half of spin-out constant,
	// this determines when to start making tire sound effects,
	// after the turbo meter finishes filling past it's max capacity

	// if you drift beyond the limit of the turbo meter
	if (((u8)driver->const_Drifting_FramesTillSpinout >> VEH_PHYS_PROC_DRIFT_SPINOUT_THRESHOLD_SHIFT) < numFramesDriftingAbs)
	{
		// Play the SFX of near-spinout

		double turnWobbleAngleAbs = NATIVE_PHYSICS_READ(driver, turnWobbleAngle);
		if (turnWobbleAngleAbs < 0)
		{
			turnWobbleAngleAbs = (-turnWobbleAngleAbs);
		}

		// if low distortion
		if (turnWobbleAngleAbs < VEH_PHYS_PROC_TURN_WOBBLE_START_ANGLE_MAX)
		{
			// count up for 8 frames
			driver->turnWobbleTimer = FPS_DOUBLE(VEH_PHYS_PROC_TURN_WOBBLE_START_TIMER);

			// distortion, rate of change
			NATIVE_PHYSICS_WRITE(driver, turnWobbleVelocity, (VEH_PHYS_PROC_TURN_WOBBLE_START_VELOCITY * NativePhysics_FrameScale()));

			if (driftTurnInput < 0)
			{
				NATIVE_PHYSICS_WRITE(driver, turnWobbleVelocity, (-NATIVE_PHYSICS_READ(driver, turnWobbleVelocity)));
			}
		}
	}

	// if not near-spinout
	else
	{
		// stop increasing distortion,
		// go back down
		driver->turnWobbleTimer = 0;
	}

	double turnWobbleAngleAbs = NATIVE_PHYSICS_READ(driver, turnWobbleAngle);
	if (turnWobbleAngleAbs < 0)
	{
		turnWobbleAngleAbs = (-turnWobbleAngleAbs);
	}

	// if distortion is too high
	if (turnWobbleAngleAbs > VEH_PHYS_PROC_TURN_WOBBLE_ANGLE_MAX)
	{
		// stop increasing distortion,
		// go back down
		driver->turnWobbleTimer = 0;
	}

	double turnWobbleAngleNext;
	// frame countdown over
	if (driver->turnWobbleTimer == 0)
	{
		// nearing spinout sfx
		NATIVE_PHYSICS_WRITE(driver, turnWobbleVelocity, (VEH_PHYS_PROC_TURN_WOBBLE_RETURN_VELOCITY * NativePhysics_FrameScale()));

		if (0 < NATIVE_PHYSICS_READ(driver, turnWobbleAngle))
		{
			NATIVE_PHYSICS_WRITE(driver, turnWobbleVelocity, (-NATIVE_PHYSICS_READ(driver, turnWobbleVelocity)));
		}

		double turnWobbleVelocityAbs = NATIVE_PHYSICS_READ(driver, turnWobbleVelocity);
		if (turnWobbleVelocityAbs < 0)
		{
			turnWobbleVelocityAbs = (-turnWobbleVelocityAbs);
		}

		// move down until zero
		turnWobbleAngleNext = NativeSteering_Approach(NATIVE_PHYSICS_READ(driver, turnWobbleAngle), turnWobbleVelocityAbs, 0);
	}

	// frames counting down
	else
	{
		driver->turnWobbleTimer = (driver->turnWobbleTimer - 1);

		// move up each frame
		turnWobbleAngleNext = (NATIVE_PHYSICS_READ(driver, turnWobbleAngle) + NATIVE_PHYSICS_READ(driver, turnWobbleVelocity));
	}

	// near-spinout distortion SFX
	NATIVE_PHYSICS_WRITE(driver, turnWobbleAngle, turnWobbleAngleNext);

	NATIVE_PHYSICS_WRITE(driver, ampTurnState, (signedSpinRate + driftTurnInput));

	NATIVE_PHYSICS_WRITE(driver, angle,
	                     NativePhysics_WrapAngle((NATIVE_PHYSICS_READ(driver, angle) +
	                                              ldexp((NATIVE_PHYSICS_READ(driver, ampTurnState) * NativePhysics_ElapsedMS(gGT->elapsedTimeMS)),
	                                                    -(VEH_PHYS_PROC_ANGLE_INTEGRATION_SHIFT)))));

	if (driver->KartStates.Drifting.driftBoostTimeMS != 0)
	{
		// decrease by elpased time
		driver->KartStates.Drifting.driftBoostTimeMS = (driver->KartStates.Drifting.driftBoostTimeMS - gGT->elapsedTimeMS);

		if (driver->KartStates.Drifting.driftBoostTimeMS < 0)
		{
			driver->KartStates.Drifting.driftBoostTimeMS = 0;
		}

		double axisKick =
		    ldexp(((u8)driver->const_DriftBoostAxisKickRate * NativePhysics_ElapsedMS(gGT->elapsedTimeMS)), -(VEH_PHYS_PROC_DRIFT_MS_SCALE_SHIFT));

		if (NATIVE_PHYSICS_READ(driver, turnAngleCurr) < 0)
		{
			axisKick = (-axisKick);
		}

		NATIVE_PHYSICS_WRITE(driver, axisRotationX, NativePhysics_WrapAngle((NATIVE_PHYSICS_READ(driver, axisRotationX) + axisKick)));
	}

	NATIVE_PHYSICS_WRITE(driver, rotCurr.y,
	                     ((NATIVE_PHYSICS_READ(driver, turnWobbleAngle) + NATIVE_PHYSICS_READ(driver, angle)) + NATIVE_PHYSICS_READ(driver, turnAngleCurr)));

	// increment this by milliseconds
	driver->KartStates.Drifting.driftTotalTimeMS = (driver->KartStates.Drifting.driftTotalTimeMS + gGT->elapsedTimeMS);

	if (driver->KartStates.Drifting.driftTotalTimeMS > ldexp((u8)driver->const_DriftTurnRampFrames, VEH_PHYS_PROC_FRAME_TIME_SHIFT))
	{
		driver->KartStates.Drifting.driftTotalTimeMS = ldexp((u8)driver->const_DriftTurnRampFrames, VEH_PHYS_PROC_FRAME_TIME_SHIFT);
	}

	PhysTerrainSlope(driver);
}

void NativePhysics_LerpRotation(struct Driver *driver, double targetRotW)
{
	double remainingRot = (NATIVE_PHYSICS_READ(driver, rotCurr.w) - targetRotW);
	if (remainingRot < 0)
	{
		remainingRot = (-remainingRot);
	}

	double lerpStep;
#if CTR_NATIVE_60FPS
	if (CTR_NATIVE_60FPS_ACTIVE)
	{
		if (CTR_FRAMES_PER_SECOND > 60)
			lerpStep = remainingRot * (1.0 - pow(0.875, NativePhysics_FrameScale()));
		else if ((P32_GET(struct GameTracker *, sdata->gGT)->timer & 1) != 0)
			lerpStep = ldexp(remainingRot, -(4));
		else
			lerpStep = ldexp((remainingRot * 16) / 15, -(3));
	}
	else
	{
		lerpStep = ldexp(remainingRot, -(3));
	}
#else
	lerpStep = ldexp(remainingRot, -(3));
#endif

	if (lerpStep == 0)
	{
		lerpStep = 1;
	}

	double maxLerpStep = (u8)driver->const_DriftCameraLerpStep;
	if (lerpStep < (u8)driver->const_DriftCameraLerpStep)
	{
		maxLerpStep = lerpStep;
	}

	// Interpolate rotation by speed
	NATIVE_PHYSICS_WRITE(driver, rotPrev.w, NativeSteering_Approach(NATIVE_PHYSICS_READ(driver, rotPrev.w), 8 * NativePhysics_FrameScale(), maxLerpStep));

	// Interpolate rotation by speed
	NATIVE_PHYSICS_WRITE(driver, rotCurr.w,
	                     NativeSteering_Approach(NATIVE_PHYSICS_READ(driver, rotCurr.w),
	                                             ldexp((NATIVE_PHYSICS_READ(driver, rotPrev.w) * NativePhysics_ElapsedMS(sdata->gGT->elapsedTimeMS)), -(5)),
	                                             targetRotW));
}
#endif
