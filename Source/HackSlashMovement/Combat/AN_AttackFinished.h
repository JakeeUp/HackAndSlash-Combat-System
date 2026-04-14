// Anim Notify -- signals that the current attack montage is done (resets combo state).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_AttackFinished.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UAN_AttackFinished : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Attack Finished"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
