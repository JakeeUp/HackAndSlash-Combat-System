// Dynamic camera effects component -- DMC-accurate kill cam, Perlin trauma shake,
// velocity tilt, launch pitch bias, and group pull-back.

#include "HSDynamicCameraComponent.h"

#include "Character/HSPlayerCharacter.h"
#include "Character/HSDummyEnemy.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/EngineTypes.h"


UHSDynamicCameraComponent::UHSDynamicCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHSDynamicCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerPlayer = Cast<AHSPlayerCharacter>(GetOwner());
}

void UHSDynamicCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerPlayer) return;

	UpdateKillCam(DeltaTime);
	UpdateVelocityTilt(DeltaTime);
	UpdateLaunchPitch(DeltaTime);
	UpdateTrauma(DeltaTime);
	UpdateGroupPullback(DeltaTime);
	UpdateCinematicShot(DeltaTime);

	ArmLengthOffset = KillCamArmOffset + CurrentGroupPullback;
}

void UHSDynamicCameraComponent::NotifyEnemyKill()
{
	if (bKillCamActive) return;
	if (!CheckNoLivingEnemiesInRange()) return;

	bKillCamActive      = true;
	KillCamStartRealTime = GetWorld()->GetRealTimeSeconds();

	if (OwnerPlayer->CameraBoom)
	{
		KillCamBaseArmLength = OwnerPlayer->CameraBoom->TargetArmLength;
	}

	GetWorld()->GetWorldSettings()->SetTimeDilation(KillCamTimeDilation);
}

void UHSDynamicCameraComponent::NotifyEnemyLaunched()
{
	// Pulse the pitch upward; let UpdateLaunchPitch decay it naturally.
	CurrentLaunchPitch = FMath::Max(CurrentLaunchPitch, LaunchPitchAmount);
}

void UHSDynamicCameraComponent::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f);
}

void UHSDynamicCameraComponent::AddPitchKick(float Degrees)
{
	// Additive -- multiple kicks stack, then decay via UpdateLaunchPitch.
	CurrentLaunchPitch = FMath::Max(CurrentLaunchPitch + Degrees, 0.f);
}

void UHSDynamicCameraComponent::AddRollKick(float Degrees)
{
	// Additive in the direction the caller asked for.  Decays every frame.
	CurrentRollKick += Degrees;
}

FRotator UHSDynamicCameraComponent::GetCameraRotationOffset() const
{
	// GDC Eiserloh formula: shake = trauma², drives Perlin-sampled rotational offsets.
	const float Shake = FMath::Pow(FMath::Clamp(Trauma, 0.f, 1.f), 2.f);

	const float ShakePitch = MaxShakeAngle * Shake * FMath::PerlinNoise1D(11.f + NoiseTime);
	const float ShakeYaw   = MaxShakeAngle * Shake * FMath::PerlinNoise1D(37.f + NoiseTime);
	const float ShakeRoll  = MaxShakeAngle * Shake * FMath::PerlinNoise1D(71.f + NoiseTime);

	// Cinematic pitch/roll/yaw bias -- eased in via blend weight alongside arm/offset changes.
	// Yaw orbit is animated over the hold phase (sine-eased in UpdateCinematicShot) so the camera
	// subtly sweeps during the hero shot instead of sitting frozen.
	const float CinePitch = CinePitchBias  * CinematicBlendWeight;
	const float CineRoll  = CineRollBias   * CinematicBlendWeight;
	const float CineYaw   = CineYawOrbitLive * CinematicBlendWeight;

	return FRotator(
		-CurrentLaunchPitch + ShakePitch + CinePitch,   // negative = tilt up (UE pitch is inverted for camera)
		ShakeYaw + CineYaw,
		CurrentTiltRoll + CurrentRollKick + ShakeRoll + CineRoll
	);
}

// ── Kill cam ────────────────────────────────────────────────────────────────────

void UHSDynamicCameraComponent::UpdateKillCam(float DeltaTime)
{
	if (!bKillCamActive) return;

	// Use real (undilated) time so the kill cam duration is wall-clock accurate.
	const float RealElapsed = GetWorld()->GetRealTimeSeconds() - KillCamStartRealTime;
	const float Progress    = FMath::Clamp(RealElapsed / KillCamDuration, 0.f, 1.f);

	// Sine curve: arm pulls in during first half, eases back during second half.
	const float MaxPullIn = KillCamBaseArmLength * (1.f - KillCamArmFraction);
	KillCamArmOffset = -MaxPullIn * FMath::Sin(Progress * PI);

	if (RealElapsed >= KillCamDuration)
	{
		bKillCamActive   = false;
		KillCamArmOffset = 0.f;
		GetWorld()->GetWorldSettings()->SetTimeDilation(1.f);
	}
}

// ── Velocity tilt ────────────────────────────────────────────────────────────────

void UHSDynamicCameraComponent::UpdateVelocityTilt(float DeltaTime)
{
	if (FMath::IsNearlyZero(VelocityTiltMaxDegrees)) return;

	// Compute how much the player is strafing relative to the camera's right vector.
	float TargetRoll = 0.f;
	if (UCharacterMovementComponent* CMC = OwnerPlayer->GetCharacterMovement())
	{
		const FVector Vel = OwnerPlayer->GetVelocity();
		const float MaxSpeed = FMath::Max(CMC->MaxWalkSpeed, 1.f);

		if (!Vel.IsNearlyZero())
		{
			// Project velocity onto the actor's right axis.
			const float RightAmount = FVector::DotProduct(Vel.GetSafeNormal(), OwnerPlayer->GetActorRightVector());
			TargetRoll = -RightAmount * VelocityTiltMaxDegrees * (Vel.Size() / MaxSpeed);
		}
	}

	CurrentTiltRoll = FMath::FInterpTo(CurrentTiltRoll, TargetRoll, DeltaTime, VelocityTiltInterpSpeed);
}

// ── Launch pitch ────────────────────────────────────────────────────────────────

void UHSDynamicCameraComponent::UpdateLaunchPitch(float DeltaTime)
{
	if (CurrentLaunchPitch > 0.f)
	{
		CurrentLaunchPitch = FMath::FInterpTo(CurrentLaunchPitch, 0.f, DeltaTime, LaunchPitchDecaySpeed);
		if (CurrentLaunchPitch < 0.1f) CurrentLaunchPitch = 0.f;
	}

	// Roll kick decays in whichever direction it currently sits (positive or negative).
	if (!FMath::IsNearlyZero(CurrentRollKick))
	{
		CurrentRollKick = FMath::FInterpTo(CurrentRollKick, 0.f, DeltaTime, RollKickDecaySpeed);
		if (FMath::Abs(CurrentRollKick) < 0.05f) CurrentRollKick = 0.f;
	}
}

// ── Trauma shake ────────────────────────────────────────────────────────────────

void UHSDynamicCameraComponent::UpdateTrauma(float DeltaTime)
{
	if (Trauma > 0.f)
	{
		Trauma = FMath::Max(0.f, Trauma - TraumaDecayRate * DeltaTime);
	}

	// Advance Perlin noise time even when trauma is 0 so there's no pop when it returns.
	NoiseTime += DeltaTime * NoiseSpeed;
}

// ── Group pull-back ──────────────────────────────────────────────────────────────

void UHSDynamicCameraComponent::UpdateGroupPullback(float DeltaTime)
{
	// Scan on an interval to keep this cheap.
	GroupScanTimer -= DeltaTime;
	if (GroupScanTimer <= 0.f)
	{
		GroupScanTimer = GroupScanInterval;

		TArray<AActor*> FoundActors;
		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
		ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(OwnerPlayer);

		UKismetSystemLibrary::SphereOverlapActors(
			GetWorld(),
			OwnerPlayer->GetActorLocation(),
			GroupScanRange,
			ObjectTypes,
			AHSDummyEnemy::StaticClass(),
			IgnoreActors,
			FoundActors
		);

		int32 LivingCount = 0;
		for (AActor* Actor : FoundActors)
		{
			if (AHSDummyEnemy* Enemy = Cast<AHSDummyEnemy>(Actor))
			{
				if (!Enemy->IsDead()) LivingCount++;
			}
		}

		const int32 ExcessEnemies = FMath::Max(0, LivingCount - GroupPullbackMinEnemies);
		TargetGroupPullback = FMath::Min(ExcessEnemies * PullbackPerEnemy, MaxPullbackOffset);
	}

	CurrentGroupPullback = FMath::FInterpTo(CurrentGroupPullback, TargetGroupPullback, DeltaTime, PullbackInterpSpeed);
}

// ── Cinematic shot ───────────────────────────────────────────────────────────

void UHSDynamicCameraComponent::PlayCinematicShot(float InArmLength, FVector InSocketOffset, float InFOV, float InPitchBias,
                                                   float InRollBias,
                                                   float InBlendIn, float InHold, float InBlendOut,
                                                   float InDollyArmDelta, float InDollyXDelta, float InOrbitYawDelta)
{
	CineArmLength    = InArmLength;
	CineSocketOffset = InSocketOffset;
	CineFOV          = InFOV;
	CinePitchBias    = InPitchBias;
	CineRollBias     = InRollBias;

	CineDollyArmDelta = InDollyArmDelta;
	CineDollyXDelta   = InDollyXDelta;
	CineOrbitYawDelta = InOrbitYawDelta;

	// Initialize live values to peak -- UpdateCinematicShot will animate the hold delta on top
	CineArmLengthLive    = InArmLength;
	CineSocketOffsetLive = InSocketOffset;
	CineYawOrbitLive     = 0.f;

	CinematicBlendIn  = FMath::Max(0.01f, InBlendIn);
	CinematicHold     = FMath::Max(0.f,   InHold);
	CinematicBlendOut = FMath::Max(0.01f, InBlendOut);

	CinematicPhase     = ECinematicPhase::BlendIn;
	CinematicPhaseTime = 0.f;
	// Preserve existing blend weight so a new cinematic fired mid-blend starts from current value
}

void UHSDynamicCameraComponent::StopCinematicShot()
{
	if (CinematicPhase == ECinematicPhase::Inactive) return;

	CinematicPhase     = ECinematicPhase::BlendOut;
	CinematicPhaseTime = 0.f;
}

void UHSDynamicCameraComponent::UpdateCinematicShot(float DeltaTime)
{
	if (CinematicPhase == ECinematicPhase::Inactive)
	{
		CinematicBlendWeight = 0.f;
		return;
	}

	CinematicPhaseTime += DeltaTime;

	// MotionT: 0 → 1 tracks progress through the *total* shot (blend-in + hold + blend-out).
	// Sine-eased so the dolly/orbit feels like a continuous camera motion instead of a keyframe jump
	// at the phase boundaries. DmC 2013's cinematic beats have smooth push-ins over the whole duration.
	const float TotalDuration = CinematicBlendIn + CinematicHold + CinematicBlendOut;
	float ElapsedTotal = CinematicPhaseTime;
	if (CinematicPhase == ECinematicPhase::Hold)       ElapsedTotal += CinematicBlendIn;
	else if (CinematicPhase == ECinematicPhase::BlendOut) ElapsedTotal += CinematicBlendIn + CinematicHold;

	const float MotionT = FMath::Clamp(ElapsedTotal / FMath::Max(TotalDuration, 0.01f), 0.f, 1.f);
	// Sine ease: fast-in-slow-out for the push, feels like natural camera momentum
	const float MotionEase = FMath::Sin(MotionT * HALF_PI);

	CineArmLengthLive    = CineArmLength + CineDollyArmDelta * MotionEase;
	CineSocketOffsetLive = CineSocketOffset + FVector(CineDollyXDelta * MotionEase, 0.f, 0.f);
	CineYawOrbitLive     = CineOrbitYawDelta * MotionEase;

	switch (CinematicPhase)
	{
	case ECinematicPhase::BlendIn:
	{
		const float T = FMath::Clamp(CinematicPhaseTime / CinematicBlendIn, 0.f, 1.f);
		// Ease in-out so the cut to cinematic framing feels intentional, not jerky
		CinematicBlendWeight = FMath::SmoothStep(0.f, 1.f, T);
		if (T >= 1.f)
		{
			CinematicPhase = ECinematicPhase::Hold;
			CinematicPhaseTime = 0.f;
		}
		break;
	}

	case ECinematicPhase::Hold:
	{
		CinematicBlendWeight = 1.f;
		if (CinematicPhaseTime >= CinematicHold)
		{
			CinematicPhase = ECinematicPhase::BlendOut;
			CinematicPhaseTime = 0.f;
		}
		break;
	}

	case ECinematicPhase::BlendOut:
	{
		const float T = FMath::Clamp(CinematicPhaseTime / CinematicBlendOut, 0.f, 1.f);
		CinematicBlendWeight = 1.f - FMath::SmoothStep(0.f, 1.f, T);
		if (T >= 1.f)
		{
			CinematicPhase = ECinematicPhase::Inactive;
			CinematicBlendWeight = 0.f;
		}
		break;
	}

	default:
		break;
	}
}

// ── Kill cam helper ──────────────────────────────────────────────────────────────

bool UHSDynamicCameraComponent::CheckNoLivingEnemiesInRange() const
{
	if (!OwnerPlayer) return false;

	TArray<AActor*> FoundActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(OwnerPlayer);

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		OwnerPlayer->GetActorLocation(),
		KillCamScanRange,
		ObjectTypes,
		AHSDummyEnemy::StaticClass(),
		IgnoreActors,
		FoundActors
	);

	for (AActor* Actor : FoundActors)
	{
		if (const AHSDummyEnemy* Enemy = Cast<AHSDummyEnemy>(Actor))
		{
			if (!Enemy->IsDead()) return false;
		}
	}

	return true;
}
