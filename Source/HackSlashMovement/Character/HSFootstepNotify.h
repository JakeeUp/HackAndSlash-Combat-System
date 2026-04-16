#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "HSFootstepNotify.generated.h"


/**
 * Anim notify placed on foot-plant frames in walk / run / idle animations.
 * Traces down from the specified foot bone, reads the physical surface,
 * and asks the owning player character to play the matching VFX / SFX.
 */
UCLASS(meta = (DisplayName = "Footstep"))
class HACKSLASHMOVEMENT_API UHSFootstepNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** Bone to trace down from. Use foot_l or foot_r on UE5 Mannequin. */
	UPROPERTY(EditAnywhere, Category = "Footstep")
	FName FootBone = TEXT("foot_l");
};
