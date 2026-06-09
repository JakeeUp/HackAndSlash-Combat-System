// Anim Notify State -- opens the DELAYED combo input window while active on the montage timeline.


#include "ANS_DelayedComboWindow.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSCombatComponent.h"


void UANS_DelayedComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner()))
	{
		if (UHSCombatComponent* Combat = Player->GetCombat())
		{
			Combat->OpenDelayedComboWindow();
		}
	}
}

void UANS_DelayedComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner()))
	{
		if (UHSCombatComponent* Combat = Player->GetCombat())
		{
			Combat->CloseDelayedComboWindow();
		}
	}
}
