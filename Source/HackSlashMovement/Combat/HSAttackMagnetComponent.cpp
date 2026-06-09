#include "HSAttackMagnetComponent.h"

#include "Character/HSPlayerCharacter.h"
#include "Character/HSDummyEnemy.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UHSAttackMagnetComponent::UHSAttackMagnetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSAttackMagnetComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerChar = Cast<AHSPlayerCharacter>(GetOwner());
}

/*****************************************************/
/*                      Slide                        */
/*****************************************************/

bool UHSAttackMagnetComponent::TryStartSlide()
{
	if (!Config.bEnabled || !OwnerChar) return false;

	AActor* Target = FindTarget();
	if (!Target) return false;

	const FVector OwnerLoc  = OwnerChar->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();

	FVector FromTarget = OwnerLoc - TargetLoc;
	FromTarget.Z = 0.f;
	if (FromTarget.IsNearlyZero())
	{
		return false;
	}
	const FVector StandoffDir = FromTarget.GetSafeNormal();

	const float CurDist2D = FVector::Dist2D(OwnerLoc, TargetLoc);
	if (Config.bOnlyCloseGaps && CurDist2D <= Config.IdealDistance + Config.GapCloseTolerance)
	{
		TargetActor  = Target;
		bSliding     = false;
		SlideElapsed = 0.f;
		return true;
	}

	FVector Destination = TargetLoc + StandoffDir * Config.IdealDistance;
	Destination.Z = OwnerLoc.Z;

	const float SlideDist = FVector::Dist(OwnerLoc, Destination);
	if (SlideDist > Config.MaxSlideDistance)
	{
		return false;
	}

	SlideStart   = OwnerLoc;
	SlideTarget  = Destination;
	SlideElapsed = 0.f;
	bSliding     = (SlideDist > KINDA_SMALL_NUMBER);
	TargetActor  = Target;

	return true;
}

void UHSAttackMagnetComponent::UpdateSlide(float DeltaTime, bool bOwnerAttacking)
{
	if (!OwnerChar) return;

	if (bSliding)
	{
		SlideElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(SlideElapsed / FMath::Max(Config.SlideDuration, 0.01f), 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);
		const FVector NewLoc = FMath::Lerp(SlideStart, SlideTarget, Eased);

		OwnerChar->SetActorLocation(NewLoc, true);

		if (Alpha >= 1.f)
		{
			bSliding = false;
		}
		return;
	}

	AActor* Target = TargetActor.Get();
	if (!Target || !bOwnerAttacking || Config.MinDistance <= 0.f) return;

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector OwnerLoc  = OwnerChar->GetActorLocation();

	FVector FromTarget = OwnerLoc - TargetLoc;
	const float SavedZ = FromTarget.Z;
	FromTarget.Z = 0.f;

	const float CurDist = FromTarget.Size();
	if (CurDist >= Config.MinDistance) return;
	if (CurDist < KINDA_SMALL_NUMBER) return;

	const FVector StandoffDir = FromTarget / CurDist;
	const FVector ClampedLoc  = TargetLoc + StandoffDir * Config.MinDistance + FVector(0.f, 0.f, SavedZ);

	OwnerChar->SetActorLocation(ClampedLoc, true);
}

void UHSAttackMagnetComponent::EndSlide()
{
	bSliding     = false;
	SlideElapsed = 0.f;
	TargetActor  = nullptr;
}

/*****************************************************/
/*                  Target Finding                   */
/*****************************************************/

AActor* UHSAttackMagnetComponent::FindTarget() const
{
	if (!OwnerChar) return nullptr;

	if (AActor* Locked = OwnerChar->GetLockedTarget())
	{
		if (!Locked->IsPendingKillPending())
		{
			const float Dist = FVector::Dist(OwnerChar->GetActorLocation(), Locked->GetActorLocation());
			if (Dist <= Config.SearchRadius)
			{
				return Locked;
			}
		}
	}

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	const FVector OwnerLoc = OwnerChar->GetActorLocation();
	const FVector Forward  = OwnerChar->GetActorForwardVector();

	TArray<AActor*> Overlaps;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(OwnerChar);

	UKismetSystemLibrary::SphereOverlapActors(
		World, OwnerLoc, Config.SearchRadius,
		ObjectTypes, AHSDummyEnemy::StaticClass(),
		IgnoreActors, Overlaps);

	const bool bAirborne = OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling();
	const float ActiveCone = bAirborne ? Config.AirSearchConeDegrees : Config.SearchConeDegrees;
	const float ConeCosine = FMath::Cos(FMath::DegreesToRadians(FMath::Min(ActiveCone, 179.9f)));

	AActor* Best = nullptr;
	float   BestDistSq = TNumericLimits<float>::Max();

	for (AActor* Candidate : Overlaps)
	{
		if (!Candidate) continue;
		AHSDummyEnemy* Enemy = Cast<AHSDummyEnemy>(Candidate);
		if (!Enemy || Enemy->IsDead()) continue;

		FVector ToTarget = Candidate->GetActorLocation() - OwnerLoc;
		ToTarget.Z = 0.f;
		const float DistSq = ToTarget.SizeSquared();
		if (DistSq < KINDA_SMALL_NUMBER) continue;

		const FVector Dir = ToTarget.GetSafeNormal();
		FVector FlatForward = Forward;
		FlatForward.Z = 0.f;
		FlatForward.Normalize();

		if (FVector::DotProduct(FlatForward, Dir) < ConeCosine) continue;

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	return Best;
}
