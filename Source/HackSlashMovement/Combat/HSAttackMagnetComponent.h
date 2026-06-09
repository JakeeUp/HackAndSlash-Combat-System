#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSCombatTweakables.h"
#include "HSAttackMagnetComponent.generated.h"

class AHSPlayerCharacter;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HACKSLASHMOVEMENT_API UHSAttackMagnetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSAttackMagnetComponent();

protected:
	virtual void BeginPlay() override;

public:
	bool TryStartSlide();
	void UpdateSlide(float DeltaTime, bool bOwnerAttacking);
	void EndSlide();

	UFUNCTION(BlueprintPure, Category = "Combat|Magnet")
	FORCEINLINE bool IsSliding() const { return bSliding; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Attack Magnet")
	FAttackMagnetConfig Config;

private:
	AActor* FindTarget() const;

	UPROPERTY()
	AHSPlayerCharacter* OwnerChar = nullptr;

	bool bSliding = false;
	float SlideElapsed = 0.f;
	FVector SlideStart = FVector::ZeroVector;
	FVector SlideTarget = FVector::ZeroVector;
	TWeakObjectPtr<AActor> TargetActor;
};
