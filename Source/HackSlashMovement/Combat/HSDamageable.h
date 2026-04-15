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

class HACKSLASHMOVEMENT_API IHSDamageable
{
	GENERATED_BODY()

public:
	/** Basic damage with no directional info. Used by projectiles. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void ApplyDamage(float DamageAmount, AActor* DamageCauser);

	/** Full damage with direction and hit weight for knockback, VFX, and hit reacts. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void ApplyDamageEx(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight);
};
