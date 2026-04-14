// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HSDamageable.generated.h"


UENUM(BlueprintType)
enum class EHitWeight : uint8
{
	EHW_Light     UMETA(DisplayName = "Light"),
	EHW_Heavy     UMETA(DisplayName = "Heavy"),
	EHW_Launcher  UMETA(DisplayName = "Launcher"),
	EHW_Finisher  UMETA(DisplayName = "Finisher"),
};


UINTERFACE(MinimalAPI, BlueprintType)
class UHSDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 *
 */
class HACKSLASHMOVEMENT_API IHSDamageable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void ApplyDamage(float DamageAmount, AActor* DamageCauser);

	/** Extended version with hit direction and attack info for knockback/VFX. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void ApplyDamageWithInfo(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, bool bIsHeavyHit);

	/** Full version with hit weight enum for launchers, finishers, etc. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void ApplyDamageEx(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight);
};
