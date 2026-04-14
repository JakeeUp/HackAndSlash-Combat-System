// Homing projectile that tracks the locked-on target (FF16 fire bolt style).


#include "HSHomingProjectile.h"
#include "HSDamageable.h"
#include "HSStyleComponent.h"
#include "GameFramework/PlayerController.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"


AHSHomingProjectile::AHSHomingProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Collision
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionSphere->InitSphereRadius(ProjectileRadius);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	RootComponent = CollisionSphere;

	// Visible mesh (sphere) -- assign material in BP for custom look
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetRelativeScale3D(FVector(MeshScale));

	// Use the engine's built-in sphere mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereMesh.Object);
	}

	// Point light for glow
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetIntensity(GlowIntensity);
	GlowLight->SetLightColor(GlowColor);
	GlowLight->SetAttenuationRadius(200.f);
	GlowLight->CastShadows = false;

	// Projectile movement with homing
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 0.f;  // Set in FireAtTarget
	ProjectileMovement->MaxSpeed = ProjectileSpeed * 2.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
	ProjectileMovement->ProjectileGravityScale = 0.f;

	InitialLifeSpan = LifeSpan;
}

void AHSHomingProjectile::FireAtTarget(AActor* Target, AActor* InInstigator, float Damage)
{
	CachedDamage = Damage;
	CachedInstigator = InInstigator;

	// Ignore the instigator so we don't self-destruct on spawn overlap
	if (InInstigator)
	{
		CollisionSphere->MoveIgnoreActors.Add(InInstigator);
	}

	if (Target)
	{
		ProjectileMovement->HomingTargetComponent = Target->GetRootComponent();

		// Give initial velocity toward the target
		const FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		ProjectileMovement->Velocity = Dir * ProjectileSpeed;
	}
	else
	{
		ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileSpeed;
	}

	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AHSHomingProjectile::OnOverlap);
}

void AHSHomingProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == CachedInstigator || OtherActor == GetOwner()) return;

	ApplyDamageToActor(OtherActor);

	// Register hit with the Style system so projectiles count toward combo
	if (CachedInstigator)
	{
		if (UHSStyleComponent* Style = CachedInstigator->FindComponentByClass<UHSStyleComponent>())
		{
			Style->RegisterHit(CachedDamage);
		}

		// Camera shake on impact
		if (ImpactCameraShake)
		{
			if (APawn* InstigatorPawn = Cast<APawn>(CachedInstigator))
			{
				if (APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController()))
				{
					PC->ClientStartCameraShake(ImpactCameraShake, ImpactShakeScale);
				}
			}
		}
	}

	Destroy();
}

void AHSHomingProjectile::ApplyDamageToActor(AActor* Target)
{
	if (!Target) return;

	if (Target->Implements<UHSDamageable>())
	{
		IHSDamageable::Execute_ApplyDamage(Target, CachedDamage, CachedInstigator);
	}
}
