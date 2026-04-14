// Lock-on reticle widget -- displays on the locked target (FF16/DMC3 style).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSLockOnReticle.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UHSLockOnReticle : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called when lock-on is first acquired -- can trigger a pop-in animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LockOn")
	void PlayLockOnAcquired();

	/** Called when lock-on is released. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LockOn")
	void PlayLockOnReleased();
};
