#include <common.h>
#include <math.h>

#if defined(CTR_NATIVE) && !defined(__vita__)
u32 NativeCollision_Impact(struct Driver *d, struct Thread *t, struct ScratchpadStruct *sps, struct Scrub *scrub, Vec3 *output)
{
	NativePhysicsVec value=NativePhysics_ReadVelocity(d);
	NativePhysicsVec *velocity=&value;
	NativePhysicsVec normal=NativeCollision_Normal(sps);

	if ((d->vShiftCount != 0) && (sps->boolDidTouchQuadblock != 0) && ((P32_GET(struct QuadBlock *, sps->hit.ptrQuadblock)->quadFlags & QUADBLOCK_FLAG_GROUND) != 0) &&
	    (sps->hit.reorderResult != COLL_TRIANGLE_CLIP_FACE) && (P32_GET(struct QuadBlock *, sps->hit.ptrQuadblock) != P32_GET(struct QuadBlock *, d->underDriver)))
	{
		if ((abs(d->speedApprox) < 0x300) && (abs(d->jumpHeightCurr) < 0x300) && (d->fireSpeed == 0))
		{
			double diffX = (ldexp(d->posCurr.x, -(8)) - sps->hit.hitPos.x);
			double diffZ = (ldexp(d->posCurr.z, -(8)) - sps->hit.hitPos.z);
			double diffY = (ldexp(d->posCurr.y, -(8)) - sps->hit.hitPos.y);

			if (diffX != 0 || diffY != 0 || diffZ != 0)
			{
				double len = sqrt((diffX * diffX) + (diffY * diffY) + (diffZ * diffZ));

				normal.x = (ldexp(diffX, 12) / len);
				normal.y = (ldexp(diffY, 12) / len);
				normal.z = (ldexp(diffZ, 12) / len);
			}
		}
	}

	double dot =
	    ldexp((((ldexp(velocity->x, -(3)) * normal.x) + (ldexp(velocity->y, -(3)) * normal.y)) + (ldexp(velocity->z, -(3)) * normal.z)), -(9));

	if (dot < -0xa00)
	{
		d->actionsFlagSet |= ACTION_TURBO_INPUT_LATCH;
	}

	dot = (dot - sps->Input1.scrubDepth);

	u32 ret = 0;
	if (dot < 0)
	{
		u32 scrubFlags = scrub->flags;

		if ((scrubFlags & SCRUB_FLAG_SKIP_WALL_RUB_TIMER) == 0)
		{
			d->actionsFlagSet |= ACTION_DRIVING_AGAINST_WALL;
		}

		if ((scrubFlags & SCRUB_FLAG_KEEP_RESERVES) == 0)
		{
			d->reserves = 0;
			d->turbo_outsideTimer = 0;
		}

		double scrubSpeed = scrub->speedLimit;

		if (!((d->wallRubTimer == 0) ? (0x3e7ff < scrubSpeed) : (d->wallRubSpeedLimit < scrubSpeed)))
		{
			d->wallRubTimer = DRIVER_WALL_RUB_TIMER_START;
			d->wallRubSpeedLimit = scrubSpeed;
			d->posWallColl = sps->hit.hitPos;
		}

		ret = 0;

		if ((scrubFlags & SCRUB_FLAG_APPLY_IMPACT) != 0)
		{
			NativePhysicsVec impact = {
			    .x = ldexp((dot * normal.x), -(12)),
			    .y = ldexp((dot * normal.y), -(12)),
			    .z = ldexp((dot * normal.z), -(12)),
			};
			double speedSq = 0;

			if (scrub->impactAngle != 0)
			{
				speedSq = ldexp((((velocity->x * velocity->x) + (velocity->y * velocity->y)) + (velocity->z * velocity->z)), -(15));
			}

			double oldVelX = velocity->x;
			double oldVelZ = velocity->z;
			velocity->x = (oldVelX - impact.x);
			velocity->z = (oldVelZ - impact.z);
			velocity->y = (velocity->y - impact.y);

			impact = NativePhysics_RotateDriver(d, impact, 1);

			if ((sps->boolDidTouchQuadblock != 0) && ((sps->Union.QuadBlockColl.searchFlags & COLL_SEARCH_WALL_PROJECTION_DONE) == 0) &&
			    ((d->actionsFlagSetPrevFrame & ACTION_TOUCH_GROUND) == 0) && ((P32_GET(struct QuadBlock *, sps->hit.ptrQuadblock)->quadFlags & QUADBLOCK_FLAG_GROUND) != 0))
			{
				NativePhysicsVec wallVelocity;
				NativePhysicsVec oldVelocity={oldVelX,0,oldVelZ};
				double projected=(oldVelX*normal.x+oldVelZ*normal.z)/16777216.0;
				wallVelocity=(NativePhysicsVec){oldVelocity.x-normal.x*projected,-normal.y*projected,oldVelocity.z-normal.z*projected};

				double wallSpeedSq = (wallVelocity.x * wallVelocity.x) + (wallVelocity.y * wallVelocity.y) +
				                  (wallVelocity.z * wallVelocity.z);
				double wallSpeed = sqrt(wallSpeedSq);
				double speedApprox = d->speedApprox;

				if ((wallSpeed != 0) && (speedApprox > 0))
				{
					sps->Union.QuadBlockColl.searchFlags |= COLL_SEARCH_WALL_PROJECTION_DONE;
					velocity->x = ((wallVelocity.x * speedApprox) / wallSpeed);
					velocity->y = ((wallVelocity.y * speedApprox) / wallSpeed);
					velocity->z = ((wallVelocity.z * speedApprox) / wallSpeed);
					velocity->x = (velocity->x - ldexp(normal.x, -(1)));
					velocity->y = (velocity->y - ldexp(normal.y, -(1)));
					velocity->z = (velocity->z - ldexp(normal.z, -(1)));
				}
			}

			double transformedImpactXZ = ((impact.x * impact.x) + (impact.z * impact.z));

			if (((scrubFlags & SCRUB_FLAG_SLAM_ON_HARD_IMPACT) != 0) && (dot < -0x13ff) && (transformedImpactXZ > 0x1900000))
			{
				if (d->kartState != KS_MASK_GRABBED)
				{
					GAMEPAD_JogCon1(d, (d->simpTurnState < 1) ? 0x1f : 0x2f, 0x60);
				}

				if (scrub->impactAngle != 0)
				{
					double trig = sin(scrub->impactAngle*(6.2831853071795864769/4096.0))*4096.0;
					double scaledSpeed = ldexp((speedSq * trig), -(12));
					double angleLimit = ldexp((scaledSpeed * trig), -(12));
					double dotSq = ldexp((dot * dot), -(15));

					if (angleLimit >= dotSq)
					{
						NativePhysics_WriteVelocity(d,*velocity);
						*output=d->velocity;
						return 1;
					}
				}

				if ((d->kartState != KS_MASK_GRABBED) && (transformedImpactXZ > 0x1900000))
				{
					u32 echo = ((d->actionsFlagSet & ACTION_ENGINE_ECHO) != 0);
					u32 soundFlags = HowlSfx_Pack(HOWL_SFX_LR_CENTER, HOWL_SFX_DISTORTION_NONE, HOWL_SFX_VOLUME_MAX, echo);

					int shouldPresentImpact = 1;
#if defined(__vita__)
					if (NativeAdhoc_IsConnected())
					{
						shouldPresentImpact = NativeAdhoc_ShouldPresentDriver(d->driverID);
					}
#endif
					if (shouldPresentImpact)
					{
						OtherFX_Play_LowLevel(6, 1, soundFlags);
						Voiceline_RequestPlayDriver(6, d->driverID, 0x10);
						GAMEPAD_ShockFreq(d, 8, 0);
						GAMEPAD_ShockForce1(d, 8, 0x7f);
					}

					if (d->kartState == KS_DRIFTING)
					{
						s16 turnAngle = d->turnAngleCurr;

						d->turnAngleCurr = 0;
						d->angle = (d->angle + turnAngle);
						d->rotCurr.w = (d->rotCurr.w - turnAngle);
					}

					P32_GET(struct Instance *, d->instSelf)->animIndex = 2;
					P32_GET(struct Instance *, d->instSelf)->animFrame = 0;
					d->matrixArray = BAKED_GTE_MATRIX_CRASH_FALL;
					d->matrixIndex = 0;

					NativePhysics_WriteVelocity(d,*velocity);
					*output=d->velocity;
					VehPhysProc_SlamWall_Init(t, d);
					NativePhysics_ResetDriver(d);
					*output=d->velocity;
					return 2;
				}
			}

			ret = 1;
		}
	}

	NativePhysics_WriteVelocity(d,*velocity);
	*output=d->velocity;
	return ret;
}
#endif
