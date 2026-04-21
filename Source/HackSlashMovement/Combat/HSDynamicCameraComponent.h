// Dynamic camera effects component -- DMC-accurate kill cam, Perlin trauma shake,
// velocity tilt, launch pitch bias, and group pull-back.
// Attach to HSPlayerCharacter; player Tick queries it for additive offsets.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSDynamicCameraComponent.generated.h"


class AHSPlayerCharacter;


UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class HACKSLASHMOVEMENT_API UHSDynamicCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSDynamicCameraComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/*****************************************************/
	/*            Notifications from gameplay            */
	/*****************************************************/

	/** Call from HSDummyEnemy::Die().  Triggers kill cam if it was the last enemy in range. */
	void NotifyEnemyKill();

	/** Call when a launcher hit lands.  Adds upward pitch bias so you can see the juggle. */
	void NotifyEnemyLaunched();

	/** Call when the player is hit.  Drives the Perlin-noise trauma shake. */
	void AddTrauma(float Amount);

	/** Add a transient pitch kick (degrees) that decays the same way NotifyEnemyLaunched does.
	 *  Positive values tilt the camera up. Use for shotgun-kickback, drive wave, heavy-hit pop. */
	void AddPitchKick(float Degrees);

	/** Add a transient roll kick (degrees) that decays over a short interval.
	 *  Useful for side-directional hits/kickback so the camera "snaps" in the direction of impact. */
	void AddRollKick(float Degrees);

	/*****************************************************/
	/*           Output queried by player Tick           */
	/*****************************************************/

	/** Additive arm length offset to add to DesiredArmLength this frame. */
	FORCEINLINE float GetArmLengthOffset() const { return ArmLengthOffset; }

	/** Additive rotation offset to apply to FollowCamera's relative rotation. */
	FRotator GetCameraRotationOffset() const;

	/*****************************************************/
	/*              Cinematic Shot                       */
	/*****************************************************/

	/** Trigger a timed cinematic camera override (DmC-style "hero shot" for key attack frames).
	 *  Blends the arm length / socket offset / FOV toward the cinematic values, holds, then blends back.
	 *  During the hold phase the camera DOLLIES and ORBITS slightly (sine-eased) so the shot feels
	 *  alive instead of frozen -- DmC 2013's cinematic beats always have subtle motion.
	 *  Called from anim notifies placed on specific hit frames.
	 *
	 *  @param InArmLength      Cinematic arm length at blend-in peak (pull-in close, e.g. 160-200).
	 *  @param InSocketOffset   Cinematic socket offset (use negative Z for low-angle upward look).
	 *  @param InFOV            FOV override at peak (0 = don't override).
	 *  @param InPitchBias      Extra pitch (degrees) layered on look.  Negative = tilt up.
	 *  @param InRollBias       Extra roll (degrees) for dutch angle.  Sign controls tilt direction.
	 *  @param InBlendIn        Seconds to blend in from current framing.
	 *  @param InHold           Seconds to hold at cinematic framing (during which dolly/orbit animate).
	 *  @param InBlendOut       Seconds to blend back to normal.
	 *  @param InDollyArmDelta  How much the arm length shrinks (negative = pull in further) over the hold.  Default -25 = subtle push-in.
	 *  @param InDollyXDelta    How much socket X advances (positive = forward) over the hold.  Default 20.
	 *  @param InOrbitYawDelta  Degrees of subtle yaw orbit applied over the hold (extra rotation offset on camera). */
	UFUNCTION(BlueprintCallable, Category = "Cinematic")
	void PlayCinematicShot(float InArmLength, FVector InSocketOffset, float InFOV, float InPitchBias,
	                       float InRollBias = 0.f,
	                       float InBlendIn = 0.08f, float InHold = 0.2f, float InBlendOut = 0.25f,
	                       float InDollyArmDelta = -25.f, float InDollyXDelta = 20.f, float InOrbitYawDelta = 6.f);

	/** Cancel the cinematic shot immediately (blend out at normal speed). */
	UFUNCTION(BlueprintCallable, Category = "Cinematic")
	void StopCinematicShot();

	/** 0 = inactive, 1 = full cinematic framing.  Smooth blend between based on internal timer. */
	float GetCinematicBlendWeight() const { return CinematicBlendWeight; }

	/** The cinematic shot's current target arm length (including hold-phase dolly animation).
	 *  Player Tick lerps toward this by CinematicBlendWeight. */
	float GetCinematicArmLength() const { return CineArmLengthLive; }

	/** Current target socket offset (including hold-phase dolly animation). */
	FVector GetCinematicSocketOffset() const { return CineSocketOffsetLive; }

	/** Returns 0 if FOV isn't being overridden by the cinematic shot. */
	float GetCinematicFOV() const { return CineFOV; }

	/** Extra pitch (degrees) the cinematic shot wants layered on top of lock-on look-at. */
	float GetCinematicPitchBias() const { return CinePitchBias * CinematicBlendWeight; }

	/** Extra roll (dutch) degrees from the cinematic shot, already weighted by blend. */
	float GetCinematicRollBias() const { return CineRollBias * CinematicBlendWeight; }

	/** Extra yaw (orbit) degrees from the hold-phase orbit animation, already weighted by blend. */
	float GetCinematicYawBias() const { return CineYawOrbitLive * CinematicBlendWeight; }

protected:
	/*****************************************************/
	/*                    Kill Cam                       */
	/*****************************************************/

	/** Radius to scan for living enemies when deciding whether to trigger kill cam. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|KillCam", meta = (ClampMin = "100.0"))
	float KillCamScanRange = 1800.f;

	/** Time dilation applied during the kill cam window (0.25 = quarter speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|KillCam", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float KillCamTimeDilation = 0.25f;

	/** Real-world duration of the kill cam in seconds (unaffected by time dilation). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|KillCam", meta = (ClampMin = "0.1"))
	float KillCamDuration = 0.85f;

	/** Arm pulls in to this fraction of its current length during kill cam (0.6 = 40% closer). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|KillCam", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float KillCamArmFraction = 0.6f;

	/*****************************************************/
	/*               Velocity Tilt (Roll)                */
	/*****************************************************/

	/** Maximum camera roll in degrees driven by lateral movement velocity.
	 *  The effect is subconscious at ≤4°.  Disable by setting to 0. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|VelocityTilt", meta = (ClampMin = "0.0", ClampMax = "15.0"))
	float VelocityTiltMaxDegrees = 3.5f;

	/** How fast the tilt follows the velocity change. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|VelocityTilt", meta = (ClampMin = "0.1"))
	float VelocityTiltInterpSpeed = 5.f;

	/*****************************************************/
	/*              Launch Pitch Bias                    */
	/*****************************************************/

	/** Upward camera pitch added when a launcher hit lands (degrees).
	 *  Gives a natural "look up to see the juggle" feel. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LaunchPitch", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float LaunchPitchAmount = 8.f;

	/** How fast the pitch bias decays back to 0 after the launch. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LaunchPitch", meta = (ClampMin = "0.1"))
	float LaunchPitchDecaySpeed = 2.5f;

	/*****************************************************/
	/*        Trauma Shake (GDC Eiserloh formula)        */
	/*****************************************************/

	/** Trauma decays at this rate per second.  0.75 = full trauma gone in ~1.3s. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Shake", meta = (ClampMin = "0.1"))
	float TraumaDecayRate = 0.75f;

	/** Speed at which the Perlin noise seed advances.  Higher = jerkier shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Shake", meta = (ClampMin = "1.0"))
	float NoiseSpeed = 20.f;

	/** Max rotation per axis at full trauma (degrees).
	 *  Applied as shake = trauma² so it's exponential -- subtle at low values, strong at full. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Shake", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float MaxShakeAngle = 7.f;

	/*****************************************************/
	/*               Group Pull-back                     */
	/*****************************************************/

	/** Radius around the player to scan for nearby enemies. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|GroupPullback", meta = (ClampMin = "100.0"))
	float GroupScanRange = 1100.f;

	/** Enemy count at which pull-back starts. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|GroupPullback", meta = (ClampMin = "1"))
	int32 GroupPullbackMinEnemies = 3;

	/** Extra arm length added per enemy above the minimum threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|GroupPullback", meta = (ClampMin = "0.0"))
	float PullbackPerEnemy = 45.f;

	/** Maximum extra arm length from group pull-back. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|GroupPullback", meta = (ClampMin = "0.0"))
	float MaxPullbackOffset = 220.f;

	/** Interp speed for the group pull-back arm extension (slow so it's not jarring). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|GroupPullback", meta = (ClampMin = "0.1"))
	float PullbackInterpSpeed = 1.5f;

	/** Seconds between enemy-count scans.  0.5s is cheap and imperceptible. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|GroupPullback", meta = (ClampMin = "0.1"))
	float GroupScanInterval = 0.5f;

private:
	UPROPERTY()
	TObjectPtr<AHSPlayerCharacter> OwnerPlayer;

	// ── Kill cam ──────────────────────────────────────────────────────────
	bool  bKillCamActive      = false;
	float KillCamStartRealTime = 0.f;
	float KillCamBaseArmLength = 0.f;
	float KillCamArmOffset     = 0.f;

	// ── Velocity tilt ──────────────────────────────────────────────────────
	float CurrentTiltRoll = 0.f;

	// ── Launch pitch ──────────────────────────────────────────────────────
	float CurrentLaunchPitch = 0.f;

	// ── Transient roll kick (directional hit feedback) ──────────────────────
	float CurrentRollKick = 0.f;

	/** How fast the roll kick decays back to 0. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LaunchPitch", meta = (ClampMin = "0.1"))
	float RollKickDecaySpeed = 8.f;

	// ── Trauma shake ──────────────────────────────────────────────────────
	float Trauma    = 0.f;
	float NoiseTime = 0.f;

	// ── Group pull-back ────────────────────────────────────────────────────
	float GroupScanTimer          = 0.f;
	float TargetGroupPullback     = 0.f;
	float CurrentGroupPullback    = 0.f;

	// ── Combined arm offset ───────────────────────────────────────────────
	float ArmLengthOffset = 0.f;

	// ── Cinematic shot state ──────────────────────────────────────────────
	enum class ECinematicPhase : uint8
	{
		Inactive,
		BlendIn,
		Hold,
		BlendOut
	};

	ECinematicPhase CinematicPhase = ECinematicPhase::Inactive;
	float CinematicPhaseTime = 0.f;
	float CinematicBlendWeight = 0.f;

	// Peak values -- what we're aiming for at weight=1
	float   CineArmLength   = 0.f;
	FVector CineSocketOffset = FVector::ZeroVector;
	float   CineFOV         = 0.f;
	float   CinePitchBias   = 0.f;
	float   CineRollBias    = 0.f;

	// Hold-phase motion deltas -- sine-eased from 0 to this value over the hold
	float   CineDollyArmDelta = 0.f;
	float   CineDollyXDelta   = 0.f;
	float   CineOrbitYawDelta = 0.f;

	// Live values (peak + current motion delta) -- queried by player Tick
	float   CineArmLengthLive    = 0.f;
	FVector CineSocketOffsetLive = FVector::ZeroVector;
	float   CineYawOrbitLive     = 0.f;

	float CinematicBlendIn  = 0.08f;
	float CinematicHold     = 0.2f;
	float CinematicBlendOut = 0.25f;

	// ── Internal update functions ─────────────────────────────────────────
	void UpdateKillCam(float DeltaTime);
	void UpdateVelocityTilt(float DeltaTime);
	void UpdateLaunchPitch(float DeltaTime);
	void UpdateTrauma(float DeltaTime);
	void UpdateGroupPullback(float DeltaTime);
	void UpdateCinematicShot(float DeltaTime);

	/** Returns true if no living AHSDummyEnemy exists within KillCamScanRange. */
	bool CheckNoLivingEnemiesInRange() const;
};
