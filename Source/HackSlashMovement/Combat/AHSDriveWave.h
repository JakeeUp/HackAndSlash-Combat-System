// Ground-travelling energy wave spawned by AN_DriveWave (Dante "Drive" style).
// Travels forward, pierces through every enemy it touches, then disappears.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/HSDamageable.h"
#include "AHSDriveWave.generated.h"


class UBoxComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class AHSPlayerCharacter;


UCLASS()
class HACKSLASHMOVEMENT_API AHSDriveWave : public AActor
{
	GENERATED_BODY()

public:
	AHSDriveWave();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** Called by AN_DriveWave immediately after spawning to arm the wave. */
	void Launch(AHSPlayerCharacter* InOwner, float InDamage, EHitWeight InHitWeight);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> HitBox;

	/** Optional looping VFX while the wave is in flight.
	 *  Assign a Niagara system in BP_DriveWave. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> WaveVFXComponent;

	/*****************************************************/
	/*               Configurations                      */
	/*****************************************************/

	/** How fast the wave travels (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "100.0"))
	float TravelSpeed = 2200.f;

	/** Maximum distance the wave travels before disappearing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "100.0"))
	float MaxRange = 2500.f;

	/** Half-extent of the hit box along the forward axis (depth of the wave). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "1.0"))
	float HitDepth = 60.f;

	/** Half-extent of the hit box along the right axis (width of the wave).
	 *  Match this roughly to the sword swing arc. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "1.0"))
	float HitWidth = 120.f;

	/** Half-extent of the hit box vertically.  Tall enough to catch standing enemies. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "1.0"))
	float HitHeight = 90.f;

	/** Niagara system to spawn at the impact point when the wave hits an enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations")
	TObjectPtr<UNiagaraSystem> ImpactVFX;

	/** Sound played at each enemy hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations")
	TObjectPtr<USoundBase> ImpactSound;

	/** Sound played when the wave is launched (at the player's feet). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations")
	TObjectPtr<USoundBase> LaunchSound;

private:
	float TraveledDistance = 0.f;
	float CachedDamage = 0.f;
	EHitWeight CachedHitWeight = EHitWeight::EHW_Heavy;

	/** Enemies already struck this wave -- prevents the same enemy taking multiple hits. */
	TArray<TWeakObjectPtr<AActor>> HitActors;

	UPROPERTY()
	TObjectPtr<AHSPlayerCharacter> OwnerPlayer;

	void CheckOverlaps();
};
