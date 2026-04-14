// Fill out your copyright notice in the Description page of Project Settings.


#include "HSDummyEnemy.h"

#include "UI/HSDamageNumber.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

// Sets default values
AHSDummyEnemy::AHSDummyEnemy()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 0.f;
}

// Called when the game starts or when spawned
void AHSDummyEnemy::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
}

void AHSDummyEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AHSDummyEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bIsDead) return;

	if (EnemyState == EEnemyState::EES_Airborne)
	{
		EnemyState = EEnemyState::EES_Idle;
	}
}

void AHSDummyEnemy::PlayGetup()
{
	if (bIsDead || EnemyState != EEnemyState::EES_Down) return;

	if (GetupMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(GetupMontage, 1.f);
		}
	}

	EnemyState = EEnemyState::EES_Idle;
}

void AHSDummyEnemy::ApplyDamage_Implementation(float DamageAmount, AActor* DamageCauser)
{
	FVector HitDir = FVector::ZeroVector;
	if (DamageCauser)
	{
		HitDir = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
		HitDir.Z = 0.f;
	}
	HandleHitReaction(DamageAmount, DamageCauser, HitDir, EHitWeight::EHW_Light);
}

void AHSDummyEnemy::ApplyDamageWithInfo_Implementation(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, bool bIsHeavyHit)
{
	HandleHitReaction(DamageAmount, DamageCauser, HitDirection, bIsHeavyHit ? EHitWeight::EHW_Heavy : EHitWeight::EHW_Light);
}

void AHSDummyEnemy::ApplyDamageEx_Implementation(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight)
{
	HandleHitReaction(DamageAmount, DamageCauser, HitDirection, HitWeight);
}

void AHSDummyEnemy::HandleHitReaction(float DamageAmount, AActor* DamageCauser, const FVector& HitDirection, EHitWeight HitWeight)
{
	if (bIsDead || DamageAmount <= 0.f) return;

	CurrentHealth = FMath::Max(0.f, CurrentHealth - DamageAmount);

	const bool bIsHeavy = (HitWeight == EHitWeight::EHW_Heavy || HitWeight == EHitWeight::EHW_Launcher || HitWeight == EHitWeight::EHW_Finisher);

	// Spawn floating damage number
	if (DamageNumberClass)
	{
		const FVector SpawnLoc = GetActorLocation() + DamageNumberOffset;
		AHSDamageNumber* DmgNum = GetWorld()->SpawnActor<AHSDamageNumber>(DamageNumberClass, SpawnLoc, FRotator::ZeroRotator);
		if (DmgNum)
		{
			FLinearColor DmgColor = bIsHeavy ? FLinearColor(1.f, 0.7f, 0.1f, 1.f) : FLinearColor::White;
			DmgNum->Initialize(DamageAmount, DmgColor);
		}
	}

	const bool bIsProjectile = (HitWeight == EHitWeight::EHW_Light) && DamageCauser && !DamageCauser->IsA(ACharacter::StaticClass());

	// Hit VFX
	SpawnHitVFX(HitDirection, bIsProjectile);

	// Knockback
	ApplyKnockback(HitDirection, HitWeight, bIsProjectile);

	// Hitstop
	ApplyHitstop(HitWeight);

	// Hit react animation -- pick montage based on enemy state
	UAnimMontage* ReactMontage = nullptr;

	if (EnemyState == EEnemyState::EES_Airborne && AirHitReactMontage)
	{
		ReactMontage = AirHitReactMontage;
	}
	else if (EnemyState == EEnemyState::EES_Down && DownHitReactMontages.Num() > 0)
	{
		// Cancel pending sequence -- they're getting hit while down
		GetWorldTimerManager().ClearTimer(DropEndTimerHandle);
		GetWorldTimerManager().ClearTimer(GetupTimerHandle);
		const int32 Idx = FMath::RandRange(0, DownHitReactMontages.Num() - 1);
		ReactMontage = DownHitReactMontages[Idx];
		// Re-schedule getup after this hit
		GetWorldTimerManager().SetTimer(GetupTimerHandle, this, &AHSDummyEnemy::PlayGetup, GetupDelay, false);
	}
	else if (bIsHeavy && HeavyHitReactMontage)
	{
		ReactMontage = HeavyHitReactMontage;
	}
	else if (LightHitReactMontages.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, LightHitReactMontages.Num() - 1);
		ReactMontage = LightHitReactMontages[Idx];
	}
	else if (HitReactMontage)
	{
		ReactMontage = HitReactMontage;
	}

	if (ReactMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(ReactMontage, HitReactPlayRate);
		}
	}

	// Face the attacker when hit (so knockback looks correct)
	if (DamageCauser)
	{
		FVector LookDir = DamageCauser->GetActorLocation() - GetActorLocation();
		LookDir.Z = 0.f;
		if (!LookDir.IsNearlyZero())
		{
			SetActorRotation(LookDir.Rotation());
		}
	}

	if (CurrentHealth <= 0.f)
	{
		Die();
	}
}

void AHSDummyEnemy::SpawnHitVFX(const FVector& HitDirection, bool bIsProjectile)
{
	UNiagaraSystem* VFX = bIsProjectile ? ProjectileHitVFX : SwordHitVFX;
	if (!VFX) return;

	// Spawn VFX at the enemy's center, oriented along the hit direction
	const FVector SpawnLoc = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FRotator SpawnRot = HitDirection.IsNearlyZero() ? GetActorRotation() : HitDirection.Rotation();

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		VFX,
		SpawnLoc,
		SpawnRot,
		HitVFXScale,
		true,   // bAutoDestroy
		true,   // bAutoActivate
		ENCPoolMethod::None
	);
}

void AHSDummyEnemy::ApplyKnockback(const FVector& HitDirection, EHitWeight HitWeight, bool bIsProjectile)
{
	if (HitDirection.IsNearlyZero()) return;

	float Force;
	float Lift = 0.f;

	if (bIsProjectile)
	{
		Force = ProjectileKnockbackForce;
	}
	else
	{
		switch (HitWeight)
		{
		case EHitWeight::EHW_Launcher:
			Force = 0.f;  // Launcher goes straight up, no horizontal push
			Lift = LaunchUpForce;
			break;
		case EHitWeight::EHW_Finisher:
			Force = SendFlyingForce;
			Lift = SendFlyingLift;
			break;
		case EHitWeight::EHW_Heavy:
			Force = HeavyKnockbackForce;
			Lift = HeavyKnockbackLift;
			break;
		default:
			Force = LightKnockbackForce;
			break;
		}
	}

	// DMC-style juggle: if already airborne, apply a small upward push to
	// keep the enemy suspended regardless of attack type.
	if (EnemyState == EEnemyState::EES_Airborne && Lift <= 0.f)
	{
		Lift = AirJuggleLift;
	}

	FVector KnockDir = HitDirection;
	KnockDir.Z = 0.f;
	KnockDir.Normalize();

	const FVector KnockbackVelocity = KnockDir * Force + FVector(0.f, 0.f, Lift);
	LaunchCharacter(KnockbackVelocity, true, true);

	// Any hit with lift puts enemy into airborne state (so air hit reacts play)
	// But don't override Down state -- enemy is already in the landing sequence
	if (Lift > 0.f && EnemyState != EEnemyState::EES_Down)
	{
		EnemyState = EEnemyState::EES_Airborne;
	}
}

void AHSDummyEnemy::ApplyHitstop(EHitWeight HitWeight)
{
	const bool bIsHeavy = (HitWeight != EHitWeight::EHW_Light);
	const float Duration = bIsHeavy ? HeavyHitstopDuration : LightHitstopDuration;
	if (Duration <= 0.f) return;

	// Freeze the enemy's animation briefly
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->GlobalAnimRateScale = 0.f;
	}
	CustomTimeDilation = 0.01f;

	// Restore after the hitstop duration
	GetWorldTimerManager().ClearTimer(HitstopTimerHandle);
	GetWorldTimerManager().SetTimer(HitstopTimerHandle, this, &AHSDummyEnemy::EndHitstop, Duration, false);
}

void AHSDummyEnemy::EndHitstop()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->GlobalAnimRateScale = 1.f;
	}
	CustomTimeDilation = 1.f;
}

void AHSDummyEnemy::Die()
{
	bIsDead = true;

	// Restore in case we die during hitstop
	EndHitstop();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	if (DeathMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(DeathMontage, 1.f);
		}
	}

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([this]()
	{
		Destroy();
	}), DeathCleanupDelay, false);
}
