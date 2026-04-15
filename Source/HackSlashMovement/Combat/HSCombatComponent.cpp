#include "HSCombatComponent.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSDamageable.h"
#include "Combat/HSStyleComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UHSCombatComponent::UHSCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<AHSPlayerCharacter>(GetOwner());
}

void UHSCombatComponent::TryLightAttack()
{
	if (!bIsAttacking)
	{
		ComboIndex = 0;
		PlayNextAttack(EAttackType::EAT_Light);
		return;
	}

	if (bComboWindowOpen)
	{
		PlayNextAttack(EAttackType::EAT_Light);
	}
	else
	{
		bSavedNextAttack = true;
		BufferedAttackType = EAttackType::EAT_Light;
	}
}

void UHSCombatComponent::TryHeavyAttack()
{
	if (!bIsAttacking)
	{
		ComboIndex = 0;
		PlayNextAttack(EAttackType::EAT_Heavy);
		return;
	}

	if (bComboWindowOpen)
	{
		PlayNextAttack(EAttackType::EAT_Heavy);
	}
	else
	{
		bSavedNextAttack = true;
		BufferedAttackType = EAttackType::EAT_Heavy;
	}
}

void UHSCombatComponent::TryAirAttack()
{
	if (!bIsAttacking)
	{
		ComboIndex = 0;
		PlayNextAttack(EAttackType::EAT_Air);
		return;
	}

	if (bComboWindowOpen)
	{
		PlayNextAttack(EAttackType::EAT_Air);
	}
	else
	{
		bSavedNextAttack = true;
		BufferedAttackType = EAttackType::EAT_Air;
	}
}

void UHSCombatComponent::TryRisingAttack()
{
	if (!OwnerChar || !RisingAttackMontage) return;

	// Cancel any active attack
	if (bIsAttacking)
	{
		CancelAttack();
	}

	UAnimInstance* AnimInst = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	// Face the locked target
	RotateOwnerToInput();

	AnimInst->Montage_Play(RisingAttackMontage, 1.f);

	bIsAttacking = true;
	bComboWindowOpen = false;
	bSavedNextAttack = false;
	CurrentAttackType = EAttackType::EAT_Rising;
	ComboIndex = 0;

	PlaySwingSound();
}

void UHSCombatComponent::PlayNextAttack(EAttackType Type)
{
	if (!OwnerChar) return;

	UAnimMontage* Montage = GetMontageForCombo(Type, ComboIndex);
	if (!Montage)
	{
		ComboIndex = 0;
		Montage = GetMontageForCombo(Type, ComboIndex);
		if (!Montage) return;
	}

	UAnimInstance* AnimInst = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	// Snap character toward camera-relative input direction so the player
	// can redirect combos mid-chain (DMC / FF16 style).
	RotateOwnerToInput();

	// Air combo hang -- DMC3 style progressive gravity per hit
	if (Type == EAttackType::EAT_Air)
	{
		if (UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
		{
			if (!bAirComboActive)
			{
				SavedGravityScale = Movement->GravityScale;
				bAirComboActive = true;
				AirHitCount = 0;
			}

			AirHitCount++;

			// First hit is near-zero gravity (full hang), each subsequent hit
			// adds more pull so by hit 4 you're noticeably sinking -- just like DMC3
			const float ScaledGravity = AirComboBaseGravity + (AirComboGravityPerHit * (AirHitCount - 1));
			Movement->GravityScale = FMath::Min(ScaledGravity, SavedGravityScale);

			// Snap vertical velocity so the character hangs instead of rising/falling
			FVector Vel = Movement->Velocity;
			Vel.Z = AirComboVerticalVelocitySnap;
			Movement->Velocity = Vel;
		}
	}

	AnimInst->Montage_Play(Montage, 1.f);

	bIsAttacking = true;
	bComboWindowOpen = false;
	bSavedNextAttack = false;
	CurrentAttackType = Type;
	ComboIndex++;

	PlaySwingSound();
}

UAnimMontage* UHSCombatComponent::GetMontageForCombo(EAttackType Type, int32 Index) const
{
	const TArray<UAnimMontage*>* List = nullptr;

	switch (Type)
	{
	case EAttackType::EAT_Light: List = &LightComboMontages; break;
	case EAttackType::EAT_Heavy: List = &HeavyComboMontages; break;
	case EAttackType::EAT_Air:   List = &AirComboMontages;   break;
	default: return nullptr;
	}

	if (!List->IsValidIndex(Index)) return nullptr;
	return (*List)[Index];
}

float UHSCombatComponent::GetDamageForCurrentAttack() const
{
	switch (CurrentAttackType)
	{
	case EAttackType::EAT_Heavy:  return HeavyDamage;
	case EAttackType::EAT_Air:    return AirDamage;
	case EAttackType::EAT_Rising: return RisingDamage;
	default:                      return LightDamage;
	}
}

void UHSCombatComponent::OpenComboWindow()
{
	bComboWindowOpen = true;

	if (bSavedNextAttack)
	{
		const EAttackType Next = BufferedAttackType;
		bSavedNextAttack = false;
		BufferedAttackType = EAttackType::EAT_None;
		PlayNextAttack(Next);
	}
}

void UHSCombatComponent::CloseComboWindow()
{
	bComboWindowOpen = false;
}

void UHSCombatComponent::OnAttackFinished()
{
	ResetCombo();
}

void UHSCombatComponent::CancelAttack()
{
	if (!bIsAttacking) return;

	if (OwnerChar && OwnerChar->GetMesh())
	{
		if (UAnimInstance* AnimInst = OwnerChar->GetMesh()->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.1f);
		}
	}

	ResetCombo();
}

void UHSCombatComponent::OnOwnerLanded()
{
	// Restore gravity if an air combo was active when we hit the ground
	if (bAirComboActive && OwnerChar)
	{
		if (UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
		{
			Movement->GravityScale = SavedGravityScale;
		}
		bAirComboActive = false;
	}
	AirHitCount = 0;
}

void UHSCombatComponent::ResetCombo()
{
	// Restore gravity if we were in an air combo
	if (bAirComboActive && OwnerChar)
	{
		if (UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
		{
			Movement->GravityScale = SavedGravityScale;
		}
		bAirComboActive = false;
	}

	bIsAttacking = false;
	bComboWindowOpen = false;
	bSavedNextAttack = false;
	CurrentAttackType = EAttackType::EAT_None;
	BufferedAttackType = EAttackType::EAT_None;
	ComboIndex = 0;
}

void UHSCombatComponent::RotateOwnerToInput()
{
	if (!OwnerChar) return;

	// If locked on, face the target instead of input direction
	if (AActor* Target = OwnerChar->GetLockedTarget())
	{
		const FVector Dir = (Target->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			OwnerChar->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
			return;
		}
	}

	// Grab the cached WASD / stick input from the player character
	const FVector2D Input = OwnerChar->GetMoveInputCached();
	if (Input.IsNearlyZero(0.1f)) return;  // no input -- keep current facing

	AController* PC = OwnerChar->GetController();
	if (!PC) return;

	const FRotator CamYaw(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::Y);

	const FVector WorldDir = (Forward * Input.Y + Right * Input.X).GetSafeNormal();
	if (WorldDir.IsNearlyZero()) return;

	const FRotator NewRot(0.f, WorldDir.Rotation().Yaw, 0.f);
	OwnerChar->SetActorRotation(NewRot);
}

void UHSCombatComponent::DoSwordTrace()
{
	if (!OwnerChar) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Start = OwnerChar->GetActorLocation() + OwnerChar->GetActorForwardVector() * 50.f;
	const FVector End = Start + OwnerChar->GetActorForwardVector() * TraceRange;

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(OwnerChar);

	TArray<FHitResult> Hits;
	const EDrawDebugTrace::Type DrawMode = bDebugDrawTrace ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	const bool bHit = UKismetSystemLibrary::SphereTraceMulti(
		World,
		Start,
		End,
		TraceRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn),
		false,
		IgnoreActors,
		DrawMode,
		Hits,
		true
	);

	if (!bHit) return;

	TSet<AActor*> AlreadyHit;
	const float Damage = GetDamageForCurrentAttack();
	bool bLandedHit = false;

	const FVector HitDir = OwnerChar->GetActorForwardVector();

	// Determine hit weight based on attack type
	EHitWeight HitWeight = EHitWeight::EHW_Light;
	if (CurrentAttackType == EAttackType::EAT_Heavy)
	{
		HitWeight = EHitWeight::EHW_Heavy;
	}
	else if (CurrentAttackType == EAttackType::EAT_Rising)
	{
		HitWeight = EHitWeight::EHW_Launcher;
	}

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || AlreadyHit.Contains(HitActor)) continue;
		AlreadyHit.Add(HitActor);

		if (HitActor->Implements<UHSDamageable>())
		{
			IHSDamageable::Execute_ApplyDamageEx(HitActor, Damage, OwnerChar, HitDir, HitWeight);

			if (UHSStyleComponent* Style = OwnerChar->GetStyle())
			{
				Style->RegisterHit(Damage);
			}

			bLandedHit = true;
		}
	}

	// Rising attack: launch the player into the air alongside the enemy
	if (bLandedHit && CurrentAttackType == EAttackType::EAT_Rising)
	{
		OwnerChar->LaunchCharacter(FVector(0.f, 0.f, RisingLaunchForce), false, true);
	}

	// Hit SFX
	if (bLandedHit)
	{
		PlayHitSound();
	}

	// Screen flash + time dilation on rising/launcher hits only
	if (bLandedHit && CurrentAttackType == EAttackType::EAT_Rising)
	{
		ApplyScreenHitEffect();
	}

	// Camera shake on hit (FF16 style impact feel)
	if (bLandedHit && HitCameraShake)
	{
		float ShakeScale = LightHitShakeScale;
		if (CurrentAttackType == EAttackType::EAT_Heavy) ShakeScale = HeavyHitShakeScale;
		else if (CurrentAttackType == EAttackType::EAT_Air) ShakeScale = AirHitShakeScale;

		if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
		{
			PC->ClientStartCameraShake(HitCameraShake, ShakeScale);
		}
	}
}

void UHSCombatComponent::PlaySwingSound()
{
	if (!OwnerChar) return;

	USoundBase* Sound = nullptr;
	switch (CurrentAttackType)
	{
	case EAttackType::EAT_Light:  Sound = LightSwingSound;  break;
	case EAttackType::EAT_Heavy:  Sound = HeavySwingSound;  break;
	case EAttackType::EAT_Air:    Sound = AirSwingSound;    break;
	case EAttackType::EAT_Rising: Sound = RisingSwingSound; break;
	default: break;
	}

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, OwnerChar->GetActorLocation());
	}
}

void UHSCombatComponent::PlayHitSound()
{
	if (!OwnerChar) return;

	USoundBase* Sound = nullptr;
	switch (CurrentAttackType)
	{
	case EAttackType::EAT_Light:  Sound = LightHitSound;  break;
	case EAttackType::EAT_Heavy:  Sound = HeavyHitSound;  break;
	case EAttackType::EAT_Air:    Sound = AirHitSound;    break;
	case EAttackType::EAT_Rising: Sound = RisingHitSound; break;
	default: break;
	}

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, OwnerChar->GetActorLocation());
	}
}

void UHSCombatComponent::ApplyScreenHitEffect()
{
	if (!OwnerChar) return;

	APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController());
	if (!PC) return;

	// White screen flash (FF16 style)
	if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
	{
		CamMgr->StartCameraFade(HeavyHitFlashIntensity, 0.f, HeavyHitFlashDuration, FLinearColor::White, false, true);
	}

	// Brief time dilation for dramatic impact (DMC3/FF16 style)
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, HitTimeDilationScale);

		World->GetTimerManager().ClearTimer(TimeDilationHandle);
		World->GetTimerManager().SetTimer(TimeDilationHandle, this, &UHSCombatComponent::RestoreTimeDilation, HitTimeDilationDuration, false);
	}
}

void UHSCombatComponent::RestoreTimeDilation()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
}
