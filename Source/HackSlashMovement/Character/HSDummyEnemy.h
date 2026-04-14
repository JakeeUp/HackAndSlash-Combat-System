// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/HSDamageable.h"
#include "HSDummyEnemy.generated.h"


class UAnimMontage;
class AHSDamageNumber;
class UNiagaraSystem;


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
	// Sets default values for this character's properties
	AHSDummyEnemy();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE EEnemyState GetEnemyState() const { return EnemyState; }

	//IHSDamageable
	virtual void ApplyDamage_Implementation(float DamageAmount, AActor* DamageCauser) override;
	virtual void ApplyDamageWithInfo_Implementation(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, bool bIsHeavyHit) override;
	virtual void ApplyDamageEx_Implementation(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight) override;

	UFUNCTION(BlueprintPure, Category = "State")
	float GetHealthPercent() const { return (MaxHealth > 0.f) ? (CurrentHealth / MaxHealth) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsDead() const { return bIsDead; }

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

	/** Fallback single montage (backwards compat). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* HitReactMontage;

	/** Hit react when the enemy is airborne (juggled). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* AirHitReactMontage;

	/** Hit react when the enemy is lying on the ground. Picks randomly. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	TArray<UAnimMontage*> DownHitReactMontages;

	/** Montage for getting up after being knocked down. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* GetupMontage;

	/** Montage played when the enemy hits the ground after being airborne. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* HitDropMontage;

	/** End/settle montage after the drop impact (plays between drop and getup). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Hit React")
	UAnimMontage* HitDropEndMontage;

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

	/** How far the enemy gets pushed back on a light hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float LightKnockbackForce = 400.f;

	/** How far the enemy gets pushed back on a heavy hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float HeavyKnockbackForce = 800.f;

	/** Vertical lift on heavy knockback (DMC launcher feel). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Knockback")
	float HeavyKnockbackLift = 200.f;

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
	/*                        State                      */
	/*****************************************************/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EEnemyState EnemyState = EEnemyState::EES_Idle;

	/** True when the enemy was launched by a Launcher/Finisher (triggers HitDrop+Getup on landing). */
	bool bWasLaunched = false;

private:
	void Die();
	void HandleHitReaction(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight);
	void SpawnHitVFX(const FVector& HitDirection, bool bIsProjectile);
	void ApplyKnockback(const FVector& HitDirection, EHitWeight HitWeight, bool bIsProjectile);
	void ApplyHitstop(EHitWeight HitWeight);
	void EndHitstop();
	void PlayGetup();

	FTimerHandle HitstopTimerHandle;
	FTimerHandle DropEndTimerHandle;
	FTimerHandle GetupTimerHandle;
};
