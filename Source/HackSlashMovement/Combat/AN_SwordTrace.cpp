// Anim Notify -- fires a sphere trace from the weapon to detect hits.


#include "AN_SwordTrace.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSCombatComponent.h"


void UAN_SwordTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner()))
	{
		if (UHSCombatComponent* Combat = Player->GetCombat())
		{
			Combat->DoSwordTrace();
		}
	}
}
