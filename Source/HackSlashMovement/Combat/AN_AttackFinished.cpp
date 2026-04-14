// Anim Notify -- signals that the current attack montage is done (resets combo state).


#include "AN_AttackFinished.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSCombatComponent.h"


void UAN_AttackFinished::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner()))
	{
		if (UHSCombatComponent* Combat = Player->GetCombat())
		{
			Combat->OnAttackFinished();
		}
	}
}
