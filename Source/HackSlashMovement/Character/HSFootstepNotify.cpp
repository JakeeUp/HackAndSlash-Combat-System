#include "HSFootstepNotify.h"

#include "HSPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UHSFootstepNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AHSPlayerCharacter* Char = Cast<AHSPlayerCharacter>(MeshComp->GetOwner());
	if (!Char) return;

	Char->PlayFootstep(FootBone);
}
