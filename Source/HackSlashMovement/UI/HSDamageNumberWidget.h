// Simple widget for displaying a damage number.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSDamageNumberWidget.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UHSDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Damage Number")
	void SetDamageText(float Damage, FLinearColor Color);

protected:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DamageText;
};
