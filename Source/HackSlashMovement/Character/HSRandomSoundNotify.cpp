#include "HSRandomSoundNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UHSRandomSoundNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || Sounds.IsEmpty()) return;

	// Pick a random sound from the pool
	const int32 Idx = FMath::RandRange(0, Sounds.Num() - 1);
	USoundBase* Sound = Sounds[Idx];
	if (!Sound) return;

	if (bFollowOwner)
	{
		// Attached to the mesh -- follows the owner while playing (good for clanks, grunts)
		UGameplayStatics::SpawnSoundAttached(
			Sound,
			MeshComp,
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false,
			VolumeMultiplier,
			PitchMultiplier
		);
	}
	else
	{
		// Spawned at world location -- stays where the swing happened (good for impacts)
		UGameplayStatics::PlaySoundAtLocation(
			MeshComp->GetWorld(),
			Sound,
			MeshComp->GetComponentLocation(),
			VolumeMultiplier,
			PitchMultiplier
		);
	}
}
