#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/HSDamageable.h"
#include "HSDummyEnemy.generated.h"


class UAnimMontage;
class AHSDamageNumber;
class UNiagaraSystem;
class USoundBase;


UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	EES_Idle      UMETA(DisplayName = "Idle"),
	EES_Airborne  UMETA(DisplayName = "Airborne"),
	EES_Down      UMETA(DisplayName = "Down"),
};


UCLASS()
class HACKSLASHMOVEMENT_API AHSDummyEnemy : public ACharacter, public IHSDamageable
{
	GENERATED_BODY()

public:
	AHSDummyEnemy();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE EEnemyState GetEnemyState() const { return EnemyState; }

	// IHSDamageable
	virtual void ApplyDamage_Implementation(float DamageAmount, AActor* DamageCauser) override;
	virtual void ApplyDamageEx_Implementation(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight) override;

	UFUNCTION(BlueprintPure, Category = "State")
	float GetHealthPercent() const { return (MaxHealth > 0.f) ? (CurrentHealth / MaxHealth) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE bool IsInDropLoop() const { return bInDropLoop; }

	/** True for the single frame/tick immediately after a launcher hit lands.
	 *  The ABP reads this to instantly transition to the air hit-react state
	 *  without waiting for EnemyState to propagate. Cleared once StartDropLoop fires. */
	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE bool IsJustLaunched() const { return bJustLaunched; }

protected:
	/*****************************************************/
	/*                    Configurations                 */
	/*****************************************************/
	UPROPERTY(EditDefaultsOnly, Category = "Configurations", meta = (ClampMin = "1"))
	float MaxHealth = 500.f;

	/** Light hit react montages -- picks randomly for variety. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	TArray<UAnimMontage*> LightHitReactMontages;

	/** Heavy hit react montage (bigger stagger). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* HeavyHitReactMontage;

	/** Hit react montages when the enemy is airborne (juggled). Picks randomly for variety. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	TArray<UAnimMontage*> AirHitReactMontages;

	/** Hit react when the enemy is lying on the ground. Picks randomly. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	TArray<UAnimMontage*> DownHitReactMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* DeathMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	float DeathCleanupDelay = 5.f;

	/** Play rate for hit react montages (faster = snappier, DMC feel). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	float HitReactPlayRate = 1.3f;

	/** Delay before the enemy plays getup after landing from airborne. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	float GetupDelay = 0.5f;

	/*****************************************************/
	/*                    Knockback                      */
	/*****************************************************/

	/** How far the enemy gets pushed back on a light hit (tiny stagger, mid-combo). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float LightKnockbackForce = 120.f;

	/** How far the enemy gets pushed back on a heavy hit (used on last light-combo swing). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float HeavyKnockbackForce = 450.f;

	/** Vertical lift on heavy knockback. 0 = horizontal-only stagger; raise for a DMC "pop up" feel. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float HeavyKnockbackLift = 0.f;

	/** Vertical launch force when hit by a launcher attack (Rising / combo_03_02). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float LaunchUpForce = 900.f;

	/** Force applied when sent flying by a finisher (combo_03_04). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float SendFlyingForce = 1200.f;

	/** Vertical lift on send-flying hits. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float SendFlyingLift = 400.f;

	/** Small upward push applied on every hit while airborne to keep the enemy juggled (DMC style). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float AirJuggleLift = 250.f;

	/** Knockback from projectile hits. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float ProjectileKnockbackForce = 300.f;

	/*****************************************************/
	/*                    Hit VFX                        */
	/*****************************************************/

	/** Particle system to spawn at the hit location for sword impacts. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|VFX")
	UNiagaraSystem* SwordHitVFX;

	/** Particle system for projectile impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|VFX")
	UNiagaraSystem* ProjectileHitVFX;

	/** Scale for hit VFX. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|VFX")
	FVector HitVFXScale = FVector(1.f);

	/*****************************************************/
	/*                    Hitstop                        */
	/*****************************************************/

	/** Brief pause on the enemy mesh when hit (DMC/FF16 hitstop). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hitstop")
	float LightHitstopDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hitstop")
	float HeavyHitstopDuration = 0.1f;

	/*****************************************************/
	/*                  Damage Numbers                   */
	/*****************************************************/

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Damage Numbers")
	TSubclassOf<AHSDamageNumber> DamageNumberClass;

	/** Offset above the enemy where damage numbers spawn. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Damage Numbers")
	FVector DamageNumberOffset = FVector(0.f, 0.f, 100.f);

	/*****************************************************/
	/*                    Hit SFX                        */
	/*****************************************************/

	/** Sound played when the enemy takes a light hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* LightHitSound;

	/** Sound played when the enemy takes a heavy hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* HeavyHitSound;

	/** Sound played when the enemy is hit by a projectile (Q ability). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* ProjectileHitSound;

	/** Sound played when the enemy gets launched into the air. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* LaunchSound;

	/** Sound when the enemy hits the ground after being airborne. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* LandImpactSound;

	/** Volume multiplier applied to all enemy SFX (hit, launch, land). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX", meta = (ClampMin = "0.0"))
	float SFXVolumeMultiplier = 1.f;

	/*****************************************************/
	/*                        State                      */
	/*****************************************************/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EEnemyState EnemyState = EEnemyState::EES_Idle;

	/** True while the hit drop loop montage is playing (enemy falling after air combo). */
	bool bInDropLoop = false;

	/** Pulsed true when a launcher hit lands -- clears once StartDropLoop fires.
	 *  One-shot signal for the ABP to jump to the air hit-react state immediately. */
	bool bJustLaunched = false;

private:
	void Die();
	void HandleHitReaction(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight);
	void SpawnHitVFX(const FVector& HitDirection, bool bIsProjectile);
	void ApplyKnockback(const FVector& HitDirection, EHitWeight HitWeight, bool bIsProjectile);
	void ApplyHitstop(EHitWeight HitWeight);
	void EndHitstop();

	/** Start the looping fall animation after an air hit react finishes. */
	void StartDropLoop();

	/** Called when an air hit react montage ends -- sets drop loop flag if still airborne. */
	void OnAirHitReactEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Timer callback -- sets state back to Idle after landing. */
	void PlayGetup();

	FTimerHandle HitstopTimerHandle;
	FTimerHandle GetupTimerHandle;
};
