// Anim Notify -- triggers a timed cinematic camera shot on the owning player.

#include "AN_CinematicCamera.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSDynamicCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"


void UAN_CinematicCamera::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner());
	if (!Player) return;

	UHSDynamicCameraComponent* DynCam = Player->GetDynamicCamera();
	if (!DynCam) return;

	DynCam->PlayCinematicShot(
		ArmLength,
		SocketOffset,
		FOV,
		PitchBias,
		RollBias,
		BlendIn, Hold, BlendOut,
		DollyArmDelta, DollyXDelta, OrbitYawDelta
	);
}
