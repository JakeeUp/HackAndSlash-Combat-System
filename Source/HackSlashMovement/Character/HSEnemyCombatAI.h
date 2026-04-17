#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/HSDamageable.h"
#include "HSEnemyCombatAI.generated.h"


class AHSDummyEnemy;
class AHSPlayerCharacter;
class UAnimMontage;


/** DMC/FF16-style enemy AI states. */
UENUM(BlueprintType)
enum class EAIState : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	Strafe     UMETA(DisplayName = "Strafe"),
	Approach   UMETA(DisplayName = "Approach"),
	Windup     UMETA(DisplayName = "Windup"),      // telegraph before attack
	Attacking  UMETA(DisplayName = "Attacking"),
	Retreat    UMETA(DisplayName = "Retreat"),
	Staggered  UMETA(DisplayName = "Staggered"),
	Suspended  UMETA(DisplayName = "Suspended"),   // airborne or downed by player
};


/**
 * DMC / FF16 enemy combat AI.
 *
 * Attach this to AHSDummyEnemy to get:
 *  - Attack-token system (configurable MaxSimultaneousAttackers) so enemies
 *    can't all lunge at once -- they queue and take turns.
 *  - Strafe orbit → approach → telegraph windup → lunge attack → retreat loop.
 *  - Crowd separation so enemies don't pile on top of each other.
 *  - Full suspension when the enemy is launched or downed by the player.
 *  - All timings tunable in the Blueprint defaults panel.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HACKSLASHMOVEMENT_API UHSEnemyCombatAI : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSEnemyCombatAI();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called by HSDummyEnemy::HandleHitReaction so the AI can react to being hit. */
	void NotifyHit(EHitWeight HitWeight);

	UFUNCTION(BlueprintPure, Category = "AI")
	FORCEINLINE EAIState GetAIState() const { return AIState; }

protected:
	/*****************************************************/
	/*                    Token System                   */
	/*****************************************************/

	/** Max enemies that may be in an active attack at the same time (global across all instances).
	 *  Keep it low (1-2) for the DMC feel where enemies politely queue. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Token", meta = (ClampMin = "1"))
	int32 MaxSimultaneousAttackers = 2;

	/*****************************************************/
	/*                      Timing                       */
	/*****************************************************/

	/** Min/max wait time in Idle before the enemy starts strafing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float IdleTimeMin = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float IdleTimeMax = 1.2f;

	/** How often (seconds) the enemy rolls to attempt an attack while strafing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.1"))
	float AttackDecisionInterval = 1.0f;

	/** Probability [0..1] of attempting to attack each decision interval. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackTriggerChance = 0.5f;

	/** How long the enemy stands still telegraphing before the actual lunge. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float WindupDuration = 0.45f;

	/** If the enemy spends longer than this in Approach without reaching the player,
	 *  it gives up, releases the token, and returns to Strafe.  Prevents permanent
	 *  stuck-in-approach if the NavMesh can't connect to the player. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "1.0"))
	float ApproachTimeout = 6.f;

	/** Seconds after entering Attacking state before the hit check fires. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float AttackDamageDelay = 0.18f;

	/** Total duration of the Attacking state -- the attack montage's length is used
	 *  when available; this is the fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.1"))
	float AttackStateDuration = 0.75f;

	/** How long the enemy backs away after an attack before re-entering Strafe. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float RetreatDuration = 1.0f;

	/** How often the enemy randomly reverses its strafe direction. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.5"))
	float StrafeDirectionChangePeriod = 2.5f;

	/** Stagger duration for a light hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float LightStaggerDuration = 0.25f;

	/** Stagger duration for a heavy hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Timing", meta = (ClampMin = "0.0"))
	float HeavyStaggerDuration = 0.55f;

	/*****************************************************/
	/*                     Movement                      */
	/*****************************************************/

	/** Preferred orbit distance from the player while strafing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float StrafeRadius = 500.f;

	/** Tolerance band around StrafeRadius before radial correction kicks in. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float StrafeRadiusTolerance = 80.f;

	/** Distance to the player at which Approach → Windup. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float AttackStartRange = 200.f;

	/** Beyond this range the enemy hard-approaches the player instead of strafing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float MaxEngagementRange = 2500.f;

	/** Walk speed while strafing/orbiting. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float StrafeSpeed = 250.f;

	/** Walk speed while approaching. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float ApproachSpeed = 450.f;

	/** Walk speed while retreating. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float RetreatSpeed = 280.f;

	/** Horizontal lunge impulse at the start of the attack. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float AttackLungeForce = 700.f;

	/** Minimum distance between this enemy and others before separation pushes them apart. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float SeparationRadius = 130.f;

	/** Strength of the AddMovementInput call used for separation (0..1 fraction of MaxWalkSpeed). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationStrength = 0.4f;

	/** Speed at which the enemy rotates to face the player. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 8.f;

	/*****************************************************/
	/*                      Combat                       */
	/*****************************************************/

	/** Attack animations -- one is chosen at random per attack. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combat")
	TArray<UAnimMontage*> AttackMontages;

	/** Damage applied to the player when the attack hits. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 20.f;

	/** Radius (from enemy centre) for the sphere hit-check during the attack damage window. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combat", meta = (ClampMin = "0.0"))
	float AttackHitRadius = 180.f;

private:
	EAIState AIState = EAIState::Idle;

	UPROPERTY()
	AHSDummyEnemy* OwnerEnemy = nullptr;

	UPROPERTY()
	AHSPlayerCharacter* PlayerChar = nullptr;

	/** General-purpose countdown timer shared across most states. */
	float StateTimer = 0.f;

	/** Counts down to the next attack-decision roll while Strafing. */
	float AttackDecisionTimer = 0.f;

	/** Counts down to the next strafe direction flip. */
	float StrafeDirectionTimer = 0.f;

	/** Counts down the delay before the attack damage check fires. */
	float AttackDamageTimer = 0.f;

	/** Counts down while in Approach.  Triggers a give-up if the enemy can't reach the player. */
	float ApproachTimeoutTimer = 0.f;

	bool bStrafeClockwise = true;
	bool bHoldsToken = false;
	bool bAttackHasHit = false;

	// ── Token management ──────────────────────────────────────────────────────
	bool TryAcquireToken();
	void ReleaseTokenIfHeld();

	// ── State transitions ─────────────────────────────────────────────────────
	void TransitionToIdle();
	void TransitionToStrafe();
	void TransitionToApproach();
	void TransitionToWindup();
	void TransitionToAttacking();
	void TransitionToRetreat();

	// ── Per-state tick handlers ───────────────────────────────────────────────
	void TickIdle(float DeltaTime);
	void TickStrafe(float DeltaTime);
	void TickApproach(float DeltaTime);
	void TickWindup(float DeltaTime);
	void TickAttacking(float DeltaTime);
	void TickRetreat(float DeltaTime);
	void TickStaggered(float DeltaTime);

	// ── Helpers ────────────────────────────────────────────────────────────────
	void RotateTowardPlayer(float DeltaTime);
	void ApplySeparation();
	void SetMovementSpeed(float Speed);
	void StopNavMovement();
	void TryFindPlayer();
	void ApplyAttackHit();

	class AAIController* GetAIController() const;

	float GetDistToPlayer() const;
	FVector GetDirectionToPlayer() const;
};
