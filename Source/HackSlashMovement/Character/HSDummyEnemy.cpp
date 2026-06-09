#include "HSDummyEnemy.h"

#include "Character/HSEnemyCombatAI.h"
#include "Character/HSEnemyAIController.h"
#include "Character/HSXPOrb.h"
#include "Character/HSPlayerCharacter.h"
#include "Combat/HSDynamicCameraComponent.h"
#include "UI/HSDamageNumber.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AHSDummyEnemy::AHSDummyEnemy()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Ignore the camera channel so the player's spring arm doesn't collide
	// with the enemy when attacking up close.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 0.f;

	// AI component -- provides the DMC-style combat state machine
	CombatAI = CreateDefaultSubobject<UHSEnemyCombatAI>(TEXT("CombatAI"));

	// Use our minimal AIController so the combat AI gets NavMesh pathfinding
	AIControllerClass = AHSEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AHSDummyEnemy::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
}

void AHSDummyEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bIsDead) return;

	if (EnemyState == EEnemyState::EES_Airborne)
	{
		bInDropLoop = false;

		if (LandImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, LandImpactSound, GetActorLocation(), SFXVolumeMultiplier);
		}

		// Transition to Down -- the ABP state machine handles drop end → getup animations.
		// After a delay, return to Idle so the getup animation has time to play.
		EnemyState = EEnemyState::EES_Down;

		GetWorldTimerManager().ClearTimer(GetupTimerHandle);
		GetWorldTimerManager().SetTimer(GetupTimerHandle, this, &AHSDummyEnemy::PlayGetup, GetupDelay, false);
	}
}

void AHSDummyEnemy::PlayGetup()
{
	if (bIsDead || EnemyState != EEnemyState::EES_Down) return;

	// Set to Idle -- the ABP state machine transitions through the getup animation
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

	// Hit SFX
	{
		USoundBase* HitSFX = nullptr;
		if (HitWeight == EHitWeight::EHW_Launcher || HitWeight == EHitWeight::EHW_Finisher)
		{
			HitSFX = LaunchSound;
		}
		else if (bIsProjectile)
		{
			HitSFX = ProjectileHitSound;
		}
		else if (bIsHeavy)
		{
			HitSFX = HeavyHitSound;
		}
		else
		{
			HitSFX = LightHitSound;
		}

		if (HitSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, HitSFX, GetActorLocation(), SFXVolumeMultiplier);
		}
	}

	// Hit VFX
	SpawnHitVFX(HitDirection, bIsProjectile);

	// Knockback
	ApplyKnockback(HitDirection, HitWeight, bIsProjectile);

	// Hitstop
	ApplyHitstop(HitWeight);

	// Red overlay flash -- player gets immediate visual confirmation of damage landing.
	FlashDamageOverlay();

	// Hit react animation -- pick montage based on enemy state.
	// Launcher hits skip the montage entirely -- the ABP state machine
	// handles the HitStart → DropLoop transition for a clean launch look.
	UAnimMontage* ReactMontage = nullptr;
	const bool bIsLauncher = (HitWeight == EHitWeight::EHW_Launcher);

	if (bIsLauncher)
	{
		// Pulse the launched flag -- ABP reads this to immediately jump to the
		// air hit-react state. No montage needed; the state machine handles it.
		bInDropLoop = false;
		bJustLaunched = true;
	}
	else if (EnemyState == EEnemyState::EES_Airborne && AirHitReactMontages.Num() > 0)
	{
		// Stop any active drop loop -- enemy is getting hit again
		bInDropLoop = false;

		const int32 Idx = FMath::RandRange(0, AirHitReactMontages.Num() - 1);
		ReactMontage = AirHitReactMontages[Idx];
	}
	else if (EnemyState == EEnemyState::EES_Down && DownHitReactMontages.Num() > 0)
	{
		// Cancel pending getup -- they're getting hit while down
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

	if (ReactMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(ReactMontage, HitReactPlayRate);

			// When an air hit react ends, transition to drop loop if still airborne
			if (EnemyState == EEnemyState::EES_Airborne)
			{
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &AHSDummyEnemy::OnAirHitReactEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, ReactMontage);
			}
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

	// Let the AI know a hit landed so it can interrupt attacks / stagger
	NotifyAIHit(HitWeight);

	if (CurrentHealth <= 0.f)
	{
		Die();
	}
}

void AHSDummyEnemy::NotifyAIHit(EHitWeight HitWeight)
{
	if (CombatAI)
	{
		CombatAI->NotifyHit(HitWeight);
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

		// Tell the player camera to tilt upward so the juggle is visible
		if (HitWeight == EHitWeight::EHW_Launcher)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(PC->GetPawn()))
				{
					if (UHSDynamicCameraComponent* DynCam = Player->GetDynamicCamera())
					{
						DynCam->NotifyEnemyLaunched();
					}
				}
			}
		}
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

void AHSDummyEnemy::OnAirHitReactEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// If interrupted (got hit again), don't start the drop loop
	if (bInterrupted) return;

	// If still airborne after the hit react, set the drop loop flag.
	// The ABP state machine reads this and plays the looping fall animation.
	if (EnemyState == EEnemyState::EES_Airborne)
	{
		bInDropLoop = true;
	}
}

void AHSDummyEnemy::StartDropLoop()
{
	if (bIsDead) return;
	bInDropLoop = true;
	bJustLaunched = false;  // ABP has had its transition window; clear the pulse
}

void AHSDummyEnemy::Die()
{
	bIsDead = true;

	// Restore in case we die during hitstop
	EndHitstop();

	// Kill any active damage flash so it doesn't linger on the death pose
	GetWorldTimerManager().ClearTimer(DamageFlashTimerHandle);
	ClearDamageFlash();

	// Scatter XP orbs from the death location
	SpawnXPOrbs();

	// Notify player's dynamic camera -- triggers kill cam if this was the last enemy
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(PC->GetPawn()))
		{
			if (UHSDynamicCameraComponent* DynCam = Player->GetDynamicCamera())
			{
				DynCam->NotifyEnemyKill();
			}
		}
	}

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

void AHSDummyEnemy::FlashDamageOverlay()
{
	if (!DamageFlashMaterial) return;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetOverlayMaterial(DamageFlashMaterial);
	}

	// Reset the clear timer so rapid hits extend the flash cleanly.
	GetWorldTimerManager().ClearTimer(DamageFlashTimerHandle);
	GetWorldTimerManager().SetTimer(DamageFlashTimerHandle, this, &AHSDummyEnemy::ClearDamageFlash, DamageFlashDuration, false);
}

void AHSDummyEnemy::ClearDamageFlash()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetOverlayMaterial(nullptr);
	}
}

void AHSDummyEnemy::SpawnXPOrbs()
{
	if (!XPOrbClass || XPReward <= 0.f || XPOrbCount <= 0) return;

	const float XPPerOrb = XPReward / static_cast<float>(XPOrbCount);
	const FVector SpawnOrigin = GetActorLocation() + FVector(0.f, 0.f, 40.f);

	for (int32 i = 0; i < XPOrbCount; ++i)
	{
		// Spread orbs evenly around a full circle so they fan out rather than cluster.
		const float Angle = (360.f / XPOrbCount) * i + FMath::FRandRange(-15.f, 15.f);
		const FVector ScatterDir(
			FMath::Cos(FMath::DegreesToRadians(Angle)),
			FMath::Sin(FMath::DegreesToRadians(Angle)),
			0.f
		);

		if (AHSXPOrb* Orb = GetWorld()->SpawnActor<AHSXPOrb>(XPOrbClass, SpawnOrigin, FRotator::ZeroRotator))
		{
			Orb->Initialize(XPPerOrb, ScatterDir);
		}
	}
}

void AHSDummyEnemy::ApplyAirHang(float Duration, float GravScale, float Lift)
{
	if (bIsDead) return;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement) return;

	// First call of a hang burst -- capture the current gravity so EndAirHang can put it back.
	// Subsequent calls during an active hang just refresh the timer (and re-pop vertically) so
	// we don't save the already-modified value.
	if (!bAirHangActive)
	{
		AirHangSavedGravity = Movement->GravityScale;
		bAirHangActive = true;
	}

	Movement->GravityScale = GravScale;

	// Zero the downward velocity and give a small upward pop so the enemy floats crisply
	// each time a hit connects, instead of slowly drifting down even on low gravity.
	FVector Vel = Movement->Velocity;
	Vel.Z = Lift;
	Movement->Velocity = Vel;

	// Reading the airborne flag so the ABP's air hit-react state activates visually.
	// This is the same flag set by launcher knockbacks.
	if (EnemyState != EEnemyState::EES_Down)
	{
		EnemyState = EEnemyState::EES_Airborne;
	}

	// Refresh the end-of-hang timer so as long as the player keeps connecting, the enemy
	// keeps floating.  Single-shot -- ClearTimer first ensures we don't stack callbacks.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AirHangTimerHandle);
		World->GetTimerManager().SetTimer(AirHangTimerHandle, this, &AHSDummyEnemy::EndAirHang, Duration, false);
	}
}

void AHSDummyEnemy::EndAirHang()
{
	if (!bAirHangActive) return;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = AirHangSavedGravity;
	}
	bAirHangActive = false;
}
