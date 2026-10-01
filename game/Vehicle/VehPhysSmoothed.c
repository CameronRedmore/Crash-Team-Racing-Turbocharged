#include <common.h>
#include <math.h>
#include <vehicle_physics_constants.h>

#if defined(CTR_NATIVE) && !defined(__vita__)
// Floating point counterpart of the original gravity/friction and jump rules.
// Gameplay flags, timers and feedback retain their original integer rules.
static double Smoothed_Neg(double a) { return -a; }
static double Smoothed_Down(double a, int bits) { return ldexp(a, -bits); }
static double Smoothed_Up(double a, int bits) { return ldexp(a, bits); }
static double Smoothed_Approach(double value, double step, double target)
{
	if (value < target) return fmin(value + step, target);
	return fmax(value - step, target);
}
static double Smoothed_JumpVelY(s16 *normal, NativePhysicsVec *velocity)
{
	if (abs(normal[1]) < VEH_PHYS_JUMP_NORMAL_Y_MIN) return 0;
	return (velocity->x * normal[0] + velocity->z * normal[2]) / normal[1];
}
void NativePhysics_SurfacePushback(struct Driver *d)
{
	if (!(d->collisionFlags & DRIVER_COLL_FLAG_SURFACE_PUSHBACK)) return;
	NativePhysicsVec position = NativePhysics_ReadPosition(d);
	double diffX = position.x / 256.0 - d->spsHitPos.x;
	double diffZ = position.z / 256.0 - d->spsHitPos.z;
	double floorDiffY = d->quadBlockHeight / 256.0 - d->spsHitPos.y + VEH_PHYS_FORCE_SURFACE_PUSHBACK_Y_BIAS;
	if (d->spsNormalVec.x * diffX + d->spsNormalVec.y * floorDiffY + d->spsNormalVec.z * diffZ >= 0) return;
	// Recovery acceleration retains fractional impulses and scales with the
	// simulation duration rather than applying it once per rendered frame.
	double scale = ldexp(NativePhysics_ElapsedMS(sdata->gGT->elapsedTimeMS) / 32.0, VEH_PHYS_FORCE_SURFACE_PUSHBACK_SHIFT);
	NativePhysicsVec velocity = NativePhysics_ReadVelocity(d);
	velocity.x += diffX * scale;
	velocity.y += (position.y / 256.0 - d->spsHitPos.y) * scale;
	velocity.z += diffZ * scale;
	NativePhysics_WriteVelocity(d, velocity);
}
void NativePhysics_Gravity(struct Driver *driver, Vec3 *output)
{
	double elapsedTimeMS = NativePhysics_ElapsedMS(sdata->gGT->elapsedTimeMS);

	NativePhysicsVec velocityValue = NativePhysics_ReadVelocity(driver);
	NativePhysicsVec *velocity = &velocityValue;

	NativePhysicsVec localVelocity = NativePhysics_RotateDriver(driver, *velocity, 1);
	double originalLocalZ = localVelocity.z;
	double gravityY = Smoothed_Neg(driver->const_Gravity);
	struct QuadBlock *underDriver = driver->underDriver;

	// NOTE(aalhendi): Retail does not branch before reading this flag. Native
	// can reach this frame after TeleportSelf clears underDriver and before
	// fixed collision repopulates it; PS1 low-RAM behavior is no low-gravity
	// flag, while host C would crash on a null dereference.
	if ((underDriver != NULL) && ((underDriver->quadFlags & VEH_PHYS_FORCE_QUAD_LOW_GRAVITY) != 0))
	{
		double scaledGravity = (Smoothed_Up(gravityY, 2) + gravityY);
		gravityY = (Smoothed_Up(scaledGravity, 3) + gravityY) / VEH_PHYS_FORCE_LOW_GRAVITY_DIVISOR;
	}

	gravityY = Smoothed_Down((gravityY * elapsedTimeMS), 5);
	NativePhysicsVec localGravity = NativePhysics_RotateDriver(driver, (NativePhysicsVec){0, gravityY, 0}, 1);

	if (((localGravity.z < 0) && (driver->forwardAccelImpulse > 0)) || ((localGravity.z > 0) && (driver->forwardAccelImpulse < 0)))
	{
		localGravity.z = 0;
	}

	u32 actionsFlagSet = driver->actionsFlagSet;
	double speedApprox = driver->speedApprox;
	double baseSpeed = driver->baseSpeed;
	if ((actionsFlagSet & ACTION_ACCEL_PREVENTION) || ((baseSpeed > 0) && (speedApprox < 0)) || ((driver->baseSpeed < 0) && (driver->speedApprox > 0)))
	{
		localGravity.x = 0;
		localGravity.z = 0;
	}

	double originalLocalX = localVelocity.x;
	double originalLocalY = localVelocity.y;
	double localX = (originalLocalX + localGravity.x);
	double localY = (originalLocalY + localGravity.y);
	double localZ = (originalLocalZ + localGravity.z);

	double maxForwardSpeed = (driver->fireSpeed + driver->const_SlopeForwardSpeedBonus);
	if (localZ > maxForwardSpeed)
	{
		localZ = originalLocalZ;
		if (originalLocalZ < maxForwardSpeed)
		{
			localZ = maxForwardSpeed;
		}
	}

	double minForwardSpeed = (driver->fireSpeed - Smoothed_Down(driver->const_SlopeForwardSpeedBonus, 1));
	if (localZ < minForwardSpeed)
	{
		localZ = originalLocalZ;
		if (originalLocalZ > minForwardSpeed)
		{
			localZ = minForwardSpeed;
		}
	}

	double maxPerpendicularSpeed = driver->const_SideSpeedClamp;
	if (localX > maxPerpendicularSpeed)
	{
		localX = originalLocalX;
		if (originalLocalX < maxPerpendicularSpeed)
		{
			localX = maxPerpendicularSpeed;
		}
	}

	double minPerpendicularSpeed = Smoothed_Neg(maxPerpendicularSpeed);
	if (localX < minPerpendicularSpeed)
	{
		localX = originalLocalX;
		if (originalLocalX > minPerpendicularSpeed)
		{
			localX = minPerpendicularSpeed;
		}
	}

	TerrainFlags terrainFlags = driver->terrainMeta1->flags;
	double terminalVelocity = driver->const_TerminalVelocity;
	if ((localY < 0) && ((terrainFlags & TERRAIN_FLAG_MUD_PHYSICS) != 0))
	{
		terminalVelocity = VEH_PHYS_FORCE_MUD_TERMINAL_SPEED;
		if (originalLocalY < -VEH_PHYS_FORCE_MUD_TERMINAL_SPEED)
		{
			originalLocalY = -VEH_PHYS_FORCE_MUD_TERMINAL_SPEED;
		}
	}

	double clampedLocalY = localY;
	if (localY > terminalVelocity)
	{
		clampedLocalY = originalLocalY;
		if (originalLocalY < terminalVelocity)
		{
			clampedLocalY = terminalVelocity;
		}
	}

	double minTerminalVelocity = Smoothed_Neg(terminalVelocity);
	if (clampedLocalY < minTerminalVelocity)
	{
		clampedLocalY = originalLocalY;
		if (originalLocalY > minTerminalVelocity)
		{
			clampedLocalY = minTerminalVelocity;
		}
	}

	localY = clampedLocalY;

	if (driver->kartState == KS_MASK_GRABBED)
	{
		localX = 0;
		localZ = 0;
	}
	else if (((driver->actionsFlagSetPrevFrame & ACTION_TOUCH_GROUND) != 0) || (driver->kartState == KS_BLASTED) ||
			 ((driver->terrainScaledBaseSpeed < driver->speedApprox) && (driver->terrainMeta2->speedMultiplier < VEH_PHYS_FORCE_TERRAIN_SCALE_NEUTRAL)))
	{
		double perpendicularFriction;
		double forwardFriction;

		if ((actionsFlagSet & ACTION_ACCEL_PREVENTION) == 0)
		{
			if (baseSpeed == 0)
			{
				perpendicularFriction = driver->const_NoPedalFriction_Perpendicular;
				forwardFriction = driver->const_NoPedalFriction_Forward;

				if (driver->rainCloudEffect == RAIN_CLOUD_EFFECT_HEAVY_FRICTION)
				{
					perpendicularFriction = Smoothed_Up(driver->const_BrakeFriction, 4);
					forwardFriction = perpendicularFriction;
				}
			}
			else
			{
				double absSpeedApprox = fabs(speedApprox);
				if ((absSpeedApprox < (VEH_PHYS_FORCE_SKID_SPEED_THRESHOLD + 1)) ||
					(((baseSpeed < 1) || (speedApprox >= 0)) && ((baseSpeed >= 0) || (speedApprox < 1))))
				{
					if (driver->kartState == KS_DRIFTING)
					{
						perpendicularFriction = driver->const_DriftCurve;
						forwardFriction = driver->const_DriftFriction;
					}
					else
					{
						perpendicularFriction = driver->const_PedalFriction_Perpendicular;
						forwardFriction = driver->const_PedalFriction_Forward;

						if (absSpeedApprox > VEH_PHYS_FORCE_SKID_SPEED_THRESHOLD)
						{
							double absBaseSpeed = fabs(baseSpeed);
							if (absSpeedApprox < Smoothed_Down(absBaseSpeed, 1))
							{
								actionsFlagSet |= ACTION_BACK_SKID;
							}
						}
					}
				}
				else
				{
					perpendicularFriction = driver->const_PedalFriction_Perpendicular;
					forwardFriction = driver->const_BrakeFriction;

					if (absSpeedApprox > VEH_PHYS_FORCE_SKID_SPEED_THRESHOLD)
					{
						actionsFlagSet |= ACTION_BACK_SKID;
					}
				}
			}
		}
		else
		{
			double absSpeedApprox = fabs(speedApprox);
			if (absSpeedApprox > VEH_PHYS_FORCE_SKID_SPEED_THRESHOLD)
			{
				actionsFlagSet |= ACTION_BACK_SKID;
			}

			perpendicularFriction = driver->const_BrakeFriction;
			if (driver->rainCloudEffect == RAIN_CLOUD_EFFECT_HEAVY_FRICTION)
			{
				perpendicularFriction = Smoothed_Up(perpendicularFriction, 4);
				forwardFriction = perpendicularFriction;
			}
			else if (driver->kartState == KS_BLASTED)
			{
				perpendicularFriction = Smoothed_Down((perpendicularFriction * 3), 2);
				forwardFriction = perpendicularFriction;
			}
			else
			{
				forwardFriction = perpendicularFriction;
				if (driver->kartState == KS_SPINNING)
				{
					perpendicularFriction = Smoothed_Down(perpendicularFriction, 1);
					forwardFriction = perpendicularFriction;
				}
			}
		}

		perpendicularFriction = Smoothed_Down((perpendicularFriction * elapsedTimeMS), 5);
		forwardFriction = Smoothed_Down((forwardFriction * elapsedTimeMS), 5);

		double terrainFrictionScale = driver->terrainMeta1->groundFrictionScale;
		if (terrainFrictionScale != VEH_PHYS_FORCE_TERRAIN_SCALE_NEUTRAL)
		{
			perpendicularFriction = Smoothed_Down((terrainFrictionScale * perpendicularFriction), 8);
			forwardFriction = Smoothed_Down((terrainFrictionScale * forwardFriction), 8);
		}

		int terrainTimer = driver->terrainFrictionTimer;
		if (terrainTimer < 0)
		{
			double absLocalX = localX;
			if (terrainTimer == VEH_PHYS_FORCE_TERRAIN_SIDE_LOCK_TIMER)
			{
				absLocalX = fabs(localX);
				perpendicularFriction = Smoothed_Down(absLocalX, 1);
			}
			else
			{
				perpendicularFriction =
					(perpendicularFriction + Smoothed_Down((perpendicularFriction * driver->const_TerrainFrictionBoost), 8));
				if (perpendicularFriction < 0)
				{
					perpendicularFriction = 0;
				}

				absLocalX = fabs(localX);
			}

			if (absLocalX > 0)
			{
				actionsFlagSet |= ACTION_BACK_SKID | ACTION_FRONT_SKID;
				GAMEPAD_ShockForce1(driver, VEH_PHYS_FORCE_TERRAIN_RUMBLE_FRAMES, VEH_PHYS_FORCE_TERRAIN_RUMBLE_FORCE);
				GAMEPAD_ShockFreq(driver, VEH_PHYS_FORCE_TERRAIN_RUMBLE_FRAMES, 0);
			}

			terrainTimer = (terrainTimer + sdata->gGT->elapsedTimeMS);
			if (terrainTimer > 0)
			{
				terrainTimer = 0;
			}
			driver->terrainFrictionTimer = (s16)terrainTimer;
		}
		else if (terrainTimer > 0)
		{
			terrainTimer = (terrainTimer - sdata->gGT->elapsedTimeMS);
			if (terrainTimer < 0)
			{
				terrainTimer = 0;
			}

			perpendicularFriction =
				(perpendicularFriction + Smoothed_Down((perpendicularFriction * driver->const_TerrainFrictionBoost), 8));
			driver->terrainFrictionTimer = (s16)terrainTimer;
			if (perpendicularFriction < 0)
			{
				perpendicularFriction = 0;
			}
		}

		if (((actionsFlagSet & ACTION_MASK_WEAPON) == 0) && ((terrainFlags & TERRAIN_FLAG_MUD_PHYSICS) != 0))
		{
			// These minimum drags are proportional decay, not constant forces.
			// Fractional powers preserve the retail 7/8 and 1/2 retention over
			// equal time intervals, including at non-integral rates such as 144 Hz.
			double frameScale = elapsedTimeMS / 32.0;
			double absSideSpeed = fabs(localX) * (1.0 - pow(0.875, frameScale));
			if (perpendicularFriction < absSideSpeed)
			{
				perpendicularFriction = absSideSpeed;
			}

			double minForwardFriction = 0;
			if ((localZ == 0) || (baseSpeed == 0) || ((localZ < 0) == (baseSpeed < 0)))
			{
				if (((baseSpeed <= localZ) || (localZ > 0)) && ((localZ <= baseSpeed) || (localZ < 0)))
				{
					goto APPLY_TERRAIN_FRICTION;
				}

				minForwardFriction = fabs(localZ - baseSpeed) * (1.0 - pow(0.5, frameScale));
			}
			else
			{
				minForwardFriction = fabs(localZ) * (1.0 - pow(0.5, frameScale));
			}

			if (forwardFriction < minForwardFriction)
			{
				forwardFriction = minForwardFriction;
			}
		}

	APPLY_TERRAIN_FRICTION:
	{
		double xFriction = perpendicularFriction;
		if ((terrainFlags & TERRAIN_FLAG_SIDESLIP_FRICTION) != 0)
		{
			xFriction = Smoothed_Down((perpendicularFriction * 3), 2);
			if (xFriction < forwardFriction)
			{
				xFriction = forwardFriction;
			}
		}

		localX = Smoothed_Approach(localX, xFriction, 0);
		localZ = Smoothed_Approach(localZ, forwardFriction, 0);
	}
	}

	*velocity = NativePhysics_RotateDriver(driver, (NativePhysicsVec){localX, localY, localZ}, 0);
	NativePhysics_WriteVelocity(driver, *velocity);
	*output = driver->velocity;
	driver->actionsFlagSet = actionsFlagSet;

	if ((actionsFlagSet & ACTION_AIRBORNE) == 0)
	{
		if ((driver->boolFirstFrameSinceRevEngine != 0) && (localZ != 0))
		{
			driver->forwardDir = (localZ < 0) ? -1 : 1;
			driver->boolFirstFrameSinceRevEngine = 0;
			goto CHECK_ROLLBACK_FROM_PREVIOUS_DIRECTION;
		}

		if (originalLocalZ < 0)
		{
			goto SET_FORWARD_DIRECTION_IF_NONNEGATIVE;
		}

		if (localZ < 0)
		{
			driver->forwardDir = -1;
		}

		if (originalLocalZ > 0)
		{
			goto CHECK_ROLLBACK_FROM_FORWARD;
		}

	SET_FORWARD_DIRECTION_IF_NONNEGATIVE:
		if (localZ >= 0)
		{
			driver->forwardDir = 1;
		}
	}

CHECK_ROLLBACK_FROM_PREVIOUS_DIRECTION:
	if (originalLocalZ >= 0)
	{
		goto CHECK_ROLLBACK_FROM_FORWARD;
	}

	if (localZ <= 0)
	{
		return;
	}
	goto START_ROLLBACK;

CHECK_ROLLBACK_FROM_FORWARD:
	if (localZ < 0)
	{
		goto START_ROLLBACK;
	}
	if (originalLocalZ > 0)
	{
		return;
	}
	if (localZ <= 0)
	{
		return;
	}

START_ROLLBACK:
	if (driver->vShiftWindowTimer != 0)
	{
		driver->vShiftCount = (s16)((u16)driver->vShiftCount + 1);
	}
	driver->vShiftWindowTimer = VEH_PHYS_FORCE_ROLLBACK_WINDOW_TIMER;
}
void NativePhysics_JumpAndFriction(struct Driver *d)
{

	// Jump state and feedback use the original rules; impulses retain fractions.


	if ((d->kartState != KS_DRIFTING) && ((d->actionsFlagSet & ACTION_MASK_WEAPON) == 0) && (d->reserves == 0))
	{
		int ampTurn = abs(CTR_MipsSra((s16)d->ampTurnState, 8));

		int turnDecrease = (ampTurn >= (u8)d->const_BackwardTurnRate ? d->const_TurnDecreaseRate : ampTurn * d->const_TurnDecreaseRate / (u8)d->const_BackwardTurnRate);
		int baseSpeed = d->baseSpeed;
		int absBaseSpeed = abs(baseSpeed);

		if (absBaseSpeed < turnDecrease)
		{
			turnDecrease = absBaseSpeed;
		}

		if (baseSpeed < 0)
		{
			d->baseSpeed = (s16)CTR_MipsAddLo((u16)d->baseSpeed, turnDecrease);
		}
		else
		{
			d->baseSpeed = (s16)CTR_MipsSubLo((u16)d->baseSpeed, turnDecrease);
		}
	}

	if (d->wallRubTimer != 0)
	{
		if (d->wallRubSpeedLimit < d->baseSpeed)
		{
			d->baseSpeed = d->wallRubSpeedLimit;
		}

		if (d->baseSpeed < CTR_MipsNegLo(d->wallRubSpeedLimit))
		{
			d->baseSpeed = (s16)CTR_MipsNegLo(d->wallRubSpeedLimit);
		}
	}

	NativePhysicsVec movement = NativePhysics_ReadVelocity(d);
	double speedLoss = 0;

	if ((d->actionsFlagSet & ACTION_TOUCH_GROUND) == 0)
	{
		goto CHECK_FOR_ANY_JUMP;
	}

	double acceleration = 0;

	if (((d->stepFlagSet & COLL_STEP_TRIGGER_TURBO_PAD_MASK) != 0) && (d->baseSpeed > 0))
	{
		acceleration = VEH_PHYS_JUMP_TURBO_PAD_ACCEL;
	}
	else if (d->baseSpeed != 0)
	{
		if (((d->terrainMeta1->flags & TERRAIN_FLAG_ACCEL_WHILE_REVERSE_SLIDING) == 0) || (d->baseSpeed < 1) || (d->speedApprox >= 0))
		{
			int speedApprox = d->speedApprox;
			int absSpeedApprox = abs(speedApprox);

			if ((absSpeedApprox > VEH_PHYS_JUMP_REVERSE_SLIDE_SPEED_COMPARE) && ((d->baseSpeed < 1) || (speedApprox < 1)) &&
				((d->baseSpeed >= 0) || (speedApprox >= 0)))
			{
				goto PROCESS_ACCEL;
			}
		}

		acceleration = (d->const_Accel_ClassStat + Smoothed_Up((s8)d->accelConst, 5) / 5);

		if ((d->stepFlagSet & COLL_STEP_TRIGGER_TURBO_PAD_MASK) == 0)
		{
			if ((d->reserves != 0) && (d->baseSpeed > 0))
			{
				acceleration = d->const_Accel_Reserves;
			}

			int slowUntilSpeed = d->terrainMeta1->slowUntilSpeed;
			if ((slowUntilSpeed != VEH_PHYS_JUMP_TERRAIN_SCALE_NEUTRAL) && ((d->actionsFlagSet & ACTION_MASK_WEAPON) == 0))
			{
				acceleration = Smoothed_Down((slowUntilSpeed * acceleration), VEH_PHYS_JUMP_SPEED_FIXED_SHIFT);
			}
		}
		else if (d->baseSpeed > 0)
		{
			acceleration = VEH_PHYS_JUMP_TURBO_PAD_ACCEL;
		}
	}

PROCESS_ACCEL:
{
	double forwardImpulse = Smoothed_Down((acceleration * NativePhysics_ElapsedMS(sdata->gGT->elapsedTimeMS)), 5);
	NativePhysicsVec rotated = NativePhysics_RotateDriver(d, (NativePhysicsVec){0, 0, forwardImpulse}, 0);

	if (d->baseSpeed < 0)
	{
		d->forwardAccelImpulse = (s16)CTR_MipsNegLo(forwardImpulse);

		movement.x = (movement.x - rotated.x);
		movement.y = (movement.y - rotated.y);
		movement.z = (movement.z - rotated.z);

		d->forwardAccelVector.x = (s16)CTR_MipsNegLo(rotated.x);
		d->forwardAccelVector.y = (s16)CTR_MipsNegLo(rotated.y);
		d->forwardAccelVector.z = (s16)CTR_MipsNegLo(rotated.z);
	}
	else
	{
		d->forwardAccelImpulse = (s16)forwardImpulse;

		movement.x = (movement.x + rotated.x);
		movement.y = (movement.y + rotated.y);
		movement.z = (movement.z + rotated.z);

		d->forwardAccelVector.x = (s16)rotated.x;
		d->forwardAccelVector.y = (s16)rotated.y;
		d->forwardAccelVector.z = (s16)rotated.z;
	}

	speedLoss = sqrt(movement.x * movement.x + movement.y * movement.y + movement.z * movement.z) - abs(d->baseSpeed);

	b32 clampToForwardImpulse = forwardImpulse < speedLoss;
	if (speedLoss < 0)
	{
		speedLoss = 0;
		clampToForwardImpulse = forwardImpulse < 0;
	}
	if (clampToForwardImpulse)
	{
		speedLoss = forwardImpulse;
	}

	if (((d->actionsFlagSet & ACTION_TOUCH_GROUND) == 0) || (d->jump_ForcedMS == 0))
	{
		goto CHECK_FOR_ANY_JUMP;
	}

	if (d->jump_HighJumpTimerMS != 0)
	{
		d->jump_HighJumpTimerMS = VEH_PHYS_JUMP_HIGH_TIMER_MS;
	}

	if (d->kartState == KS_BLASTED)
	{
		GAMEPAD_ShockFreq(d, VEH_PHYS_JUMP_RUMBLE_CHANNEL, 0);
		GAMEPAD_ShockForce1(d, VEH_PHYS_JUMP_RUMBLE_CHANNEL, VEH_PHYS_JUMP_RUMBLE_FORCE);
	}
}

	goto PROCESS_JUMP;

CHECK_FOR_ANY_JUMP:
	if (((d->actionsFlagSet & ACTION_WEAPON_FIRE_REQUEST) != 0) && (d->heldItemID == HELD_ITEM_SPRING))
	{
		d->actionsFlagSet &= ~ACTION_WEAPON_FIRE_REQUEST;

		if ((d->jump_CoyoteTimerMS != 0) && (d->jump_CooldownMS == 0))
		{
			d->jump_ForcedMS = VEH_PHYS_JUMP_FORCED_MS;

			int jumpForce = CTR_MipsAddLo(CTR_MipsSll(d->const_JumpForce, 3), d->const_JumpForce);
			d->jump_InitialVelY = (s16)(jumpForce / 4);

#if defined(__vita__)
			if (NativeAdhoc_ShouldPresentDriver(d->driverID))
#endif
			{
				OtherFX_Play_Echo(VEH_PHYS_JUMP_SPRING_SFX, 1, (d->actionsFlagSet & ACTION_ENGINE_ECHO) != 0);
			}

			d->jump_HighJumpTimerMS = VEH_PHYS_JUMP_HIGH_TIMER_MS;
			goto PROCESS_JUMP;
		}

		d->noItemTimer = 0;
	}

	if (d->forcedJumpType == FORCED_JUMP_NONE)
	{
		if ((d->jump_CoyoteTimerMS == 0) || (d->jump_TenBuffer == 0) || (d->jump_CooldownMS != 0))
		{
			if ((d->actionsFlagSet & ACTION_TOUCH_GROUND) != 0)
			{
				if ((d->underDriver != NULL) && (d->underDriver->mulNormVecY != 0))
				{
					int speedApprox = d->speedApprox;
					if (speedApprox < 0)
					{
						speedApprox = abs(speedApprox);
					}

					double antiGravVelY = Smoothed_Down((d->underDriver->mulNormVecY * speedApprox), 8);
					antiGravVelY *= NativePhysics_ElapsedMS(sdata->gGT->elapsedTimeMS) / 32.0;
					NativePhysicsVec rotated = NativePhysics_RotateDriver(d, (NativePhysicsVec){0, antiGravVelY, 0}, 0);

					movement.x = (movement.x + rotated.x);
					movement.y = (movement.y + rotated.y);
					movement.z = (movement.z + rotated.z);
				}
			}

			goto NOT_JUMPING;
		}

		d->jump_ForcedMS = VEH_PHYS_JUMP_FORCED_MS;
		d->numberOfJumps = (s16)CTR_MipsAddLo((u16)d->numberOfJumps, 1);
		d->jump_InitialVelY = d->const_JumpForce;

#if defined(__vita__)
		if (NativeAdhoc_ShouldPresentDriver(d->driverID))
#endif
		{
			OtherFX_Play_Echo(VEH_PHYS_JUMP_NORMAL_SFX, 1, (d->actionsFlagSet & ACTION_ENGINE_ECHO) != 0);
		}
	}
	else
	{
		if ((d->jump_ForcedMS == 0) || (d->jump_InitialVelY == d->const_JumpForce))
		{
#if defined(__vita__)
			if (NativeAdhoc_ShouldPresentDriver(d->driverID))
#endif
			{
				OtherFX_Play(VEH_PHYS_JUMP_FORCED_SFX, 1);
			}
		}

		d->jump_ForcedMS = VEH_PHYS_JUMP_FORCED_MS;

		int jumpForce = CTR_MipsAddLo(CTR_MipsSll(d->const_JumpForce, 1), d->const_JumpForce);
		if (d->forcedJumpType == FORCED_JUMP_HIGH)
		{
			d->jump_HighJumpTimerMS = VEH_PHYS_JUMP_HIGH_TIMER_MS;
			d->jump_InitialVelY = (s16)jumpForce;
		}
		else
		{
			d->jump_InitialVelY = (s16)(jumpForce / 2);
		}

		d->forcedJumpType = FORCED_JUMP_NONE;
	}

PROCESS_JUMP:
	d->jump_CooldownMS = VEH_PHYS_JUMP_COOLDOWN_MS;
	d->jump_TenBuffer = 0;
	d->actionsFlagSet |= ACTION_JUMP_STARTED | ACTION_TURBO_INPUT_LATCH;

	double bestJumpVelY = 0;
	double jumpVelY = Smoothed_JumpVelY(d->AxisAngle4_normalVec.v, &movement);
	if (fabs(bestJumpVelY) < fabs(jumpVelY))
	{
		bestJumpVelY = jumpVelY;
	}

	s16 *normalVec = d->AxisAngle1_normalVec.v;
	if ((d->actionsFlagSet & ACTION_TOUCH_GROUND) == 0)
	{
		normalVec = d->AxisAngle2_normalVec.v;
	}

	jumpVelY = Smoothed_JumpVelY(normalVec, &movement);

	double jumpVelYSquared = (bestJumpVelY * bestJumpVelY);
	if (fabs(bestJumpVelY) < fabs(jumpVelY))
	{
		jumpVelYSquared = (jumpVelY * jumpVelY);
		bestJumpVelY = jumpVelY;
	}

	double verticalSpeed = sqrt(jumpVelYSquared + (double)d->jump_InitialVelY * d->jump_InitialVelY);

	int maxVerticalSpeed = sdata->gGT->level1->jumpVerticalSpeedCap << VEH_PHYS_JUMP_SPEED_FIXED_SHIFT;
	if (maxVerticalSpeed == 0)
	{
		maxVerticalSpeed = VEH_PHYS_JUMP_VERTICAL_SPEED_DEFAULT;
	}
	else if (maxVerticalSpeed > VEH_PHYS_JUMP_VERTICAL_SPEED_MAX)
	{
		maxVerticalSpeed = VEH_PHYS_JUMP_VERTICAL_SPEED_MAX;
	}

	verticalSpeed = (verticalSpeed - bestJumpVelY);
	if (maxVerticalSpeed < verticalSpeed)
	{
		verticalSpeed = maxVerticalSpeed;
	}

	if (movement.y < verticalSpeed)
	{
		movement.y = verticalSpeed;
	}

NOT_JUMPING:
	NativePhysics_WriteVelocity(d, movement);
	NativePhysics_ConvertVecToSpeed(d, movement);

	double speed = (NativePhysics_GetSpeed(d) - speedLoss);
	NativePhysics_SetSpeed(d, speed);

	int speedApprox = d->speedApprox;
	if (speedApprox < 0)
	{
		speedApprox = abs(speedApprox);

		if (speedApprox < VEH_PHYS_JUMP_SPEEDOMETER_REVERSE_THRESHOLD)
		{
			d->speedometerNeedleValue =
				(s16)CTR_MipsSubLo((u16)d->speedometerNeedleValue, CTR_MipsSra(d->speedometerNeedleValue, VEH_PHYS_JUMP_SPEEDOMETER_DECAY_SHIFT));
		}
		else
		{
			d->speedometerNeedleValue =
				(s16)((u32)CTR_MipsAddLo(CTR_MipsMulLo(d->speedometerNeedleValue, VEH_PHYS_JUMP_SPEEDOMETER_BLEND_OLD),
										 CTR_MipsMulLo(sdata->gGT->timer & VEH_PHYS_JUMP_SPEEDOMETER_TIMER_MASK, VEH_PHYS_JUMP_SPEEDOMETER_TIMER_SCALE)) >>
					  VEH_PHYS_JUMP_SPEEDOMETER_BLEND_SHIFT);
		}
	}
	else
	{
		d->speedometerNeedleValue = (s16)CTR_MipsSra(CTR_MipsAddLo(CTR_MipsMulLo(d->speedometerNeedleValue, VEH_PHYS_JUMP_SPEEDOMETER_BLEND_OLD),
																   CTR_MipsMulLo(speedApprox, VEH_PHYS_JUMP_SPEEDOMETER_BLEND_NEW)),
													 VEH_PHYS_JUMP_SPEEDOMETER_BLEND_SHIFT);
	}
}

#endif
