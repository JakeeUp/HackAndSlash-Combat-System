// XP orb -- explodes outward on enemy death with simulated arc, hovers, then vacuums to player.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSXPOrb.generated.h"


class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class AHSPlayerCharacter;


UCLASS()
class HACKSLASHMOVEMENT_API AHSXPOrb : public AActor
{
	GENERATED_BODY()

public:
	AHSXPOrb();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** Called immediately after SpawnActor to set the XP value and outward burst direction.
	 *  Pass ZeroVector for a fully random scatter direction. */
	void Initialize(float InXPValue, const FVector& BurstDirection);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	/*****************************************************/
	/*                     Burst Phase                   */
	/*****************************************************/

	/** Horizontal launch speed on burst (cm/s).  Randomised ±30% per orb. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Burst", meta = (ClampMin = "0.0"))
	float BurstSpeed = 520.f;

	/** Upward launch velocity on burst (cm/s).  Randomised ±30% per orb.
	 *  Higher = more air time, taller arc. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Burst", meta = (ClampMin = "0.0"))
	float BurstUpForce = 750.f;

	/** Simulated gravity pulling the orb back down during the arc (cm/s²).
	 *  Keep lower than real UE gravity (980) so orbs hang in the air longer. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Burst", meta = (ClampMin = "0.0"))
	float BurstGravity = 700.f;

	/** Max seconds the burst arc runs before the orb stops and starts hovering.
	 *  Acts as a safety net -- the arc will naturally slow anyway. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Burst", meta = (ClampMin = "0.1"))
	float BurstDuration = 0.7f;

	/** Random angular spread applied to each orb's burst direction (degrees).
	 *  Breaks up the uniform fan so no two kills look the same. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Burst", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float BurstSpread = 20.f;

	/*****************************************************/
	/*                    Hover Phase                    */
	/*****************************************************/

	/** Amplitude of the idle bob while the orb waits to be collected (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hover", meta = (ClampMin = "0.0"))
	float HoverBobAmplitude = 8.f;

	/** Speed of the idle bob cycle. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hover", meta = (ClampMin = "0.1"))
	float HoverBobSpeed = 4.f;

	/*****************************************************/
	/*                  Attraction Phase                 */
	/*****************************************************/

	/** Seconds after spawn before the orb starts vacuuming toward the player.
	 *  Set slightly longer than BurstDuration so the orb settles before being pulled. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Attraction", meta = (ClampMin = "0.0"))
	float AttractionDelay = 0.85f;

	/** Starting attraction speed (cm/s).  Ramps up to AttractionSpeed × 8 over time. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Attraction", meta = (ClampMin = "100.0"))
	float AttractionSpeed = 700.f;

	/** Interp speed for the attraction ramp.  Higher = snappier vacuum snap. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Attraction", meta = (ClampMin = "0.5"))
	float AttractionAcceleration = 4.f;

	/** If the player gets within this distance the orb attracts immediately, ignoring the delay. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Attraction", meta = (ClampMin = "0.0"))
	float AutoPickupRange = 200.f;

	/** Orb is collected when it reaches within this distance of the player. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Attraction", meta = (ClampMin = "0.0"))
	float CollectRange = 80.f;

	/*****************************************************/
	/*                      Lifetime                     */
	/*****************************************************/

	/** Orb self-destructs after this many seconds if never collected. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "1.0"))
	float OrbLifeSpan = 14.f;

	/** Sound played at the orb's location on collection. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations")
	TObjectPtr<USoundBase> PickupSound;

	/** Optional Niagara burst at the collection point. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations")
	TObjectPtr<UNiagaraSystem> PickupVFX;

private:
	float XPValue = 0.f;

	// ── Burst state ──────────────────────────────────────────────────────────
	bool  bBursting = false;
	float BurstTimer = 0.f;
	FVector BurstVelocity = FVector::ZeroVector;

	// ── Hover state ──────────────────────────────────────────────────────────
	float HoverBaseZ  = 0.f;     // world Z when hover started
	float HoverTime   = 0.f;     // accumulated time for sin bob

	// ── Attraction state ──────────────────────────────────────────────────────
	bool  bAttracting = false;
	float CurrentAttractionSpeed = 0.f;

	UPROPERTY()
	TObjectPtr<AHSPlayerCharacter> CachedPlayer;

	FTimerHandle AttractionTimerHandle;

	void StartAttracting();
	void Collect();
};
