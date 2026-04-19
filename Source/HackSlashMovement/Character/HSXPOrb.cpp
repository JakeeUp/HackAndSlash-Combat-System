// XP orb -- explodes outward on enemy death with simulated arc, hovers, then vacuums to player.

#include "HSXPOrb.h"

#include "HSPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"


AHSXPOrb::AHSXPOrb()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USphereComponent>(TEXT("Root"));
	Root->InitSphereRadius(16.f);
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Root);

	OrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh"));
	OrbMesh->SetupAttachment(Root);
	OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMesh->SetGenerateOverlapEvents(false);
}

void AHSXPOrb::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		CachedPlayer = Cast<AHSPlayerCharacter>(PC->GetPawn());
	}

	SetLifeSpan(OrbLifeSpan);
	CurrentAttractionSpeed = AttractionSpeed;

	GetWorldTimerManager().SetTimer(
		AttractionTimerHandle,
		this,
		&AHSXPOrb::StartAttracting,
		AttractionDelay,
		false
	);
}

void AHSXPOrb::Initialize(float InXPValue, const FVector& BurstDirection)
{
	XPValue = InXPValue;

	// Resolve the outward direction -- randomise if none given.
	FVector Dir = BurstDirection;
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero())
	{
		const float Angle = FMath::FRandRange(0.f, 360.f);
		Dir = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.f);
	}
	Dir.Normalize();

	// Add random angular spread so every orb flies a slightly different path.
	const float SpreadAngle = FMath::FRandRange(-BurstSpread, BurstSpread);
	Dir = Dir.RotateAngleAxis(SpreadAngle, FVector::UpVector);

	// Randomise magnitude so orbs don't all travel the same distance.
	const float HSpeed = FMath::FRandRange(BurstSpeed  * 0.7f, BurstSpeed  * 1.3f);
	const float VSpeed = FMath::FRandRange(BurstUpForce * 0.6f, BurstUpForce * 1.2f);

	BurstVelocity = Dir * HSpeed + FVector(0.f, 0.f, VSpeed);
	BurstTimer    = BurstDuration;
	bBursting     = true;
}

void AHSXPOrb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedPlayer) return;

	// ── Burst phase: simulate a projectile arc with manual gravity ─────────
	if (bBursting)
	{
		BurstVelocity.Z -= BurstGravity * DeltaTime;

		const FVector NewLoc = GetActorLocation() + BurstVelocity * DeltaTime;
		SetActorLocation(NewLoc);

		BurstTimer -= DeltaTime;
		if (BurstTimer <= 0.f)
		{
			bBursting  = false;
			HoverBaseZ = GetActorLocation().Z;
			HoverTime  = FMath::FRandRange(0.f, PI * 2.f); // random phase so orbs don't all bob in sync
		}
		return; // Don't check attraction during the burst arc
	}

	// ── Early attraction trigger: player walked into auto-pickup range ──────
	if (!bAttracting)
	{
		HoverTime += DeltaTime * HoverBobSpeed;
		const FVector Loc = GetActorLocation();
		SetActorLocation(FVector(Loc.X, Loc.Y, HoverBaseZ + FMath::Sin(HoverTime) * HoverBobAmplitude));

		if (FVector::Dist(Loc, CachedPlayer->GetActorLocation()) <= AutoPickupRange)
		{
			StartAttracting();
		}
		return;
	}

	// ── Attraction phase: vacuum toward the player, accelerating ──────────
	CurrentAttractionSpeed = FMath::FInterpTo(
		CurrentAttractionSpeed,
		AttractionSpeed * 8.f,
		DeltaTime,
		AttractionAcceleration
	);

	const FVector Target = CachedPlayer->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Dir    = (Target - GetActorLocation()).GetSafeNormal();
	SetActorLocation(GetActorLocation() + Dir * CurrentAttractionSpeed * DeltaTime);

	if (FVector::Dist(GetActorLocation(), CachedPlayer->GetActorLocation()) < CollectRange)
	{
		Collect();
	}
}

void AHSXPOrb::StartAttracting()
{
	if (bAttracting) return;
	bBursting = false; // safety: cut arc short if somehow still running
	bAttracting = true;
	CurrentAttractionSpeed = AttractionSpeed;
	GetWorldTimerManager().ClearTimer(AttractionTimerHandle);
}

void AHSXPOrb::Collect()
{
	if (!CachedPlayer) return;

	CachedPlayer->AddXP(XPValue);

	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	if (PickupVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), PickupVFX, GetActorLocation(),
			FRotator::ZeroRotator, FVector(1.f),
			true, true, ENCPoolMethod::None
		);
	}

	Destroy();
}
