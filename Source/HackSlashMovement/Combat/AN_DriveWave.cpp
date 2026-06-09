// Anim Notify -- spawns a ground-travelling energy wave (Dante "Drive" style).

#include "AN_DriveWave.h"

#include "AHSDriveWave.h"
#include "Character/HSPlayerCharacter.h"


void UAN_DriveWave::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !WaveClass) return;

	AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(MeshComp->GetOwner());
	if (!Player) return;

	UWorld* World = Player->GetWorld();
	if (!World) return;

	// Fire toward the locked-on target; fall back to actor forward.
	FVector FireDir = Player->GetActorForwardVector();
	if (AActor* Target = Player->GetLockedTarget())
	{
		FireDir = (Target->GetActorLocation() - Player->GetActorLocation());
		FireDir.Z = 0.f;
		FireDir.Normalize();
	}

	// Spawn at the player's base so the wave hugs the ground.
	const FVector SpawnLoc = Player->GetActorLocation();
	const FRotator SpawnRot = FireDir.Rotation();

	FActorSpawnParameters Params;
	Params.Instigator = Player;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AHSDriveWave* Wave = World->SpawnActor<AHSDriveWave>(WaveClass, SpawnLoc, SpawnRot, Params))
	{
		Wave->Launch(Player, DamageAmount, HitWeight);
		
	}
}
