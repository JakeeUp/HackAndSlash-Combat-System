#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSCombatTweakables.h"
#include "HSHitFeedbackComponent.generated.h"

class AHSPlayerCharacter;
enum class EAttackType : uint8;
enum class EHitWeight : uint8;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HACKSLASHMOVEMENT_API UHSHitFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSHitFeedbackComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void OnHitLanded(EAttackType AttackType, EHitWeight HitWeight, const FVector& SwingDirection);
	void UpdateFOVCompression(float DeltaTime);
	void ApplyScreenHitEffect();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Hit Feedback")
	FCombatHitFeedbackConfig Config;

private:
	void RestoreTimeDilation();

	UPROPERTY()
	AHSPlayerCharacter* OwnerChar = nullptr;

	FTimerHandle TimeDilationHandle;
	float CurrentFOVCompression = 0.f;
	float DefaultCameraFOV = 90.f;
};
