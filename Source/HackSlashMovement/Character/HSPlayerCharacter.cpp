#include "HSPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "Combat/HSCombatComponent.h"
#include "Combat/HSStyleComponent.h"
#include "Combat/HSDynamicCameraComponent.h"
#include "UI/HSStyleHUD.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Character/HSDummyEnemy.h"
#include "Combat/HSHomingProjectile.h"
#include "UI/HSLockOnReticle.h"
#include "Components/WidgetComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "DrawDebugHelpers.h"
#include "Components/AudioComponent.h"

AHSPlayerCharacter::AHSPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

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

	// Double jump
	JumpMaxCount = 2;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->SocketOffset = FVector(0.f, 40.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

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
	DynamicCamera = CreateDefaultSubobject<UHSDynamicCameraComponent>(TEXT("DynamicCamera"));

	BGMAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BGMAudio"));
	BGMAudio->SetupAttachment(RootComponent);
	BGMAudio->bAutoActivate = false;
	BGMAudio->bAllowSpatialization = false;
	BGMAudio->bIsUISound = false;

	CombatBGMAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CombatBGMAudio"));
	CombatBGMAudio->SetupAttachment(RootComponent);
	CombatBGMAudio->bAutoActivate = false;
	CombatBGMAudio->bAllowSpatialization = false;
	CombatBGMAudio->bIsUISound = false;
}

void AHSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	CurrentMP = MaxMP;
	CurrentXP = 0.f;
	XPToNextLevel = BaseXPToLevel;

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

		if (StyleHUDClass)
		{
			UUserWidget* HUD = CreateWidget<UUserWidget>(PC, StyleHUDClass);
			if (HUD)
			{
				HUD->AddToViewport();
			}
		}

		if (PlayerHUDClass)
		{
			UUserWidget* HUD = CreateWidget<UUserWidget>(PC, PlayerHUDClass);
			if (HUD)
			{
				HUD->AddToViewport();
			}
		}
	}

	// Kick off the BGM loop -- either fade in or start at full volume
	if (BGMAudio && BGMTrack)
	{
		BGMAudio->SetSound(BGMTrack);
		if (BGMFadeInDuration > 0.f)
		{
			BGMAudio->FadeIn(BGMFadeInDuration, BGMVolume);
		}
		else
		{
			BGMAudio->SetVolumeMultiplier(BGMVolume);
			BGMAudio->Play();
		}
	}

	// Register the combat track so it's ready -- don't play yet, FadeIn will start it
	if (CombatBGMAudio && CombatBGMTrack)
	{
		CombatBGMAudio->SetSound(CombatBGMTrack);
	}

	// Proximity check every 0.75s -- cheap enough to not care about, avoids per-frame overlap queries
	GetWorldTimerManager().SetTimer(
		CombatMusicCheckHandle,
		this,
		&AHSPlayerCharacter::UpdateCombatMusicState,
		0.75f,
		true   // looping
	);
}

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

			// DmC 2013 pattern: when either combatant is airborne, bias focus more toward the target
			// so the enemy stays framed during juggles. Ground duels use the standard bias.
			const bool bPlayerFalling = GetCharacterMovement()->IsFalling();
			const bool bTargetFalling = false;  // placeholder -- could check target's falling state if it's a character
			const bool bAnyoneAirborne = bPlayerFalling || bTargetFalling;
			const float EffectiveFocusBias = bAnyoneAirborne ? LockOnFocusBiasAir : LockOnFocusBias;

			// DMC3 pattern: focus point between both characters horizontally,
			// but at a FIXED HEIGHT above the HIGHER of the two characters.
			// This prevents the camera from going under terrain on slopes.
			const float HigherZ = FMath::Max(MyLoc.Z, EnemyLoc.Z);

			// During a cinematic hero shot the camera is deliberately positioned low and
			// the shot defines its own framing, so we pull the focus point DOWN toward
			// target chest height. Otherwise the normal high focus forces a huge upward
			// pitch that slams into the min-camera-height safety and points the camera
			// at the ground.
			const float CineWForFocus = DynamicCamera ? DynamicCamera->GetCinematicBlendWeight() : 0.f;
			const float EffectiveFocusHeight = FMath::Lerp(LockOnFocusHeight, 0.f, CineWForFocus);

			FVector FocusPoint;
			FocusPoint.X = FMath::Lerp(MyLoc.X, EnemyLoc.X, EffectiveFocusBias);
			FocusPoint.Y = FMath::Lerp(MyLoc.Y, EnemyLoc.Y, EffectiveFocusBias);
			FocusPoint.Z = HigherZ + EffectiveFocusHeight;

			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				// Calculate look direction from the camera's approximate world position
				// (behind and above the player) toward the focus point
				FRotator LookAt = (FocusPoint - MyLoc).Rotation();
				LookAt.Pitch += LockOnPitchOffset;

				// Clamp pitch so camera never goes under the action
				LookAt.Pitch = FMath::Clamp(LookAt.Pitch, LockOnPitchMin, LockOnPitchMax);

				// DMC3 safety: if the resulting camera position would be below the player,
				// push the pitch back up. Calculate where the camera would end up.
				// SKIP this entirely when a cinematic shot is active -- the shot defines
				// its own deliberate low-angle framing and this safety would force pitch
				// to -90 (straight down at the ground) when the shot pulls the camera low.
				if (CineWForFocus < 0.1f)
				{
					const float ArmLen = CameraBoom ? CameraBoom->TargetArmLength : 500.f;
					const FVector CamOffset = CameraBoom ? CameraBoom->SocketOffset : FVector::ZeroVector;
					const float CamZ = MyLoc.Z + CamOffset.Z - ArmLen * FMath::Sin(FMath::DegreesToRadians(LookAt.Pitch));

					if (CamZ < MyLoc.Z + LockOnMinCameraHeight)
					{
						// Solve for the pitch that keeps the camera at minimum height
						const float NeededSin = (MyLoc.Z + CamOffset.Z - (MyLoc.Z + LockOnMinCameraHeight)) / ArmLen;
						LookAt.Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(NeededSin, -1.f, 1.f)));
					}
				}

				// DmC 2013 soft-lock: only interp pitch (and roll), keep player's yaw so the right stick
				// can still orbit freely around the target. Hard lock (default) snaps yaw to target too.
				const FRotator Current = PC->GetControlRotation();
				FRotator TargetRot = LookAt;
				if (bSoftLockYaw)
				{
					TargetRot.Yaw = Current.Yaw;
				}

				const FRotator Interped = FMath::RInterpTo(Current, TargetRot, DeltaTime, LockOnInterpSpeed);
				PC->SetControlRotation(Interped);
			}

			// DMC3 pattern: separate arm lengths for ground vs air.
			DesiredArmLength = bPlayerFalling ? LockOnArmLengthAir : LockOnArmLengthGround;
		}
	}

	// Smooth interpolation for camera offset and arm length (lock-on transitions + ground/air).
	// DynamicCamera's additive offsets (kill cam pull-in, group pull-back) are folded into
	// DesiredArmLength here so the existing interp handles the easing naturally.
	// Cinematic shot (from anim notify) blends from normal framing toward specific hero-shot
	// values weighted by the component's internal blend timer.
	if (CameraBoom)
	{
		const float DynArmOffset = DynamicCamera ? DynamicCamera->GetArmLengthOffset() : 0.f;
		const float CineW        = DynamicCamera ? DynamicCamera->GetCinematicBlendWeight() : 0.f;

		float TargetArm = DesiredArmLength + DynArmOffset;
		FVector TargetSocket = DesiredCameraOffset;

		if (CineW > 0.f && DynamicCamera)
		{
			TargetArm    = FMath::Lerp(TargetArm,    DynamicCamera->GetCinematicArmLength(),   CineW);
			TargetSocket = FMath::Lerp(TargetSocket, DynamicCamera->GetCinematicSocketOffset(), CineW);
		}

		CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocket, DeltaTime, LockOnArmInterpSpeed);
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaTime, LockOnArmInterpSpeed);
	}

	// Apply dynamic camera rotational effects (trauma shake, velocity tilt, launch pitch).
	if (DynamicCamera && FollowCamera)
	{
		FollowCamera->SetRelativeRotation(DynamicCamera->GetCameraRotationOffset());
	}

	// DMC-style FOV compression: zoom in slightly per hit, ease back passively
	if (Combat)
	{
		Combat->UpdateFOVCompression(DeltaTime);
	}

	// Cinematic FOV override: layered AFTER combat's FOV compression so hero-shot FOV wins
	// during the blend. Lerps from current (hit-compression-modulated) FOV toward cinematic FOV
	// by the cinematic blend weight, so it never snaps.
	if (DynamicCamera && FollowCamera)
	{
		const float CineW   = DynamicCamera->GetCinematicBlendWeight();
		const float CineFOV = DynamicCamera->GetCinematicFOV();
		if (CineW > 0.f && CineFOV > 0.f)
		{
			FollowCamera->FieldOfView = FMath::Lerp(FollowCamera->FieldOfView, CineFOV, CineW);
		}
	}
}

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
		if (lockOnSwitchInputAction)
		{
			enhancedInputComp->BindAction(lockOnSwitchInputAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::SwitchLockOnFromInput);
		}
		enhancedInputComp->BindAction(projectileInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::FireProjectile);
		enhancedInputComp->BindAction(pullInputAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::PullEnemy);
	}
}

void AHSPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	AirDodgesUsed = 0;
	bIsDoubleJumping = false;

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

void AHSPlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	// JumpCurrentCount is incremented by ACharacter::CheckJumpInput before this fires.
	// Second jump (air jump) → flip the flag so the ABP can transition to the double-jump state.
	if (JumpCurrentCount >= 2)
	{
		bIsDoubleJumping = true;
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

		// Combine both input axes into a single world-space movement vector so we can
		// sanitize it (cancel toward-enemy push) before applying.
		FVector WorldMove = ForwardDir * input.Y + RightDir * input.X;

		// DMC3/FF16 lock-on "pocket": when close to the locked target, kill the
		// component of input that pushes the player INTO the enemy capsule. Keeps
		// strafe + back-up working normally, just prevents capsule-on-capsule jitter.
		if (LockedTarget && !GetCharacterMovement()->IsFalling())
		{
			FVector ToEnemy = LockedTarget->GetActorLocation() - GetActorLocation();
			ToEnemy.Z = 0.f;
			const float Dist = ToEnemy.Size();

			if (Dist > KINDA_SMALL_NUMBER)
			{
				const FVector ToEnemyDir = ToEnemy / Dist;
				const float ClosingAmount = FVector::DotProduct(WorldMove, ToEnemyDir);

				if (ClosingAmount > 0.f)
				{
					// Inside MinDistance: fully cancel the closing component.
					// In the buffer zone just outside: scale it down linearly.
					float ClosingScale = 1.f;
					if (Dist < LockOnMinDistance)
					{
						ClosingScale = 0.f;
					}
					else if (LockOnApproachBuffer > 0.f && Dist < LockOnMinDistance + LockOnApproachBuffer)
					{
						ClosingScale = (Dist - LockOnMinDistance) / LockOnApproachBuffer;
					}

					// Remove the closing portion, re-add the scaled-down version.
					WorldMove -= ToEnemyDir * ClosingAmount;
					WorldMove += ToEnemyDir * ClosingAmount * ClosingScale;
				}
			}
		}

		AddMovementInput(WorldMove);
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
	else if (LockedTarget && IsBackTiltInput())
	{
		// DMC3 High Time: locked on + back tilt + light attack = rising attack
		Combat->TryRisingAttack();
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

void AHSPlayerCharacter::SwitchLockOnFromInput(const FInputActionValue& InputValue)
{
	if (!LockedTarget) return;  // only active while already locked on

	const FVector2D FlickRaw = InputValue.Get<FVector2D>();
	if (FlickRaw.Size() < LockOnSwitchFlickThreshold) return;

	// Cooldown so one flick = one switch (otherwise right-stick hold would chew through enemies)
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastLockOnSwitchTime < LockOnSwitchCooldown) return;

	AActor* NewTarget = FindLockOnTargetInDirection(FlickRaw);
	if (!NewTarget || NewTarget == LockedTarget) return;

	// Swap reticle to the new target
	HideLockOnReticle();
	LockedTarget = NewTarget;
	ShowLockOnReticle(LockedTarget);

	LastLockOnSwitchTime = Now;
}

AActor* AHSPlayerCharacter::FindLockOnTargetInDirection(const FVector2D& FlickDir) const
{
	if (!LockedTarget || !FollowCamera) return nullptr;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(const_cast<AHSPlayerCharacter*>(this));
	IgnoreActors.Add(LockedTarget);  // exclude the current target

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

	if (FoundActors.Num() == 0) return nullptr;

	// Project each candidate into camera-space screen coords. Compare against the
	// current locked target's screen position. Pick the one that best matches the
	// flick direction in screen space.
	const FVector CamRight   = FollowCamera->GetRightVector();
	const FVector CamForward = FollowCamera->GetForwardVector();
	const FVector CamLoc     = FollowCamera->GetComponentLocation();

	// Reference: current target's projected right-axis and forward-axis coords
	const FVector ToCurrent  = LockedTarget->GetActorLocation() - CamLoc;
	const float CurrentRight = FVector::DotProduct(ToCurrent, CamRight);
	const float CurrentFwd   = FVector::DotProduct(ToCurrent, CamForward);

	// Screen-space: X = right, Y = forward-depth. Flick.X positive = right, Flick.Y positive = up.
	// We want the candidate whose delta-right has the same sign as FlickDir.X (if non-zero)
	// and whose screen distance from the current target is small -- closest in flick direction wins.
	const FVector2D FlickNorm = FlickDir.GetSafeNormal();

	AActor* Best = nullptr;
	float BestScore = -1.f;

	for (AActor* Actor : FoundActors)
	{
		if (!Actor || Actor->IsActorBeingDestroyed()) continue;

		const FVector ToActor = Actor->GetActorLocation() - CamLoc;
		const float ActorFwd  = FVector::DotProduct(ToActor, CamForward);
		if (ActorFwd < 50.f) continue;  // behind the camera (or nearly so)

		const float ActorRight = FVector::DotProduct(ToActor, CamRight);

		// Delta in screen-right axis, normalized by reference distance so it's framerate / scale independent
		const float DeltaRight = ActorRight - CurrentRight;
		const float DeltaFwd   = ActorFwd   - CurrentFwd;

		const FVector2D DeltaScreen(DeltaRight, -DeltaFwd);  // -DeltaFwd so "further" is up (positive Y)
		if (DeltaScreen.IsNearlyZero()) continue;

		const FVector2D DeltaNorm = DeltaScreen.GetSafeNormal();
		const float DirScore = FVector2D::DotProduct(DeltaNorm, FlickNorm);
		if (DirScore < 0.2f) continue;  // not in the flick direction

		// Prefer closer-in-flick-direction targets
		const float Dist = DeltaScreen.Size();
		const float Score = DirScore * 10000.f - Dist;

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
	if (!ProjectileClass) return;
	if (CurrentMP < ProjectileMPCost) return;

	CurrentMP = FMath::Max(0.f, CurrentMP - ProjectileMPCost);

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
	if (Combat && Combat->IsAttacking())
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

	// Cast sound
	if (ProjectileCastSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ProjectileCastSound, GetActorLocation());
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

	// Dodging only rewards style points if used mid-combat (cancel dodge)
	const bool bWasAttacking = Combat && Combat->IsAttacking();

	// Dodge cancels active attack montages
	if (bWasAttacking)
	{
		Combat->CancelAttack();
	}

	bIsDodging = true;
	bDodgeRecoveryActive = false;

	if (bWasAttacking && Style)
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

bool AHSPlayerCharacter::IsBackTiltInput() const
{
	if (!LockedTarget || !Controller) return false;
	if (moveInputCached.IsNearlyZero(0.1f)) return false;

	// Resolve the stick input into a world direction relative to the camera
	const FRotator CamYaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::Y);
	const FVector InputWorldDir = (Forward * moveInputCached.Y + Right * moveInputCached.X).GetSafeNormal();

	// Direction from player toward the locked target
	FVector ToTarget = LockedTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	ToTarget.Normalize();

	// If the input direction is roughly opposite the target direction, it's a back-tilt
	const float Dot = FVector::DotProduct(InputWorldDir, ToTarget);
	return Dot < -0.5f;
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

void AHSPlayerCharacter::PlayFootstep(FName FootBone)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	// Start slightly ABOVE the foot bone so a sprint stride that dips the foot below
	// a thin surface (water plane) still traces down THROUGH that surface instead of
	// starting under it and missing.
	const FVector FootLoc = MeshComp->GetBoneLocation(FootBone);
	const FVector Start = FootLoc + FVector(0.f, 0.f, FootstepTraceStartHeight);
	const FVector End = FootLoc - FVector(0.f, 0.f, FootstepTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HSFootstep), /*bTraceComplex=*/true, this);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// If we didn't find the ground below the foot, fall back to the foot position itself
	const FVector ImpactLoc = bHit ? Hit.ImpactPoint : Start;

	// Default to SurfaceType_Default; override if the trace hit a surface with a physical material
	EPhysicalSurface Surface = SurfaceType_Default;
	if (bHit && Hit.PhysMaterial.IsValid())
	{
		Surface = Hit.PhysMaterial->SurfaceType;
	}

#if !UE_BUILD_SHIPPING
	// Temporary diagnostic: visualize the trace and log what surface we read.
	DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 1.f);
	const FString HitActorName = bHit && Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("None");
	const FString PhysMatName = bHit && Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->GetName() : TEXT("None");
	UE_LOG(LogTemp, Warning, TEXT("[Footstep] Bone=%s Hit=%s Actor=%s PhysMat=%s Surface=%d"),
		*FootBone.ToString(),
		bHit ? TEXT("true") : TEXT("false"),
		*HitActorName,
		*PhysMatName,
		static_cast<int32>(Surface));
#endif

	// Pick VFX -- per-surface map first, fall back to default
	UNiagaraSystem* VFXToPlay = nullptr;
	if (UNiagaraSystem* const* Found = SurfaceFootstepVFX.Find(Surface))
	{
		VFXToPlay = *Found;
	}
	if (!VFXToPlay)
	{
		VFXToPlay = DefaultFootstepVFX;
	}

	if (VFXToPlay)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			VFXToPlay,
			ImpactLoc,
			FRotator::ZeroRotator
		);
	}

	// Pick SFX -- per-surface pool first, fall back to default pool. Picks one at random.
	USoundBase* SFXToPlay = nullptr;
	if (const FHSFootstepSoundSet* Found = SurfaceFootstepSFX.Find(Surface))
	{
		if (Found->Sounds.Num() > 0)
		{
			const int32 Idx = FMath::RandRange(0, Found->Sounds.Num() - 1);
			SFXToPlay = Found->Sounds[Idx];
		}
	}
	if (!SFXToPlay && DefaultFootstepSFX.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, DefaultFootstepSFX.Num() - 1);
		SFXToPlay = DefaultFootstepSFX[Idx];
	}

	if (SFXToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SFXToPlay, ImpactLoc, FootstepVolumeMultiplier);
	}
}

void AHSPlayerCharacter::UpdateCombatMusicState()
{
	if (!CombatBGMTrack)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatMusic] CombatBGMTrack is not assigned on BP_HSPlayer -- assign it in Configurations|BGM"));
		return;
	}

	// Sphere overlap for living enemy pawns within CombatMusicRange
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);

	TArray<AActor*> FoundActors;
	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		GetActorLocation(),
		CombatMusicRange,
		ObjectTypes,
		AHSDummyEnemy::StaticClass(),
		IgnoreActors,
		FoundActors
	);

	// Filter -- only count living enemies
	const bool bEnemyNearby = FoundActors.ContainsByPredicate([](const AActor* A)
	{
		const AHSDummyEnemy* Enemy = Cast<AHSDummyEnemy>(A);
		return Enemy && !Enemy->IsDead();
	});

	UE_LOG(LogTemp, Log, TEXT("[CombatMusic] Tick — EnemiesFound=%d  bEnemyNearby=%d  bInCombatMusic=%d"),
		FoundActors.Num(), bEnemyNearby ? 1 : 0, bIsInCombatMusic ? 1 : 0);

	if (bEnemyNearby)
	{
		// Cancel any pending linger-out and switch to combat music immediately
		GetWorldTimerManager().ClearTimer(CombatLingerHandle);
		if (!bIsInCombatMusic)
		{
			EnterCombatMusic();
		}
	}
	else if (bIsInCombatMusic)
	{
		// No enemies nearby -- start the linger countdown if not already running
		if (!GetWorldTimerManager().IsTimerActive(CombatLingerHandle))
		{
			GetWorldTimerManager().SetTimer(
				CombatLingerHandle,
				this,
				&AHSPlayerCharacter::ExitCombatMusic,
				CombatMusicLingerTime,
				false
			);
		}
	}
}

void AHSPlayerCharacter::EnterCombatMusic()
{
	if (bIsInCombatMusic) return;
	bIsInCombatMusic = true;

	// Fade exploration track out
	if (BGMAudio && BGMAudio->IsPlaying())
	{
		BGMAudio->FadeOut(BGMCrossfadeDuration, 0.f);
	}

	// Start combat track from clean state and fade it in
	if (CombatBGMAudio)
	{
		CombatBGMAudio->SetVolumeMultiplier(1.f);
		CombatBGMAudio->FadeIn(BGMCrossfadeDuration, CombatBGMVolume);
	}
}

void AHSPlayerCharacter::ExitCombatMusic()
{
	if (!bIsInCombatMusic) return;
	bIsInCombatMusic = false;

	// Fade combat track out
	if (CombatBGMAudio && CombatBGMAudio->IsPlaying())
	{
		CombatBGMAudio->FadeOut(BGMCrossfadeDuration, 0.f);
	}

	// Restart exploration track from clean state and fade it back in
	if (BGMAudio && BGMTrack)
	{
		BGMAudio->SetVolumeMultiplier(1.f);
		BGMAudio->FadeIn(BGMCrossfadeDuration, BGMVolume);
	}
}

void AHSPlayerCharacter::PlayAttackGrunt(bool bIsHeavy)
{
	const TArray<USoundBase*>& Pool = bIsHeavy ? HeavyAttackGrunts : LightAttackGrunts;
	if (Pool.IsEmpty()) return;

	const int32 Idx = FMath::RandRange(0, Pool.Num() - 1);
	if (USoundBase* Sound = Pool[Idx])
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), GruntVolumeMultiplier);
	}
}

void AHSPlayerCharacter::AddXP(float Amount)
{
	if (Amount <= 0.f) return;

	CurrentXP += Amount;

	// Support levelling up multiple times from one large XP gain.
	while (CurrentXP >= XPToNextLevel && XPToNextLevel > 0.f)
	{
		CurrentXP -= XPToNextLevel;
		PlayerLevel++;
		// Each level requires progressively more XP.
		XPToNextLevel = FMath::RoundToFloat(BaseXPToLevel * FMath::Pow(XPScalePerLevel, static_cast<float>(PlayerLevel - 1)));
	}
}

void AHSPlayerCharacter::ReceiveEnemyAttack(float Damage)
{
	if (bIsInvincible || Damage <= 0.f) return;

	CurrentHealth = FMath::Max(0.f, CurrentHealth - Damage);

	// Add trauma -- drives the Perlin-noise camera shake.
	if (DynamicCamera)
	{
		DynamicCamera->AddTrauma(0.45f);
	}

	// Brief invincibility window so a single enemy attack can't multi-hit
	bIsInvincible = true;
	GetWorldTimerManager().ClearTimer(InvincibilityTimerHandle);
	GetWorldTimerManager().SetTimer(
		InvincibilityTimerHandle,
		FTimerDelegate::CreateLambda([this]() { bIsInvincible = false; }),
		HitInvincibilityDuration,
		false
	);

	// Red screen flash so the player knows they got hit
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
		{
			CamMgr->StartCameraFade(0.25f, 0.f, 0.2f, FLinearColor(1.f, 0.f, 0.f), false, true);
		}

		// Camera shake
		if (HitReceiveCameraShake)
		{
			PC->ClientStartCameraShake(HitReceiveCameraShake, HitReceiveShakeScale);
		}
	}
}
