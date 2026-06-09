#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSCombatTweakables.h"
#include "HSCombatComponent.generated.h"


class AHSPlayerCharacter;


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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** Drives the held-light mid-air loop.  Called every frame by the player character Tick
	 *  with the current held/airborne state.  Handles start, auto-replay while the built-in
	 *  montage isn't looping, and blend-out when released or grounded.  Safe to call when
	 *  AirHoldLoopMontage is unset -- early-outs. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void UpdateAirHoldLoop(bool bButtonHeld, bool bFalling);

	UFUNCTION(BlueprintPure, Category = "Combat")
	FORCEINLINE bool IsAttacking() const { return bIsAttacking; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FORCEINLINE bool IsAirHoldActive() const { return bAirHoldActive; }

	/** Called when the character lands -- restores gravity and resets air hit tracking. */
	void OnOwnerLanded();

	/*****************************************************/
	/*               Anim Notify Callbacks               */
	/*****************************************************/
	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void OpenComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void CloseComboWindow();

	/** Opens the DELAYED-input branch window.  Called by ANS_DelayedComboWindow.
	 *  Place this AFTER the regular combo window on the timeline so a late light press
	 *  here branches into LightDelayedComboMontages instead of the normal chain. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void OpenDelayedComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void CloseDelayedComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void OnAttackFinished();

	UFUNCTION(BlueprintCallable, Category = "Combat|Notifies")
	void DoSwordTrace();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FHSCombatTweakables Tweakables;

	/*****************************************************/
	/*                        State                      */
	/*****************************************************/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	class AHSPlayerCharacter* OwnerChar;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsAttacking = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bComboWindowOpen = false;

	/** True while the DELAYED-input notify state is active on the current swing.
	 *  A light press while this is true (and the regular combo window has closed)
	 *  immediately branches into LightDelayedComboMontages. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bDelayedWindowOpen = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bSavedNextAttack = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EAttackType CurrentAttackType = EAttackType::EAT_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EAttackType BufferedAttackType = EAttackType::EAT_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	int32 ComboIndex = 0;

	/** True when the player entered the delayed-input branch this combo string.  Cleared by ResetCombo.
	 *  While set, GetMontageForCombo pulls from LightDelayedComboMontages instead of LightComboMontages. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bOnDelayedBranch = false;

	/** True when the player's initial light-press was tilted forward (toward target / camera-forward).
	 *  Locks the current combo string to LightForwardComboMontages for its duration.  Cleared by
	 *  ResetCombo.  Takes a back seat to bOnDelayedBranch -- a player who ends up on both branches
	 *  in the same string gets delayed-branch priority (rarer, input-specific timing). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bOnForwardBranch = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bAirComboActive = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	int32 AirHitCount = 0;

	/** True while the held-light mid-air loop montage is active. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bAirHoldActive = false;

	/** True once the player has released the button while airborne -- the Loop section has been
	 *  rewired to transition into the Out section, but we're still technically "active" until
	 *  Out finishes playing.  Lets a re-press mid-Out cancel the exit and jump back to looping. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bAirHoldStopRequested = false;

	/** Authoritative baseline gravity scale, captured ONCE in BeginPlay from the CMC's CDO value
	 *  before any combat system can override it.  All gravity-restore paths set the CMC back to
	 *  this value -- never to a "saved before override" snapshot, which used to cause a leak
	 *  when air-combo + hold-loop stacked their overrides (each system saved the other's modified
	 *  value as the "original", and gravity would stay low forever after the chain unwound). */
	float DefaultGravityScale = 1.f;

	/** True while the hold-loop is currently driving the player's gravity scale.  Gates restore
	 *  so other systems (or a second hold-loop re-entry) don't double-save or clobber. */
	bool bHoldGravityApplied = false;

private:
	void PlayNextAttack(EAttackType Type);
	UAnimMontage* GetMontageForCombo(EAttackType Type, int32 Index) const;
	float GetDamageForCurrentAttack() const;
	void ResetCombo();

	/** Snap the owning character toward camera-relative input direction before each swing. */
	void RotateOwnerToInput();

	/** Returns true if the player's cached move input is tilted "forward" -- toward the locked
	 *  target if one exists, otherwise along camera-forward.  Used at the start of a light combo
	 *  to pick between LightComboMontages and LightForwardComboMontages. */
	bool IsInputTiltForward() const;

	/** Play the swing sound for the current attack type. */
	void PlaySwingSound();

	/** Play the hit sound for the current attack type. */
	void PlayHitSound();

	/** Force-restore the owner's GravityScale to DefaultGravityScale and clear any gravity-override
	 *  tracking flags (bAirComboActive, bHoldGravityApplied).  Safe to call unconditionally --
	 *  no-ops if nothing was overridden.  This is the single choke point for ALL gravity restore
	 *  paths (land, release, combo end, attack cancel, component EndPlay) so the two override
	 *  systems can't leak their state into each other. */
	void RestoreGravityIfOverridden();

};
