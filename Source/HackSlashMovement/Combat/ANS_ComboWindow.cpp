// Anim Notify State -- opens the combo window while active on the montage timeline.


#include "ANS_ComboWindow.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSCombatComponent.h"


void UANS_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner()))
	{
		if (UHSCombatComponent* Combat = Player->GetCombat())
		{
			Combat->OpenComboWindow();
		}
	}
}

void UANS_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner()))
	{
		if (UHSCombatComponent* Combat = Player->GetCombat())
		{
			Combat->CloseComboWindow();
		}
	}
}
