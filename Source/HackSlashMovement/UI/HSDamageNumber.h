// Floating damage number that spawns at hit location, floats up, and fades out.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSDamageNumber.generated.h"

class UWidgetComponent;


UCLASS()
class HACKSLASHMOVEMENT_API AHSDamageNumber : public AActor
{
	GENERATED_BODY()

public:
	AHSDamageNumber();

	virtual void Tick(float DeltaTime) override;

	/** Initialize with damage value and optional color. */
	void Initialize(float Damage, FLinearColor Color = FLinearColor::White);

protected:
	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* WidgetComp;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Number")
	float FloatSpeed = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Number")
	float Lifetime = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Number")
	float FadeStartAt = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Number")
	float RandomSpreadX = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Number")
	float RandomSpreadY = 30.f;

private:
	float Age = 0.f;
	FVector FloatDirection;
};
