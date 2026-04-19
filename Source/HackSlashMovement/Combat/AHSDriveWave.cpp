// Ground-travelling energy wave spawned by AN_DriveWave (Dante "Drive" style).

#include "AHSDriveWave.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSDamageable.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/EngineTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"


AHSDriveWave::AHSDriveWave()
{
	PrimaryActorTick.bCanEverTick = true;

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // we do manual overlaps
	HitBox->SetGenerateOverlapEvents(false);
	SetRootComponent(HitBox);

	WaveVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WaveVFXComponent"));
	WaveVFXComponent->SetupAttachment(HitBox);
	WaveVFXComponent->SetAutoActivate(false);
}

void AHSDriveWave::BeginPlay()
{
	Super::BeginPlay();

	// Size the hit box to the configured half-extents.
	HitBox->SetBoxExtent(FVector(HitDepth, HitWidth, HitHeight));
}

void AHSDriveWave::Launch(AHSPlayerCharacter* InOwner, float InDamage, EHitWeight InHitWeight)
{
	OwnerPlayer    = InOwner;
	CachedDamage   = InDamage;
	CachedHitWeight = InHitWeight;

	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}

	if (WaveVFXComponent && WaveVFXComponent->GetAsset())
	{
		WaveVFXComponent->ActivateSystem();
	}
}

void AHSDriveWave::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Advance the wave forward along its local X axis.
	const float StepDist = TravelSpeed * DeltaTime;
	AddActorWorldOffset(GetActorForwardVector() * StepDist);
	TraveledDistance += StepDist;

	CheckOverlaps();

	if (TraveledDistance >= MaxRange)
	{
		Destroy();
	}
}

void AHSDriveWave::CheckOverlaps()
{
	TArray<AActor*> IgnoreActors;
	if (OwnerPlayer) IgnoreActors.Add(OwnerPlayer);

	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::BoxOverlapActors(
		GetWorld(),
		GetActorLocation(),
		FVector(HitDepth, HitWidth, HitHeight),
		ObjectTypes,
		nullptr,
		IgnoreActors,
		OverlappedActors
	);

	for (AActor* Actor : OverlappedActors)
	{
		if (!Actor) continue;

		// Pierce: each enemy is only hit once per wave.
		bool bAlreadyHit = false;
		for (const TWeakObjectPtr<AActor>& Weak : HitActors)
		{
			if (Weak.Get() == Actor) { bAlreadyHit = true; break; }
		}
		if (bAlreadyHit) continue;

		if (!Actor->GetClass()->ImplementsInterface(UHSDamageable::StaticClass())) continue;

		HitActors.Add(Actor);

		// Direction is always the wave's forward (push enemies away from the player).
		const FVector HitDir = GetActorForwardVector();
		IHSDamageable::Execute_ApplyDamageEx(Actor, CachedDamage, OwnerPlayer, HitDir, CachedHitWeight);

		if (ImpactVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), ImpactVFX,
				Actor->GetActorLocation(),
				HitDir.Rotation(),
				FVector(1.f), true, true, ENCPoolMethod::None
			);
		}

		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Actor->GetActorLocation());
		}
	}
}
