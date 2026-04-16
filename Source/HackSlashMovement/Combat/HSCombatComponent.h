#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSCombatComponent.generated.h"


class UAnimMontage;
class AHSPlayerCharacter;
class UCameraShakeBase;
class USoundBase;


UENUM(BlueprintType)
enum class EAttackType : uint8
{
	EAT_None    UMETA(DisplayName = "None"),
	EAT_Light   UMETA(DisplayName = "Light"),
	EAT_Heavy   UMETA(DisplayName = "Heavy"),
	EAT_Air     UMETA(DisplayName = "Air"),
	EAT_Rising  UMETA(DisplayName = "Rising")
};


UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HACKSLASHMOVEMENT_API UHSCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSCombatComponent();

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryLightAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryHeavyAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryAirAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryRisingAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CancelAttack();

	UFUNCTION(BlueprintPure, Category = "Combat")
	FORCEINLINE bool IsAttacking() const { return bIsAttacking; }

	/** Called when the character lands -- restores gravity and resets air hit tracking. */
	void OnOwnerLanded();

	/*****************************************************/
	/*               Anim Notify Callbacks               */
	/*****************************************************/
	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void OpenComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void CloseComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void OnAttackFinished();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void DoSwordTrace();

protected:
	/*****************************************************/
	/*                    Configurations                 */
	/*****************************************************/
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Montages")
	TArray<UAnimMontage*> LightComboMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Montages")
	TArray<UAnimMontage*> HeavyComboMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Montages")
	TArray<UAnimMontage*> AirComboMontages;

	/** DMC3 High Time / Rising attack montage (back+attack while locked on). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Montages")
	UAnimMontage* RisingAttackMontage;

	/** How high the player launches on a rising attack. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Rising Attack")
	float RisingLaunchForce = 1000.f;

	/** Damage for the rising attack. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Rising Attack")
	float RisingDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Trace")
	float TraceRange = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Trace")
	float TraceRadius = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Damage")
	float LightDamage = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Damage")
	float HeavyDamage = 18.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Damage")
	float AirDamage = 10.f;

	/** Gravity scale for the first air hit. Near-zero = full hang like DMC. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Air Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirComboBaseGravity = 0.05f;

	/** Extra gravity added per subsequent air hit. DMC3 uses ~0.15 so by hit 4 you're noticeably sinking. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Air Combat")
	float AirComboGravityPerHit = 0.15f;

	/** Vertical velocity is snapped to this when an air attack starts so the character doesn't keep rising or falling. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Air Combat")
	float AirComboVerticalVelocitySnap = 0.f;

	/** Camera shake played when a melee attack connects. Light hits use Scale 0.5, heavy hits use 1.0. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Camera Shake")
	TSubclassOf<UCameraShakeBase> HitCameraShake;

	/** Scale for light attack camera shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Camera Shake")
	float LightHitShakeScale = 0.5f;

	/** Scale for heavy attack camera shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Camera Shake")
	float HeavyHitShakeScale = 1.0f;

	/** Scale for air attack camera shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Camera Shake")
	float AirHitShakeScale = 0.6f;

	/*****************************************************/
	/*                    Sound Effects                  */
	/*****************************************************/

	/** Sword swing whoosh for light attacks. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* LightSwingSound;

	/** Sword swing whoosh for heavy attacks. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* HeavySwingSound;

	/** Sword swing for air attacks. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* AirSwingSound;

	/** Rising attack swing sound. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* RisingSwingSound;

	/** Impact sound when a light attack connects. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* LightHitSound;

	/** Impact sound when a heavy attack connects. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* HeavyHitSound;

	/** Impact sound for air hits. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* AirHitSound;

	/** Impact sound for the rising/launcher attack. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|SFX")
	USoundBase* RisingHitSound;

	/*****************************************************/
	/*               Screen Hit Effects                  */
	/*****************************************************/

	/** Brief white screen flash on heavy/rising hits (FF16 impact feel). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Screen Effects")
	float HeavyHitFlashIntensity = 0.3f;

	/** How fast the screen flash fades out. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Screen Effects")
	float HeavyHitFlashDuration = 0.12f;

	/** Time dilation applied on launcher/finisher hits for dramatic impact. 0.1 = near freeze. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Screen Effects", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HitTimeDilationScale = 0.15f;

	/** How long the time dilation lasts (real seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Screen Effects")
	float HitTimeDilationDuration = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Debug")
	bool bDebugDrawTrace = true;

	/** When true, buffered inputs only fire after the current montage fully ends (OnAttackFinished).
	 *  When false (DMC default), buffered inputs fire as soon as OpenComboWindow is hit by an anim notify. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combo")
	bool bStrictFinishBeforeChain = false;

	/** Forward nudge applied to the player at the start of each swing so combos stay in range of the enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combo")
	float AttackStepInForce = 450.f;

	/** Step-in force only applies while grounded (avoids turning air combos into forward dives). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combo")
	bool bStepInGroundedOnly = true;

	/** Max distance at which step-in still applies when locked on. Prevents teleporting through a far target. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combo")
	float StepInMaxLockOnRange = 350.f;

	/*****************************************************/
	/*                        State                      */
	/*****************************************************/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	class AHSPlayerCharacter* OwnerChar;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsAttacking = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bComboWindowOpen = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bSavedNextAttack = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EAttackType CurrentAttackType = EAttackType::EAT_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EAttackType BufferedAttackType = EAttackType::EAT_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	int32 ComboIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bAirComboActive = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	int32 AirHitCount = 0;

	float SavedGravityScale = 1.f;

private:
	void PlayNextAttack(EAttackType Type);
	UAnimMontage* GetMontageForCombo(EAttackType Type, int32 Index) const;
	float GetDamageForCurrentAttack() const;
	void ResetCombo();

	/** Snap the owning character toward camera-relative input direction before each swing. */
	void RotateOwnerToInput();

	/** Play the swing sound for the current attack type. */
	void PlaySwingSound();

	/** Play the hit sound for the current attack type. */
	void PlayHitSound();

	/** Screen flash + time dilation for heavy/rising hits. */
	void ApplyScreenHitEffect();

	/** Restore time dilation after hit freeze. */
	void RestoreTimeDilation();

	FTimerHandle TimeDilationHandle;
};
