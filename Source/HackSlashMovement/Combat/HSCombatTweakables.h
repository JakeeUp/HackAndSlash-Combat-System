#pragma once

#include "CoreMinimal.h"
#include "HSCombatTweakables.generated.h"

class UAnimMontage;
class UCameraShakeBase;
class USoundBase;

USTRUCT(BlueprintType)
struct FCombatMontageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TArray<TObjectPtr<UAnimMontage>> LightCombo;

	UPROPERTY(EditDefaultsOnly, meta = (Tooltip = "DmC/DMC3 delayed light combo branch. Indexed parallel to LightCombo. Leave empty to disable."))
	TArray<TObjectPtr<UAnimMontage>> LightDelayedCombo;

	UPROPERTY(EditDefaultsOnly, meta = (Tooltip = "DMC-Stinger/FF16 forward-tilt light combo branch. Indexed parallel to LightCombo. Leave empty to disable."))
	TArray<TObjectPtr<UAnimMontage>> LightForwardCombo;

	UPROPERTY(EditDefaultsOnly)
	TArray<TObjectPtr<UAnimMontage>> HeavyCombo;

	UPROPERTY(EditDefaultsOnly)
	TArray<TObjectPtr<UAnimMontage>> AirCombo;

	UPROPERTY(EditDefaultsOnly, meta = (Tooltip = "Held-light mid-air montage. Expected sections: In -> Loop -> Out."))
	TObjectPtr<UAnimMontage> AirHoldLoop;

	UPROPERTY(EditDefaultsOnly, meta = (Tooltip = "DMC3 High Time / Rising attack montage."))
	TObjectPtr<UAnimMontage> RisingAttack;
};

USTRUCT(BlueprintType)
struct FComboChainConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForwardTiltDotThreshold = 0.5f;

	UPROPERTY(EditDefaultsOnly, meta = (Tooltip = "When true, buffered inputs only fire after the current montage fully ends."))
	bool bStrictFinishBeforeChain = false;

	UPROPERTY(EditDefaultsOnly)
	float StepInForce = 450.f;

	UPROPERTY(EditDefaultsOnly)
	bool bStepInGroundedOnly = true;

	UPROPERTY(EditDefaultsOnly)
	float StepInMaxLockOnRange = 350.f;
};

USTRUCT(BlueprintType)
struct FAirCombatConfig
{
	GENERATED_BODY()

	// -- Air Hold Loop --

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop")
	FName HoldLoopSectionName = TEXT("Loop");

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop")
	FName HoldOutSectionName = TEXT("Out");

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop", meta = (ClampMin = "0.0"))
	float HoldLandBlendOut = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoldPlayerGravityScale = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop", meta = (ClampMin = "0.05"))
	float EnemyHangDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EnemyHangGravity = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Hold Loop", meta = (ClampMin = "0.0"))
	float EnemyHangLift = 60.f;

	// -- Air Combo Gravity --

	UPROPERTY(EditDefaultsOnly, Category = "Gravity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ComboBaseGravity = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity")
	float ComboGravityPerHit = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Gravity")
	float ComboVerticalVelocitySnap = 0.f;
};

USTRUCT(BlueprintType)
struct FCombatTraceConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	float Range = 180.f;

	UPROPERTY(EditDefaultsOnly)
	float Radius = 90.f;

	UPROPERTY(EditDefaultsOnly)
	bool bDebugDraw = false;
};

USTRUCT(BlueprintType)
struct FCombatDamageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	float Light = 8.f;

	UPROPERTY(EditDefaultsOnly)
	float Heavy = 18.f;

	UPROPERTY(EditDefaultsOnly)
	float Air = 10.f;

	UPROPERTY(EditDefaultsOnly)
	float Rising = 20.f;

	UPROPERTY(EditDefaultsOnly)
	float RisingLaunchForce = 1000.f;
};

USTRUCT(BlueprintType)
struct FAttackMagnetConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	bool bEnabled = true;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float SearchRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "180.0"))
	float SearchConeDegrees = 100.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "180.0"))
	float AirSearchConeDegrees = 140.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float IdealDistance = 160.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float MaxSlideDistance = 200.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled"))
	bool bOnlyCloseGaps = true;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float GapCloseTolerance = 30.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float MinDistance = 120.f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled", ClampMin = "0.01"))
	float SlideDuration = 0.07f;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bEnabled"))
	bool bHorizontalOnlyInAir = true;
};

USTRUCT(BlueprintType)
struct FCombatHitFeedbackConfig
{
	GENERATED_BODY()

	// -- Camera Shake --

	UPROPERTY(EditDefaultsOnly, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditDefaultsOnly, Category = "Camera Shake")
	float LightShakeScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera Shake")
	float HeavyShakeScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera Shake")
	float AirShakeScale = 0.6f;

	// -- FOV Compression --

	UPROPERTY(EditDefaultsOnly, Category = "FOV", meta = (ClampMin = "0.0"))
	float FOVCompressionPerHit = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "FOV", meta = (ClampMin = "0.0"))
	float MaxFOVCompression = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "FOV", meta = (ClampMin = "0.1"))
	float FOVRecoverySpeed = 3.f;

	// -- Screen Effects --

	UPROPERTY(EditDefaultsOnly, Category = "Screen Effects")
	float FlashIntensity = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Screen Effects")
	float FlashDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Screen Effects", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TimeDilationScale = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Screen Effects")
	float TimeDilationDuration = 0.08f;
};

USTRUCT(BlueprintType)
struct FCombatSFXConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Swing")
	TObjectPtr<USoundBase> LightSwing;

	UPROPERTY(EditDefaultsOnly, Category = "Swing")
	TObjectPtr<USoundBase> HeavySwing;

	UPROPERTY(EditDefaultsOnly, Category = "Swing")
	TObjectPtr<USoundBase> AirSwing;

	UPROPERTY(EditDefaultsOnly, Category = "Swing")
	TObjectPtr<USoundBase> RisingSwing;

	UPROPERTY(EditDefaultsOnly, Category = "Hit")
	TObjectPtr<USoundBase> LightHit;

	UPROPERTY(EditDefaultsOnly, Category = "Hit")
	TObjectPtr<USoundBase> HeavyHit;

	UPROPERTY(EditDefaultsOnly, Category = "Hit")
	TObjectPtr<USoundBase> AirHit;

	UPROPERTY(EditDefaultsOnly, Category = "Hit")
	TObjectPtr<USoundBase> RisingHit;
};

USTRUCT(BlueprintType)
struct FHSCombatTweakables
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	FCombatMontageConfig Montages;

	UPROPERTY(EditDefaultsOnly, Category = "Combo Chain")
	FComboChainConfig ComboChain;

	UPROPERTY(EditDefaultsOnly, Category = "Trace")
	FCombatTraceConfig Trace;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FCombatDamageConfig Damage;

	UPROPERTY(EditDefaultsOnly, Category = "Air Combat", meta = (InlineEditConditionToggle))
	bool bEnableAirCombat = true;

	UPROPERTY(EditDefaultsOnly, Category = "Air Combat", meta = (EditCondition = "bEnableAirCombat"))
	FAirCombatConfig AirCombat;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Effects", meta = (InlineEditConditionToggle))
	bool bEnableSFX = true;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Effects", meta = (EditCondition = "bEnableSFX"))
	FCombatSFXConfig SFX;
};
