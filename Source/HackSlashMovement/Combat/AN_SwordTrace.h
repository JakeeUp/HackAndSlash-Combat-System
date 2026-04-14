// Anim Notify -- fires a sphere trace from the weapon to detect hits.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_SwordTrace.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UAN_SwordTrace : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Sword Trace"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
