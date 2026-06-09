#include "HSEnemyCombatAI.h"

#include "Character/HSDummyEnemy.h"
#include "Character/HSPlayerCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/UnrealMathUtility.h"

// ── Global attack token counter ───────────────────────────────────────────────
// All UHSEnemyCombatAI instances share this.  TryAcquireToken / ReleaseTokenIfHeld
// keep it within [0, MaxSimultaneousAttackers].
static int32 GActiveAttackers = 0;

UHSEnemyCombatAI::UHSEnemyCombatAI()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f; // every frame
}

void UHSEnemyCombatAI::BeginPlay()
{
	Super::BeginPlay();

	OwnerEnemy = Cast<AHSDummyEnemy>(GetOwner());
	TryFindPlayer();

	// Stagger start times so a room full of enemies doesn't all rush at once
	StateTimer        = FMath::RandRange(IdleTimeMin, IdleTimeMax * 1.5f);
	AttackDecisionTimer = FMath::RandRange(0.5f, AttackDecisionInterval);
	StrafeDirectionTimer = StrafeDirectionChangePeriod * FMath::RandRange(0.3f, 1.f);
	bStrafeClockwise  = FMath::RandBool();

	// AI-controlled enemies need to walk -- override the dummy's default 0
	SetMovementSpeed(0.f);
}

void UHSEnemyCombatAI::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerEnemy || OwnerEnemy->IsDead())
	{
		ReleaseTokenIfHeld();
		SetComponentTickEnabled(false);
		return;
	}

	// ── Training-dummy gate ──────────────────────────────────────────────────
	// Toggled on the dummy to make it a passive target: drop any held attack
	// token, stop walking, and skip every state tick. Hit reacts still work
	// because they run on the actor itself, not here.
	if (OwnerEnemy->bTrainingDummy)
	{
		if (bHoldsToken)
		{
			ReleaseTokenIfHeld();
		}
		if (AIState != EAIState::Idle)
		{
			AIState = EAIState::Idle;
		}
		SetMovementSpeed(0.f);
		StopNavMovement();
		return;
	}

	// Lazily find the player if we lost the reference
	if (!PlayerChar)
	{
		TryFindPlayer();
		if (!PlayerChar) return;
	}

	// ── Suspension check ─────────────────────────────────────────────────────
	// While the enemy is being juggled or getting up, AI is fully paused.
	// This integrates cleanly with the existing hit-react / launch system.
	const EEnemyState EState = OwnerEnemy->GetEnemyState();
	if (EState == EEnemyState::EES_Airborne || EState == EEnemyState::EES_Down)
	{
		if (AIState != EAIState::Suspended)
		{
			ReleaseTokenIfHeld();
			AIState = EAIState::Suspended;
			SetMovementSpeed(0.f);
		}
		return;
	}

	// Recovering from suspension → re-enter Idle with a short wait
	if (AIState == EAIState::Suspended)
	{
		TransitionToIdle();
	}

	// ── Rotate to face player (smooth, every state) ───────────────────────────
	RotateTowardPlayer(DeltaTime);

	// ── Run current state ─────────────────────────────────────────────────────
	switch (AIState)
	{
	case EAIState::Idle:      TickIdle(DeltaTime);      break;
	case EAIState::Strafe:    TickStrafe(DeltaTime);    break;
	case EAIState::Approach:  TickApproach(DeltaTime);  break;
	case EAIState::Windup:    TickWindup(DeltaTime);    break;
	case EAIState::Attacking: TickAttacking(DeltaTime); break;
	case EAIState::Retreat:   TickRetreat(DeltaTime);   break;
	case EAIState::Staggered: TickStaggered(DeltaTime); break;
	default: break;
	}

	// ── Crowd separation (always, regardless of state) ────────────────────────
	ApplySeparation();
}

// ─────────────────────────────────────────────────────────────────────────────
//  External notifications
// ─────────────────────────────────────────────────────────────────────────────

void UHSEnemyCombatAI::NotifyHit(EHitWeight HitWeight)
{
	// Launcher / finisher send the enemy airborne -- the suspension check in
	// TickComponent handles those automatically; no extra state logic needed here.
	if (HitWeight == EHitWeight::EHW_Launcher || HitWeight == EHitWeight::EHW_Finisher)
	{
		ReleaseTokenIfHeld();
		// AIState will flip to Suspended on the next tick when EnemyState = Airborne
		return;
	}

	// Release attack token so another enemy may attack
	ReleaseTokenIfHeld();

	const float Duration = (HitWeight == EHitWeight::EHW_Heavy)
		? HeavyStaggerDuration
		: LightStaggerDuration;

	AIState    = EAIState::Staggered;
	StateTimer = Duration;
	SetMovementSpeed(0.f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  State tick handlers
// ─────────────────────────────────────────────────────────────────────────────

void UHSEnemyCombatAI::TickIdle(float DeltaTime)
{
	StopNavMovement();
	SetMovementSpeed(0.f);

	StateTimer -= DeltaTime;
	if (StateTimer <= 0.f)
	{
		TransitionToStrafe();
	}
}

void UHSEnemyCombatAI::TickStrafe(float DeltaTime)
{
	const float Dist = GetDistToPlayer();

	// If the player ran far away, skip orbit and just approach
	if (Dist > MaxEngagementRange)
	{
		TransitionToApproach();
		return;
	}

	SetMovementSpeed(StrafeSpeed);

	// Compute the orbit tangent direction
	FVector ToPlayer = GetDirectionToPlayer();
	ToPlayer.Z = 0.f;
	ToPlayer.Normalize();

	// Perpendicular tangent (clockwise or counter-clockwise)
	FVector Tangent = FVector::CrossProduct(FVector::UpVector, ToPlayer);
	if (!bStrafeClockwise) Tangent = -Tangent;

	// Radial correction: blend in approach/retreat if we've drifted off orbit radius
	FVector MoveDir = Tangent;
	const float RadiusDiff = Dist - StrafeRadius;
	if (RadiusDiff < -StrafeRadiusTolerance)
	{
		// Too close -- blend in some retreat
		const float t = FMath::Clamp(-RadiusDiff / StrafeRadiusTolerance, 0.f, 1.f);
		MoveDir = FMath::Lerp(Tangent, -ToPlayer, t * 0.5f);
	}
	else if (RadiusDiff > StrafeRadiusTolerance)
	{
		// Too far -- blend in some approach
		const float t = FMath::Clamp(RadiusDiff / (StrafeRadiusTolerance * 2.f), 0.f, 1.f);
		MoveDir = FMath::Lerp(Tangent, ToPlayer, t * 0.5f);
	}

	MoveDir.Z = 0.f;
	if (!MoveDir.IsNearlyZero())
	{
		OwnerEnemy->AddMovementInput(MoveDir.GetSafeNormal(), 1.f);
	}

	// Periodically reverse strafe direction for unpredictability
	StrafeDirectionTimer -= DeltaTime;
	if (StrafeDirectionTimer <= 0.f)
	{
		bStrafeClockwise = !bStrafeClockwise;
		StrafeDirectionTimer = StrafeDirectionChangePeriod + FMath::RandRange(-0.5f, 0.5f);
	}

	// Attack decision roll
	AttackDecisionTimer -= DeltaTime;
	if (AttackDecisionTimer <= 0.f)
	{
		AttackDecisionTimer = AttackDecisionInterval + FMath::RandRange(-0.2f, 0.3f);

		if (FMath::FRand() < AttackTriggerChance && TryAcquireToken())
		{
			TransitionToApproach();
		}
	}
}

void UHSEnemyCombatAI::TickApproach(float DeltaTime)
{
	const float Dist = GetDistToPlayer();

	if (Dist <= AttackStartRange)
	{
		TransitionToWindup();
		return;
	}

	// Stuck-detection: if we can't reach the player after ApproachTimeout seconds,
	// give up and return to strafe so we don't lock the token forever
	ApproachTimeoutTimer -= DeltaTime;
	if (ApproachTimeoutTimer <= 0.f)
	{
		ReleaseTokenIfHeld();
		TransitionToStrafe();
	}
	// NavMesh path following drives movement -- MoveToActor issued in TransitionToApproach
}

void UHSEnemyCombatAI::TickWindup(float DeltaTime)
{
	// Stand still while telegraphing -- gives the player time to read and react
	StopNavMovement();
	SetMovementSpeed(0.f);

	StateTimer -= DeltaTime;
	if (StateTimer <= 0.f)
	{
		TransitionToAttacking();
	}
}

void UHSEnemyCombatAI::TickAttacking(float DeltaTime)
{
	// Count down the delay before the hit check fires
	if (AttackDamageTimer > 0.f)
	{
		AttackDamageTimer -= DeltaTime;
		if (AttackDamageTimer <= 0.f && !bAttackHasHit)
		{
			ApplyAttackHit();
		}
	}

	// Wait for the full attack duration before retreating
	StateTimer -= DeltaTime;
	if (StateTimer <= 0.f)
	{
		TransitionToRetreat();
	}
}

void UHSEnemyCombatAI::TickRetreat(float DeltaTime)
{
	// NavMesh movement issued in TransitionToRetreat -- just count down the timer
	StateTimer -= DeltaTime;
	if (StateTimer <= 0.f)
	{
		ReleaseTokenIfHeld();
		TransitionToIdle();
	}
}

void UHSEnemyCombatAI::TickStaggered(float DeltaTime)
{
	StopNavMovement();
	SetMovementSpeed(0.f);

	StateTimer -= DeltaTime;
	if (StateTimer <= 0.f)
	{
		TransitionToStrafe();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  State transitions
// ─────────────────────────────────────────────────────────────────────────────

void UHSEnemyCombatAI::TransitionToIdle()
{
	AIState    = EAIState::Idle;
	StateTimer = FMath::RandRange(IdleTimeMin, IdleTimeMax);
	SetMovementSpeed(0.f);
}

void UHSEnemyCombatAI::TransitionToStrafe()
{
	AIState = EAIState::Strafe;
	// Randomise first decision delay so enemies don't all attack on the same beat
	AttackDecisionTimer = AttackDecisionInterval + FMath::RandRange(0.f, AttackDecisionInterval * 0.5f);

	// Cancel any active NavMesh request -- strafe uses AddMovementInput orbit math
	StopNavMovement();
	SetMovementSpeed(StrafeSpeed);
}

void UHSEnemyCombatAI::TransitionToApproach()
{
	AIState = EAIState::Approach;
	ApproachTimeoutTimer = ApproachTimeout;
	SetMovementSpeed(ApproachSpeed);

	// Issue a NavMesh move-to-actor request.  UE's PathFollowingComponent tracks
	// the player's position automatically so this one call is enough; no per-frame
	// re-issue needed.
	if (AAIController* AIC = GetAIController())
	{
		AIC->MoveToActor(PlayerChar, AttackStartRange * 0.85f, true /* stop on overlap */);
	}
}

void UHSEnemyCombatAI::TransitionToWindup()
{
	AIState    = EAIState::Windup;
	StateTimer = WindupDuration;
	SetMovementSpeed(0.f);
	// The ABP can read GetAIState() == Windup to show a telegraph pose/effect
}

void UHSEnemyCombatAI::TransitionToAttacking()
{
	AIState          = EAIState::Attacking;
	StateTimer       = AttackStateDuration;
	AttackDamageTimer = AttackDamageDelay;
	bAttackHasHit    = false;

	// Play a random attack montage; sync the attack duration to the clip length
	if (AttackMontages.Num() > 0 && OwnerEnemy)
	{
		const int32 Idx = FMath::RandRange(0, AttackMontages.Num() - 1);
		if (UAnimMontage* Montage = AttackMontages[Idx])
		{
			if (UAnimInstance* AnimInst = OwnerEnemy->GetMesh() ? OwnerEnemy->GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInst->Montage_Play(Montage, 1.f);
				StateTimer = FMath::Max(Montage->GetPlayLength() + 0.15f, AttackStateDuration);
			}
		}
	}

	// Lunge toward the player
	FVector LungeDir = GetDirectionToPlayer();
	LungeDir.Z = 0.f;
	if (!LungeDir.IsNearlyZero())
	{
		OwnerEnemy->LaunchCharacter(LungeDir.GetSafeNormal() * AttackLungeForce, true, false);
	}

	SetMovementSpeed(0.f);
}

void UHSEnemyCombatAI::TransitionToRetreat()
{
	AIState    = EAIState::Retreat;
	StateTimer = RetreatDuration + FMath::RandRange(-0.2f, 0.3f);
	SetMovementSpeed(RetreatSpeed);

	// Compute a point directly behind the enemy (away from the player) and
	// let the NavMesh path there.  bProjectDestinationToNavigation snaps the
	// target onto the navmesh surface so the request doesn't silently fail.
	if (AAIController* AIC = GetAIController())
	{
		FVector Away = -GetDirectionToPlayer();
		Away.Z = 0.f;
		const FVector RetreatTarget = OwnerEnemy->GetActorLocation() + Away.GetSafeNormal() * 350.f;
		AIC->MoveToLocation(RetreatTarget, 50.f,
			true,   // bStopOnOverlap
			true,   // bUsePathfinding
			true,   // bProjectDestinationToNavigation
			true    // bCanStrafe
		);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UHSEnemyCombatAI::RotateTowardPlayer(float DeltaTime)
{
	if (!OwnerEnemy || !PlayerChar) return;

	FVector ToPlayer = PlayerChar->GetActorLocation() - OwnerEnemy->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (ToPlayer.IsNearlyZero()) return;

	const FRotator TargetRot(0.f, ToPlayer.Rotation().Yaw, 0.f);
	const FRotator NewRot = FMath::RInterpTo(OwnerEnemy->GetActorRotation(), TargetRot, DeltaTime, RotationInterpSpeed);
	OwnerEnemy->SetActorRotation(NewRot);
}

void UHSEnemyCombatAI::ApplySeparation()
{
	// Don't push during idle or when suspended/staggered -- looks odd and wastes cycles
	if (AIState == EAIState::Idle     ||
		AIState == EAIState::Staggered ||
		AIState == EAIState::Suspended ||
		!OwnerEnemy || !GetWorld()) return;

	const FVector MyLoc = OwnerEnemy->GetActorLocation();

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(OwnerEnemy);

	TArray<AActor*> Nearby;
	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		MyLoc,
		SeparationRadius,
		ObjectTypes,
		AHSDummyEnemy::StaticClass(),
		IgnoreActors,
		Nearby
	);

	for (AActor* Other : Nearby)
	{
		if (!Other || Other == OwnerEnemy) continue;

		FVector PushDir = MyLoc - Other->GetActorLocation();
		PushDir.Z = 0.f;
		if (PushDir.IsNearlyZero()) continue;

		const float Dist = PushDir.Size2D();
		const float Strength = FMath::Clamp(1.f - (Dist / SeparationRadius), 0.f, 1.f) * SeparationStrength;
		OwnerEnemy->AddMovementInput(PushDir.GetSafeNormal(), Strength);
	}
}

void UHSEnemyCombatAI::SetMovementSpeed(float Speed)
{
	if (!OwnerEnemy) return;
	if (UCharacterMovementComponent* Movement = OwnerEnemy->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Speed;
	}
}

void UHSEnemyCombatAI::StopNavMovement()
{
	if (AAIController* AIC = GetAIController())
	{
		AIC->StopMovement();
	}
}

AAIController* UHSEnemyCombatAI::GetAIController() const
{
	if (!OwnerEnemy) return nullptr;
	return Cast<AAIController>(OwnerEnemy->GetController());
}

float UHSEnemyCombatAI::GetDistToPlayer() const
{
	if (!OwnerEnemy || !PlayerChar) return 0.f;
	return FVector::Dist(OwnerEnemy->GetActorLocation(), PlayerChar->GetActorLocation());
}

FVector UHSEnemyCombatAI::GetDirectionToPlayer() const
{
	if (!OwnerEnemy || !PlayerChar) return FVector::ForwardVector;
	return (PlayerChar->GetActorLocation() - OwnerEnemy->GetActorLocation()).GetSafeNormal();
}

void UHSEnemyCombatAI::TryFindPlayer()
{
	if (GetWorld())
	{
		PlayerChar = Cast<AHSPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	}
}

void UHSEnemyCombatAI::ApplyAttackHit()
{
	if (!OwnerEnemy || !PlayerChar || !GetWorld()) return;

	// Simple distance check -- keeps it cheap and predictable
	const float Dist = FVector::Dist(OwnerEnemy->GetActorLocation(), PlayerChar->GetActorLocation());
	if (Dist <= AttackHitRadius)
	{
		PlayerChar->ReceiveEnemyAttack(AttackDamage);
		bAttackHasHit = true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Token management
// ─────────────────────────────────────────────────────────────────────────────

bool UHSEnemyCombatAI::TryAcquireToken()
{
	if (bHoldsToken) return true; // already attacking -- re-use our slot

	if (GActiveAttackers < MaxSimultaneousAttackers)
	{
		++GActiveAttackers;
		bHoldsToken = true;
		return true;
	}
	return false;
}

void UHSEnemyCombatAI::ReleaseTokenIfHeld()
{
	if (bHoldsToken)
	{
		GActiveAttackers = FMath::Max(0, GActiveAttackers - 1);
		bHoldsToken = false;
	}
}
