// Anim Notify State -- opens the DELAYED combo input window while active on the montage timeline.
// Place this notify state AFTER the regular ComboWindow ends (near the tail of the swing anim)
// so that a light-attack press landing inside this range branches to the delayed combo list
// instead of the normal one.  If this range overlaps with the regular combo window, the regular
// window wins (TryLightAttack checks it first).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_DelayedComboWindow.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UANS_DelayedComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Delayed Combo Window"); }

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
