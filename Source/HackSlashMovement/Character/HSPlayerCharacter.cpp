// Fill out your copyright notice in the Description page of Project Settings.


#include "HSPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "Combat/HSCombatComponent.h"
#include "Combat/HSStyleComponent.h"
#include "UI/HSStyleHUD.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Character/HSDummyEnemy.h"
#include "Combat/HSHomingProjectile.h"
#include "UI/HSLockOnReticle.h"
#include "Components/WidgetComponent.h"

// Sets default values
AHSPlayerCharacter::AHSPlayerCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 1080.f, 0.f);

	// Snappy ground feel
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->MaxAcceleration = 4096.f;
	Movement->BrakingDecelerationWalking = 4096.f;
	Movement->GroundFriction = 10.f;
	Movement->BrakingFrictionFactor = 2.f;

	// DMC / FF16 style air control
	Movement->JumpZVelocity = 700.f;
	Movement->AirControl = 0.9f;
	Movement->AirControlBoostMultiplier = 2.f;
	Movement->AirControlBoostVelocityThreshold = 25.f;
	Movement->FallingLateralFriction = 1.5f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->SocketOffset = FVector(0.f, 40.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("Weapon_R"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetComponentTickEnabled(false);
	WeaponMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Combat = CreateDefaultSubobject<UHSCombatComponent>(TEXT("Combat"));
	Style = CreateDefaultSubobject<UHSStyleComponent>(TEXT("Style"));
}

// Called when the game starts or when spawned
void AHSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (inputMapping)
			{
				Subsystem->AddMappingContext(inputMapping, 0);
			}
		}

		// Cache initial camera values for lerp transitions
		if (CameraBoom)
		{
			DesiredCameraOffset = CameraBoom->SocketOffset;
			DesiredArmLength = CameraBoom->TargetArmLength;
		}

		// Spawn the style HUD widget
		if (StyleHUDClass)
		{
			UUserWidget* HUD = CreateWidget<UUserWidget>(PC, StyleHUDClass);
			if (HUD)
			{
				HUD->AddToViewport();
			}
		}
	}
}

// Called every frame
void AHSPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Lock-on camera tracking (DMC3/FF16 style)
	if (LockedTarget)
	{
		// If the target is dead or too far, release lock and lerp camera back
		if (LockedTarget->IsActorBeingDestroyed() ||
			FVector::Dist(GetActorLocation(), LockedTarget->GetActorLocation()) > LockOnRange * 1.5f)
		{
			HideLockOnReticle();
			LockedTarget = nullptr;

			if (bCameraDefaultsSaved)
			{
				DesiredCameraOffset = DefaultCameraOffset;
				DesiredArmLength = DefaultArmLength;
			}
			// Fall through to interpolation below
		}
		else
		{
			const FVector MyLoc = GetActorLocation();
			const FVector EnemyLoc = LockedTarget->GetActorLocation();

			// DMC3 pattern: camera focus point uses horizontal position of both characters
			// but keeps a FIXED HEIGHT relative to the player -- never chases the enemy's Z.
			// This prevents the camera from tilting under during air combos.
			FVector FocusPoint;
			FocusPoint.X = FMath::Lerp(MyLoc.X, EnemyLoc.X, LockOnFocusBias);
			FocusPoint.Y = FMath::Lerp(MyLoc.Y, EnemyLoc.Y, LockOnFocusBias);
			FocusPoint.Z = MyLoc.Z + LockOnFocusHeight;  // Fixed height above player, not enemy

			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				FRotator LookAt = (FocusPoint - MyLoc).Rotation();
				LookAt.Pitch += LockOnPitchOffset;

				// Clamp pitch so camera never goes under the action or looks straight up
				LookAt.Pitch = FMath::Clamp(LookAt.Pitch, LockOnPitchMin, LockOnPitchMax);

				const FRotator Current = PC->GetControlRotation();
				const FRotator Interped = FMath::RInterpTo(Current, LookAt, DeltaTime, LockOnInterpSpeed);
				PC->SetControlRotation(Interped);
			}

			// DMC3 pattern: separate arm lengths for ground vs air.
			const bool bInAir = GetCharacterMovement()->IsFalling();
			DesiredArmLength = bInAir ? LockOnArmLengthAir : LockOnArmLengthGround;
		}
	}

	// Smooth interpolation for camera offset and arm length (lock-on transitions + ground/air)
	if (CameraBoom)
	{
		CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, DesiredCameraOffset, DeltaTime, LockOnArmInterpSpeed);
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, DesiredArmLength, DeltaTime, LockOnArmInterpSpeed);
	}
}

// Called to bind functionality to input
void AHSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* enhancedInputComp = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (enhancedInputComp)
	{
		enhancedInputComp->BindAction(moveInputAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::Move);
		enhancedInputComp->BindAction(moveInputAction, ETriggerEvent::Completed, this, &AHSPlayerCharacter::MoveCompleted);
		enhancedInputComp->BindAction(moveInputAction, ETriggerEvent::Canceled, this, &AHSPlayerCharacter::MoveCompleted);
		enhancedInputComp->BindAction(lookInputAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::Look);
		enhancedInputComp->BindAction(jumpInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::JumpCancel);
		enhancedInputComp->BindAction(jumpInputAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		enhancedInputComp->BindAction(sprintInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::StartSprint);
		enhancedInputComp->BindAction(sprintInputAction, ETriggerEvent::Completed, this, &AHSPlayerCharacter::StopSprint);
		enhancedInputComp->BindAction(lightAttackInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::LightAttack);
		enhancedInputComp->BindAction(heavyAttackInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HeavyAttack);
		enhancedInputComp->BindAction(dodgeInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::Dodge);
		enhancedInputComp->BindAction(lockOnInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleLockOn);
		enhancedInputComp->BindAction(projectileInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::FireProjectile);
		enhancedInputComp->BindAction(pullInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::PullEnemy);
	}
}

void AHSPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	AirDodgesUsed = 0;

	// Restore gravity and reset air hit count
	if (Combat)
	{
		Combat->OnOwnerLanded();
	}

	// If a dodge montage ended while we were still airborne (e.g. a forward
	// ground dodge with a tiny vertical root-motion hop), the bIsDodging flag
	// was held open until now. Clear it on touchdown so locomotion resumes.
	if (bIsDodging)
	{
		bIsDodging = false;
		bDodgeRecoveryActive = false;
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = true;
		}
	}
}

void AHSPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Re-attach the weapon to whatever socket name is set in defaults.
	// This runs in the editor (so the BP viewport updates) and at runtime.
	if (WeaponMesh && GetMesh() && !WeaponSocketName.IsNone())
	{
		WeaponMesh->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			WeaponSocketName
		);
	}
}

void AHSPlayerCharacter::Move(const FInputActionValue& InputValue)
{
	const FVector2D input = InputValue.Get<FVector2D>();
	moveInputCached = input;

	if (Combat && Combat->IsAttacking()) return;

	// Allow input to cancel the recovery tail of a dodge. Once the cancel has
	// fired, bDodgeRecoveryActive is held true through the blend-out so we
	// stop re-running the cancel attempt and just apply movement input.
	if (bIsDodging && !bDodgeRecoveryActive && !TryCancelDodgeFromInput(input)) return;

	// During dodge recovery, block movement while still airborne so the
	// character doesn't "run" mid-air from the dodge's root motion hop.
	if (bIsDodging && bDodgeRecoveryActive && GetCharacterMovement()->IsFalling()) return;

	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, input.Y);
		AddMovementInput(RightDir, input.X);
	}
}

void AHSPlayerCharacter::MoveCompleted(const FInputActionValue& InputValue)
{
	moveInputCached = FVector2D::ZeroVector;
}

void AHSPlayerCharacter::Look(const FInputActionValue& InputValue)
{
	FVector2D input = InputValue.Get<FVector2D>();
	AddControllerYawInput(input.X);
	AddControllerPitchInput(input.Y);
}

void AHSPlayerCharacter::StartSprint()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AHSPlayerCharacter::StopSprint()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AHSPlayerCharacter::LightAttack()
{
	if (bIsDodging) return;
	if (!Combat) return;

	if (GetCharacterMovement()->IsFalling())
	{
		Combat->TryAirAttack();
	}
	else
	{
		Combat->TryLightAttack();
	}
}

void AHSPlayerCharacter::HeavyAttack()
{
	if (bIsDodging) return;
	if (!Combat) return;

	if (GetCharacterMovement()->IsFalling())
	{
		Combat->TryAirAttack();
	}
	else
	{
		Combat->TryHeavyAttack();
	}
}

void AHSPlayerCharacter::ToggleLockOn()
{
	if (LockedTarget)
	{
		// Release lock -- lerp camera back to defaults
		HideLockOnReticle();
		LockedTarget = nullptr;

		if (bCameraDefaultsSaved)
		{
			DesiredCameraOffset = DefaultCameraOffset;
			DesiredArmLength = DefaultArmLength;
		}
	}
	else
	{
		LockedTarget = FindLockOnTarget();

		if (LockedTarget && CameraBoom)
		{
			// Save defaults on first lock
			if (!bCameraDefaultsSaved)
			{
				DefaultCameraOffset = CameraBoom->SocketOffset;
				DefaultArmLength = CameraBoom->TargetArmLength;
				bCameraDefaultsSaved = true;
			}

			// Set targets -- Tick will interpolate toward these
			DesiredCameraOffset = LockOnCameraOffset;
			DesiredArmLength = LockOnArmLengthGround;

			// Show the lock-on reticle on the target
			ShowLockOnReticle(LockedTarget);
		}
	}
}

AActor* AHSPlayerCharacter::FindLockOnTarget() const
{
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(const_cast<AHSPlayerCharacter*>(this));

	TArray<AActor*> FoundActors;
	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		GetActorLocation(),
		LockOnRange,
		ObjectTypes,
		AHSDummyEnemy::StaticClass(),
		IgnoreActors,
		FoundActors
	);

	// Find the closest target in front of the camera
	AActor* Best = nullptr;
	float BestScore = -1.f;
	const FVector CamForward = FollowCamera->GetForwardVector();
	const FVector MyLoc = GetActorLocation();

	for (AActor* Actor : FoundActors)
	{
		if (!Actor || Actor->IsActorBeingDestroyed()) continue;

		const FVector Dir = (Actor->GetActorLocation() - MyLoc).GetSafeNormal();
		const float Dot = FVector::DotProduct(CamForward, Dir);

		// Only targets roughly in front of the camera (within ~120 degree cone)
		if (Dot < -0.1f) continue;

		// Score: prefer targets closer to center of screen and closer distance
		const float Dist = FVector::Dist(MyLoc, Actor->GetActorLocation());
		const float Score = Dot * 1000.f - Dist;

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Actor;
		}
	}

	return Best;
}

void AHSPlayerCharacter::FireProjectile()
{
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireProjectile: No ProjectileClass set!"));
		return;
	}

	// If locked on, always home toward the locked target.
	// Otherwise, look for a nearby enemy -- if none found, fire straight forward.
	AActor* Target = LockedTarget;
	if (!Target)
	{
		Target = FindLockOnTarget();  // returns nullptr if nothing in range
	}

	// Spawn projectile from the left hand bone
	FVector SpawnLoc;
	if (GetMesh() && GetMesh()->DoesSocketExist(TEXT("hand_lSocket")))
	{
		SpawnLoc = GetMesh()->GetSocketLocation(TEXT("hand_lSocket"));
	}
	else if (GetMesh() && GetMesh()->DoesSocketExist(TEXT("hand_l")))
	{
		SpawnLoc = GetMesh()->GetSocketLocation(TEXT("hand_l"));
	}
	else
	{
		// Fallback to offset (left side)
		SpawnLoc = GetActorLocation()
			+ GetActorForwardVector() * ProjectileSpawnOffset.X
			- GetActorRightVector() * ProjectileSpawnOffset.Y
			+ FVector(0.f, 0.f, ProjectileSpawnOffset.Z);
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;

	AHSHomingProjectile* Proj = GetWorld()->SpawnActor<AHSHomingProjectile>(ProjectileClass, SpawnLoc, GetActorRotation(), Params);
	if (Proj)
	{
		// Target can be null -- projectile flies forward in that case
		Proj->FireAtTarget(Target, this, ProjectileDamage);
	}

	// Cancel active attacks first so the cast montage doesn't orphan AN_AttackFinished
	if (Combat->IsAttacking())
	{
		Combat->CancelAttack();
	}

	// Cast animation on UpperBody slot
	if (ProjectileCastMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(ProjectileCastMontage, 0.8f);
		}
	}

	// Camera shake on fire
	if (FireCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ClientStartCameraShake(FireCameraShake, FireShakeScale);
		}
	}
}

void AHSPlayerCharacter::PullEnemy()
{
	if (bIsDodging) return;

	// Cancel any active attack -- pull overrides everything like DMC/FF16
	if (Combat && Combat->IsAttacking())
	{
		Combat->CancelAttack();
	}

	// Find target -- locked target or nearest enemy in pull range
	AActor* Target = LockedTarget;
	if (!Target)
	{
		Target = FindLockOnTarget();
	}
	if (!Target) return;

	// Check pull range
	const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Dist > PullRange) return;

	// Face the target
	FVector LookDir = Target->GetActorLocation() - GetActorLocation();
	LookDir.Z = 0.f;
	if (!LookDir.IsNearlyZero())
	{
		SetActorRotation(LookDir.Rotation());
	}

	// Play pull animation on the player
	if (PullMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(PullMontage, 1.2f);
		}
	}

	// Launch the enemy toward the player
	if (ACharacter* EnemyChar = Cast<ACharacter>(Target))
	{
		FVector PullDir = GetActorLocation() - Target->GetActorLocation();
		PullDir.Z = 0.f;
		PullDir.Normalize();

		const FVector PullVelocity = PullDir * PullForce + FVector(0.f, 0.f, PullLift);
		EnemyChar->LaunchCharacter(PullVelocity, true, true);
	}

	// Camera shake
	if (PullCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ClientStartCameraShake(PullCameraShake, PullShakeScale);
		}
	}
}

void AHSPlayerCharacter::JumpCancel()
{
	// If in the air during an attack, cancel the attack and do a small hop
	// This is core DMC tech -- lets you extend air combos indefinitely with skill
	if (GetCharacterMovement()->IsFalling() && Combat && Combat->IsAttacking())
	{
		Combat->CancelAttack();

		// Small upward boost to reset the air combo arc
		LaunchCharacter(FVector(0.f, 0.f, GetCharacterMovement()->JumpZVelocity * 0.6f), false, true);
		return;
	}

	// Normal jump on the ground
	Jump();
}

void AHSPlayerCharacter::Dodge()
{
	if (bIsDodging) return;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement) return;

	const bool bInAir = Movement->IsFalling();
	if (bInAir && AirDodgesUsed >= MaxAirDodges) return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	const bool bHasInput = !moveInputCached.IsNearlyZero(0.1f);

	// Phoenix Shift style: with input, snap character to face the camera-relative
	// input direction and play the forward-dodge anim. Without input, play the
	// backstep anim and keep the current facing.
	UAnimMontage* Montage = GetDodgeMontage(bHasInput ? 0 : 1);
	if (!Montage) return;

	// Dodge cancels active attack montages
	if (Combat && Combat->IsAttacking())
	{
		Combat->CancelAttack();
	}

	bIsDodging = true;
	bDodgeRecoveryActive = false;

	// Dodging rewards style points
	if (Style)
	{
		Style->RegisterDodge();
	}

	// Stop the movement comp from yanking the character back toward residual velocity
	// while we snap rotation. Restored when the montage ends.
	Movement->bOrientRotationToMovement = false;

	FVector DodgeWorldDir = GetActorForwardVector();

	if (bHasInput)
	{
		DodgeWorldDir = ResolveCameraRelativeInputDirection();

		const FRotator NewRot = DodgeWorldDir.Rotation();
		SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
	}
	else
	{
		// Backstep travels opposite the character's facing
		DodgeWorldDir = -GetActorForwardVector();
	}

	if (bInAir)
	{
		AirDodgesUsed++;

		// Air dodge gets a launch impulse so it feels like a real dash
		const FVector Launch = DodgeWorldDir * AirDodgeLaunchSpeed + FVector(0.f, 0.f, AirDodgeVerticalLift);
		LaunchCharacter(Launch, true, true);
	}

	AnimInst->Montage_Play(Montage, DodgePlayRate);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AHSPlayerCharacter::OnDodgeMontageEnded);
	AnimInst->Montage_SetEndDelegate(EndDelegate, Montage);
}

FVector AHSPlayerCharacter::ResolveCameraRelativeInputDirection() const
{
	if (!Controller) return GetActorForwardVector();

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	const FVector WorldDir = (ForwardDir * moveInputCached.Y) + (RightDir * moveInputCached.X);
	return WorldDir.GetSafeNormal();
}

UAnimMontage* AHSPlayerCharacter::GetDodgeMontage(int32 Index) const
{
	if (!DodgeMontages.IsValidIndex(Index)) return nullptr;
	return DodgeMontages[Index];
}

void AHSPlayerCharacter::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// If we're still airborne when the dodge montage ends, hold bIsDodging
	// open so the locomotion state machine doesn't snap into JumpStart for the
	// brief tail of falling. Landed() will clear it.
	if (Movement && Movement->IsFalling())
	{
		return;
	}

	bIsDodging = false;
	bDodgeRecoveryActive = false;

	if (Movement)
	{
		Movement->bOrientRotationToMovement = true;
	}
}

bool AHSPlayerCharacter::TryCancelDodgeFromInput(const FVector2D& Input)
{
	if (Input.IsNearlyZero(0.1f)) return false;

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInst) return false;

	UAnimMontage* Active = AnimInst->GetCurrentActiveMontage();
	if (!Active) return false;

	const float Position = AnimInst->Montage_GetPosition(Active);
	const float Length = Active->GetPlayLength();
	if (Length <= 0.f) return false;

	const float Fraction = Position / Length;
	if (Fraction < DodgeCancelAfterFraction) return false;

	AnimInst->Montage_Stop(DodgeCancelBlendOut, Active);

	// Mark recovery active so this same Move() call (and subsequent ones during
	// the blend out) can apply movement input. We deliberately leave bIsDodging
	// set so the locomotion state machine doesn't snap into JumpStart from any
	// leftover Z root motion in the dodge anim. OnDodgeMontageEnded clears
	// bIsDodging when the blend-out actually completes.
	bDodgeRecoveryActive = true;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
	}

	return true;
}

void AHSPlayerCharacter::ShowLockOnReticle(AActor* Target)
{
	if (!Target || !LockOnReticleClass) return;

	// Remove any existing reticle
	HideLockOnReticle();

	// Create a widget component and attach it to the target
	LockOnReticleComp = NewObject<UWidgetComponent>(Target);
	LockOnReticleComp->SetWidgetSpace(EWidgetSpace::Screen);
	LockOnReticleComp->SetDrawAtDesiredSize(true);
	LockOnReticleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LockOnReticleComp->SetGenerateOverlapEvents(false);
	LockOnReticleComp->SetWidgetClass(LockOnReticleClass);
	LockOnReticleComp->SetRelativeLocation(FVector(0.f, 0.f, LockOnReticleHeightOffset));
	LockOnReticleComp->RegisterComponent();
	LockOnReticleComp->AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	// Trigger the acquired animation if the widget supports it
	if (UHSLockOnReticle* Reticle = Cast<UHSLockOnReticle>(LockOnReticleComp->GetWidget()))
	{
		Reticle->PlayLockOnAcquired();
	}
}

void AHSPlayerCharacter::HideLockOnReticle()
{
	if (LockOnReticleComp)
	{
		// Trigger the released animation
		if (UHSLockOnReticle* Reticle = Cast<UHSLockOnReticle>(LockOnReticleComp->GetWidget()))
		{
			Reticle->PlayLockOnReleased();
		}

		LockOnReticleComp->DestroyComponent();
		LockOnReticleComp = nullptr;
	}
}
