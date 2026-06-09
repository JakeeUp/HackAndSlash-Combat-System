#include "HSCombatComponent.h"

#include "Character/HSPlayerCharacter.h"
#include "Character/HSDummyEnemy.h"
#include "Combat/HSDamageable.h"
#include "Combat/HSStyleComponent.h"
#include "Combat/HSDynamicCameraComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
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

	// Cache the camera's default FOV so UpdateFOVCompression can restore it correctly
	if (OwnerChar && OwnerChar->FollowCamera)
	{
		DefaultCameraFOV = OwnerChar->FollowCamera->FieldOfView;
	}

	// Cache the authoritative gravity baseline ONCE, before any combat system can modify it.
	// All restore paths target this value -- never a "saved before override" snapshot, which
	// previously leaked whenever air-combo and hold-loop stacked their overrides.
	if (OwnerChar)
	{
		if (const UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
		{
			DefaultGravityScale = Movement->GravityScale;
		}
	}
}

void UHSCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Paranoia gravity restore in case the component is torn down mid-override (e.g. character
	// destroyed while air-holding).  Clear both override flags so the helper actually restores.
	// Also kills any pending time-dilation timer so it can't fire on a destroyed object.
	bAirComboActive      = false;
	bHoldGravityApplied  = false;
	RestoreGravityIfOverridden();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UHSCombatComponent::RestoreGravityIfOverridden()
{
	if (!OwnerChar) return;

	// Coordinated restore: only snap gravity back to default when NEITHER override system is
	// currently holding it.  Each caller is responsible for clearing its own flag (bAirComboActive
	// for air-combo, bHoldGravityApplied for hold-loop) BEFORE calling this helper.  If the other
	// system is still holding, we leave gravity at its value so the two don't stomp each other
	// when they overlap (e.g. hold-loop interrupted by an air-combo swing -- hold clears its flag,
	// but the combo's own gravity level needs to stay in effect until the combo ends).
	if (bAirComboActive || bHoldGravityApplied) return;

	if (UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
	{
		Movement->GravityScale = DefaultGravityScale;
	}
	AirHitCount = 0;
}

void UHSCombatComponent::TryLightAttack()
{
	if (!bIsAttacking)
	{
		ComboIndex = 0;
		// DMC-Stinger / FF16 pattern: if the forward-tilt branch has montages configured AND
		// the player's stick was pushed forward at press-time, lock this string to the forward
		// chain.  Decision is made ONCE per string (matches the delayed-branch contract) -- once
		// locked, the whole combo reads as a single cohesive attack string.
		bOnForwardBranch = Tweakables.Montages.LightForwardCombo.Num() > 0 && IsInputTiltForward();
		PlayNextAttack(EAttackType::EAT_Light);
		return;
	}

	if (bComboWindowOpen)
	{
		// Normal combo chain -- next light in Tweakables.Montages.LightCombo.
		PlayNextAttack(EAttackType::EAT_Light);
	}
	else if (bDelayedWindowOpen
		&& CurrentAttackType == EAttackType::EAT_Light
		&& Tweakables.Montages.LightDelayedCombo.Num() > 0)
	{
		// Delayed-input branch: the player held the press until past the normal combo window
		// but inside the explicit delayed window placed on the anim.  Switch combo lists and
		// continue the string from the same index.
		bOnDelayedBranch = true;
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
	if (!OwnerChar || !Tweakables.Montages.RisingAttack) return;

	// Cancel any active attack
	if (bIsAttacking)
	{
		CancelAttack();
	}

	UAnimInstance* AnimInst = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	// Face the locked target
	RotateOwnerToInput();

	AnimInst->Montage_Play(Tweakables.Montages.RisingAttack, 1.f);

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
				bAirComboActive = true;
				AirHitCount = 0;
			}

			AirHitCount++;

			// First hit is near-zero gravity (full hang), each subsequent hit
			// adds more pull so by hit 4 you're noticeably sinking -- just like DMC3.
			// Capped by DefaultGravityScale (not "current" or "saved-at-entry") so a stacked
			// hold-loop override can't leak its lowered value into our ceiling.
			const float ScaledGravity = Tweakables.AirCombat.ComboBaseGravity + (Tweakables.AirCombat.ComboGravityPerHit * (AirHitCount - 1));
			Movement->GravityScale = FMath::Min(ScaledGravity, DefaultGravityScale);

			// Snap vertical velocity so the character hangs instead of rising/falling
			FVector Vel = Movement->Velocity;
			Vel.Z = Tweakables.AirCombat.ComboVerticalVelocitySnap;
			Movement->Velocity = Vel;
		}
	}

	AnimInst->Montage_Play(Montage, 1.f);

	// Attack magnet: slide the player to an ideal stand-off distance in front of the
	// target and ignore their capsule for the rest of the attack.  When the magnet fires
	// we SKIP the legacy step-in launch -- otherwise the launch fights the slide and we
	// end up intersecting the enemy (exactly what the magnet exists to prevent).
	const bool bMagnetArmed = (Type != EAttackType::EAT_Rising) && TryStartMagnetSlide();

	// Step-in fallback: only used when no magnet target was found.  Preserves the old
	// "swing at air lunges you forward" behaviour for when you're not locked on / aimed
	// at anyone, so combos still travel.
	if (!bMagnetArmed && Tweakables.ComboChain.StepInForce > 0.f && Type != EAttackType::EAT_Rising)
	{
		if (UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
		{
			const bool bGrounded = !Movement->IsFalling();
			if (bGrounded || !Tweakables.ComboChain.bStepInGroundedOnly)
			{
				bool bAllow = true;

				// If locked on and already close, skip the step-in so we don't
				// shove through the target.
				if (AActor* Target = OwnerChar->GetLockedTarget())
				{
					const float Dist = FVector::Dist2D(OwnerChar->GetActorLocation(), Target->GetActorLocation());
					if (Dist < Tweakables.ComboChain.StepInMaxLockOnRange * 0.5f)
					{
						bAllow = false;
					}
				}

				if (bAllow)
				{
					const FVector Forward = OwnerChar->GetActorForwardVector() * Tweakables.ComboChain.StepInForce;
					// XYOverride = true so the step replaces residual horizontal velocity,
					// but preserve Z so jump/air state isn't affected.
					OwnerChar->LaunchCharacter(Forward, true, false);
				}
			}
		}
	}

	bIsAttacking = true;
	bComboWindowOpen = false;
	bDelayedWindowOpen = false;
	bSavedNextAttack = false;
	CurrentAttackType = Type;
	ComboIndex++;

	PlaySwingSound();
}

UAnimMontage* UHSCombatComponent::GetMontageForCombo(EAttackType Type, int32 Index) const
{
	const TArray<TObjectPtr<UAnimMontage>>* List = nullptr;

	switch (Type)
	{
	case EAttackType::EAT_Light:
		// Branch priority: delayed > forward-tilt > normal.  Each branch falls back to normal
		// silently if its own array isn't populated at this index, so a mostly-empty forward
		// array won't strand the player mid-combo.
		if (bOnDelayedBranch && Tweakables.Montages.LightDelayedCombo.IsValidIndex(Index))
		{
			List = &Tweakables.Montages.LightDelayedCombo;
		}
		else if (bOnForwardBranch && Tweakables.Montages.LightForwardCombo.IsValidIndex(Index))
		{
			List = &Tweakables.Montages.LightForwardCombo;
		}
		else
		{
			List = &Tweakables.Montages.LightCombo;
		}
		break;
	case EAttackType::EAT_Heavy: List = &Tweakables.Montages.HeavyCombo; break;
	case EAttackType::EAT_Air:   List = &Tweakables.Montages.AirCombo;   break;
	default: return nullptr;
	}

	if (!List->IsValidIndex(Index)) return nullptr;
	return (*List)[Index];
}

float UHSCombatComponent::GetDamageForCurrentAttack() const
{
	switch (CurrentAttackType)
	{
	case EAttackType::EAT_Heavy:  return Tweakables.Damage.Heavy;
	case EAttackType::EAT_Air:    return Tweakables.Damage.Air;
	case EAttackType::EAT_Rising: return Tweakables.Damage.Rising;
	default:                      return Tweakables.Damage.Light;
	}
}

void UHSCombatComponent::OpenComboWindow()
{
	bComboWindowOpen = true;

	// In strict-finish mode we don't early-fire the buffer here; we wait for the
	// montage to fully play out and fire the buffered attack in OnAttackFinished.
	if (Tweakables.ComboChain.bStrictFinishBeforeChain) return;

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

void UHSCombatComponent::OpenDelayedComboWindow()
{
	bDelayedWindowOpen = true;

	// If a light press was buffered BEFORE the delayed window opened (e.g. the player
	// pressed slightly too early), consume it now as the delayed branch trigger.
	if (bSavedNextAttack
		&& BufferedAttackType == EAttackType::EAT_Light
		&& CurrentAttackType == EAttackType::EAT_Light
		&& Tweakables.Montages.LightDelayedCombo.Num() > 0)
	{
		bSavedNextAttack = false;
		BufferedAttackType = EAttackType::EAT_None;
		bOnDelayedBranch = true;
		PlayNextAttack(EAttackType::EAT_Light);
	}
}

void UHSCombatComponent::CloseDelayedComboWindow()
{
	bDelayedWindowOpen = false;
}

void UHSCombatComponent::OnAttackFinished()
{
	// Strict-finish mode: if a buffered attack is waiting, play it now that the
	// current animation has fully completed, keeping the current combo index.
	if (Tweakables.ComboChain.bStrictFinishBeforeChain && bSavedNextAttack)
	{
		const EAttackType Next = BufferedAttackType;
		bSavedNextAttack = false;
		BufferedAttackType = EAttackType::EAT_None;
		PlayNextAttack(Next);
		return;
	}

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
	// Kill any held-air loop on touchdown FIRST -- tick would catch this next frame anyway,
	// but an explicit stop here avoids a visible flicker between landing and tick.
	// Passing bFalling=false routes UpdateAirHoldLoop through its Montage_Stop path
	// (with Tweakables.AirCombat.HoldLandBlendOut) rather than trying to play the airborne Out section.
	// This path also clears bHoldGravityApplied internally.
	if (bAirHoldActive)
	{
		UpdateAirHoldLoop(false, false);
	}

	// Air combos make no sense once grounded -- clear the flag, then let the helper snap
	// gravity back to default (no-ops if hold-loop somehow still holds it, but that path
	// was just cleared above).
	bAirComboActive = false;
	RestoreGravityIfOverridden();
}

void UHSCombatComponent::UpdateAirHoldLoop(bool bButtonHeld, bool bFalling)
{
	if (!OwnerChar || !Tweakables.Montages.AirHoldLoop) return;

	UAnimInstance* AnimInst = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement();

	// Detect natural completion (Out section finished playing) OR external interrupt
	// (another attack Montage_Play took over the channel).  Clear HOLD state -- do NOT touch
	// air-combo state here, even if it's active.  RestoreGravityIfOverridden only actually
	// restores when both systems are clear, so if an air-combo is mid-chain its gravity stays.
	if (bAirHoldActive && !AnimInst->Montage_IsPlaying(Tweakables.Montages.AirHoldLoop))
	{
		bAirHoldActive = false;
		bAirHoldStopRequested = false;
		bHoldGravityApplied = false;
		RestoreGravityIfOverridden();
	}

	const bool bShouldRun = bButtonHeld && bFalling;

	if (bShouldRun)
	{
		// Don't start while another attack montage owns the channel -- let it finish first.
		if (bIsAttacking) return;

		if (!bAirHoldActive)
		{
			// Fresh start: Montage_Play drops us at time 0 (In section assuming it's first
			// in the montage asset).  The asset's own section links carry us In -> Loop, then
			// we self-loop Loop here so it doesn't auto-advance to Out.
			AnimInst->Montage_Play(Tweakables.Montages.AirHoldLoop, 1.f);
			AnimInst->Montage_SetNextSection(Tweakables.AirCombat.HoldLoopSectionName, Tweakables.AirCombat.HoldLoopSectionName, Tweakables.Montages.AirHoldLoop);
			bAirHoldActive = true;
			bAirHoldStopRequested = false;

			// Slow the player's fall so they actually hang during the flurry.  No "current
			// gravity" snapshot needed -- RestoreGravityIfOverridden always targets the
			// authoritative DefaultGravityScale captured at BeginPlay.
			if (Movement && !bHoldGravityApplied)
			{
				Movement->GravityScale = Tweakables.AirCombat.HoldPlayerGravityScale;
				bHoldGravityApplied = true;
			}
		}
		else if (bAirHoldStopRequested)
		{
			// Player re-pressed mid-out -- cancel the Loop -> Out rewire so we stay looping.
			// If the current section has already advanced past Loop (i.e. we're already inside Out),
			// Montage_SetNextSection on "Loop" has no effect on the currently playing section, so
			// the Out section will finish naturally and the next tick will re-start fresh.
			AnimInst->Montage_SetNextSection(Tweakables.AirCombat.HoldLoopSectionName, Tweakables.AirCombat.HoldLoopSectionName, Tweakables.Montages.AirHoldLoop);
			bAirHoldStopRequested = false;
		}
	}
	else if (bAirHoldActive)
	{
		if (!bFalling)
		{
			// Landed mid-loop -- character is grounded now, snap out with a short blend
			// rather than playing an airborne Out animation on the floor.  Clear hold flags;
			// helper restores gravity only if air-combo isn't also active.
			AnimInst->Montage_Stop(Tweakables.AirCombat.HoldLandBlendOut, Tweakables.Montages.AirHoldLoop);
			bAirHoldActive = false;
			bAirHoldStopRequested = false;
			bHoldGravityApplied = false;
			RestoreGravityIfOverridden();
		}
		else if (!bAirHoldStopRequested)
		{
			// Released in mid-air -- let the current Loop iteration finish, then play Out.
			// CRITICAL: restore gravity IMMEDIATELY on release, not when Out ends.  Otherwise
			// if the player drifts off a ledge during Out (so they never "land"), gravity
			// stays low indefinitely because OnOwnerLanded never fires.  Out's blend-out is
			// short enough that the slight weight return during it is imperceptible -- and
			// far better than a permanently-floaty player.
			AnimInst->Montage_SetNextSection(Tweakables.AirCombat.HoldLoopSectionName, Tweakables.AirCombat.HoldOutSectionName, Tweakables.Montages.AirHoldLoop);
			bAirHoldStopRequested = true;
			bHoldGravityApplied = false;
			RestoreGravityIfOverridden();
		}
	}
}

void UHSCombatComponent::ResetCombo()
{
	// Hard reset of both gravity-override systems -- combo chain is fully ending, so whatever
	// state was held can safely be released.  Clear flags explicitly, then the helper will
	// actually snap gravity to DefaultGravityScale (since neither flag is still set).
	bAirComboActive      = false;
	bHoldGravityApplied  = false;
	RestoreGravityIfOverridden();

	bIsAttacking = false;
	bComboWindowOpen = false;
	bDelayedWindowOpen = false;
	bSavedNextAttack = false;
	CurrentAttackType = EAttackType::EAT_None;
	BufferedAttackType = EAttackType::EAT_None;
	ComboIndex = 0;
	bOnDelayedBranch = false;
	bOnForwardBranch = false;

	// Release capsule-ignore on the most recent magnet target so out-of-combat
	// movement/push behaviour is restored.  Safe to call when no magnet was armed.
	EndMagnet();
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

bool UHSCombatComponent::IsInputTiltForward() const
{
	if (!OwnerChar) return false;

	const FVector2D Input = OwnerChar->GetMoveInputCached();
	if (Input.IsNearlyZero(0.1f)) return false;

	AController* PC = OwnerChar->GetController();
	if (!PC) return false;

	// Resolve the stick into a camera-relative world direction (same formula as RotateOwnerToInput).
	const FRotator CamYaw(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::Y);
	const FVector InputWorldDir = (Forward * Input.Y + Right * Input.X).GetSafeNormal();
	if (InputWorldDir.IsNearlyZero()) return false;

	// "Forward" when locked = toward the target.  Otherwise = along camera-forward.
	FVector ReferenceDir = Forward;
	if (AActor* Target = OwnerChar->GetLockedTarget())
	{
		FVector ToTarget = Target->GetActorLocation() - OwnerChar->GetActorLocation();
		ToTarget.Z = 0.f;
		if (!ToTarget.IsNearlyZero())
		{
			ReferenceDir = ToTarget.GetSafeNormal();
		}
	}

	const float Dot = FVector::DotProduct(InputWorldDir, ReferenceDir);
	return Dot >= Tweakables.ComboChain.ForwardTiltDotThreshold;
}

void UHSCombatComponent::DoSwordTrace()
{
	if (!OwnerChar) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Start = OwnerChar->GetActorLocation() + OwnerChar->GetActorForwardVector() * 50.f;
	const FVector End = Start + OwnerChar->GetActorForwardVector() * Tweakables.Trace.Range;

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(OwnerChar);

	TArray<FHitResult> Hits;
	const EDrawDebugTrace::Type DrawMode = Tweakables.Trace.bDebugDraw ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	const bool bHit = UKismetSystemLibrary::SphereTraceMulti(
		World,
		Start,
		End,
		Tweakables.Trace.Radius,
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

	// Determine hit weight based on attack type AND combo position.
	// Mid-combo swings send a mild hit so the enemy staggers but stays in range;
	// only the final swing of a string actually knocks them back / flings them.
	// ComboIndex was already incremented at the end of PlayNextAttack, so if
	// GetMontageForCombo(Type, ComboIndex) returns null the swing we're tracing
	// right now is the last in that combo.
	const bool bIsFinalInCombo = (GetMontageForCombo(CurrentAttackType, ComboIndex) == nullptr);

	EHitWeight HitWeight = EHitWeight::EHW_Light;
	if (CurrentAttackType == EAttackType::EAT_Rising)
	{
		HitWeight = EHitWeight::EHW_Launcher;
	}
	else if (CurrentAttackType == EAttackType::EAT_Heavy)
	{
		// Mid-combo heavy: light-weight pushback. Final heavy: big finisher that flings.
		HitWeight = bIsFinalInCombo ? EHitWeight::EHW_Finisher : EHitWeight::EHW_Light;
	}
	else if (CurrentAttackType == EAttackType::EAT_Light)
	{
		// Mid-combo light: tiny push. Final light: a heavier stagger to finish the string.
		HitWeight = bIsFinalInCombo ? EHitWeight::EHW_Heavy : EHitWeight::EHW_Light;
	}
	else if (CurrentAttackType == EAttackType::EAT_Air)
	{
		// Air combos keep enemy juggled; final air hit slams them down.
		HitWeight = bIsFinalInCombo ? EHitWeight::EHW_Finisher : EHitWeight::EHW_Light;
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

			// Air-hold-loop: suspend the hit enemy so the flurry reads visually.  Each
			// connecting hit refreshes the hang window (ApplyAirHang's timer resets), so
			// the enemy keeps floating as long as the loop keeps connecting.
			if (bAirHoldActive)
			{
				if (AHSDummyEnemy* Enemy = Cast<AHSDummyEnemy>(HitActor))
				{
					Enemy->ApplyAirHang(Tweakables.AirCombat.EnemyHangDuration, Tweakables.AirCombat.EnemyHangGravity, Tweakables.AirCombat.EnemyHangLift);
				}
			}

			bLandedHit = true;
		}
	}

	// Rising attack: launch the player into the air alongside the enemy
	if (bLandedHit && CurrentAttackType == EAttackType::EAT_Rising)
	{
		OwnerChar->LaunchCharacter(FVector(0.f, 0.f, Tweakables.Damage.RisingLaunchForce), false, true);
	}

	// Hit SFX
	if (bLandedHit)
	{
		PlayHitSound();
	}

	// Camera shake on hit (FF16 style impact feel)
	if (bLandedHit && Tweakables.HitFeedback.CameraShake)
	{
		float ShakeScale = Tweakables.HitFeedback.LightShakeScale;
		if (CurrentAttackType == EAttackType::EAT_Heavy) ShakeScale = Tweakables.HitFeedback.HeavyShakeScale;
		else if (CurrentAttackType == EAttackType::EAT_Air) ShakeScale = Tweakables.HitFeedback.AirShakeScale;

		if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
		{
			PC->ClientStartCameraShake(Tweakables.HitFeedback.CameraShake, ShakeScale);
		}
	}

	// FOV compression per hit -- accumulates up to Tweakables.HitFeedback.MaxFOVCompression, eases back in Tick
	if (bLandedHit)
	{
		CurrentFOVCompression = FMath::Min(CurrentFOVCompression + Tweakables.HitFeedback.FOVCompressionPerHit, Tweakables.HitFeedback.MaxFOVCompression);
	}

	// Trauma-based camera shake. DmC 2013 favors hitstop over shake, but a small
	// Perlin-trauma nudge sells the impact without overwhelming the frame.
	// Scale by hit weight so light pokes barely move the camera and finishers/launchers snap it.
	if (bLandedHit && OwnerChar)
	{
		if (UHSDynamicCameraComponent* DynCam = OwnerChar->GetDynamicCamera())
		{
			float TraumaAmount = 0.12f;  // light / mid-combo default
			float PitchKick    = 0.f;    // upward tilt bias for heavier hits
			float KickHoriz    = 8.f;    // backward positional recoil (units/sec impulse into spring)
			float KickUp       = 2.f;    // small vertical rise component

			switch (HitWeight)
			{
			case EHitWeight::EHW_Light:    TraumaAmount = 0.12f; KickHoriz = 8.f;  KickUp = 2.f; break;
			case EHitWeight::EHW_Heavy:    TraumaAmount = 0.22f; KickHoriz = 14.f; KickUp = 4.f; break;
			case EHitWeight::EHW_Finisher: TraumaAmount = 0.30f; PitchKick = 3.f; KickHoriz = 20.f; KickUp = 6.f; break;
			case EHitWeight::EHW_Launcher: TraumaAmount = 0.30f; PitchKick = 6.f; KickHoriz = 22.f; KickUp = 8.f; break;
			default: break;
			}

			// Heavy attack type bumps trauma a notch regardless of weight (charged swings feel weightier).
			if (CurrentAttackType == EAttackType::EAT_Heavy)
			{
				TraumaAmount = FMath::Min(TraumaAmount + 0.04f, 1.f);
				KickHoriz   += 3.f;
			}

			DynCam->AddTrauma(TraumaAmount);
			if (PitchKick > 0.f)
			{
				DynCam->AddPitchKick(PitchKick);
			}

			// Positional kick: opposite the swing direction (player is hitting forward, camera
			// pushes backward), with a small upward component.  The spring on the camera side
			// dampens it back within ~0.25s so the next hit in the combo can stack cleanly.
			const FVector KickImpulse = (-OwnerChar->GetActorForwardVector() * KickHoriz)
			                          + FVector(0.f, 0.f, KickUp);
			DynCam->AddPositionalKick(KickImpulse);
		}
	}
}

void UHSCombatComponent::UpdateFOVCompression(float DeltaTime)
{
	if (!OwnerChar || !OwnerChar->FollowCamera) return;

	// Ease back toward 0 every frame (whether or not we hit)
	CurrentFOVCompression = FMath::FInterpTo(CurrentFOVCompression, 0.f, DeltaTime, Tweakables.HitFeedback.FOVRecoverySpeed);

	// Apply compression as a zoom-in (subtract from default)
	OwnerChar->FollowCamera->FieldOfView = DefaultCameraFOV - CurrentFOVCompression;
}

void UHSCombatComponent::PlaySwingSound()
{
	if (!OwnerChar) return;

	USoundBase* Sound = nullptr;
	switch (CurrentAttackType)
	{
	case EAttackType::EAT_Light:  Sound = Tweakables.SFX.LightSwing;  break;
	case EAttackType::EAT_Heavy:  Sound = Tweakables.SFX.HeavySwing;  break;
	case EAttackType::EAT_Air:    Sound = Tweakables.SFX.AirSwing;    break;
	case EAttackType::EAT_Rising: Sound = Tweakables.SFX.RisingSwing; break;
	default: break;
	}

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, OwnerChar->GetActorLocation());
	}

	// Voice grunt -- heavy attacks use the heavy pool, everything else uses light
	const bool bIsHeavy = (CurrentAttackType == EAttackType::EAT_Heavy);
	OwnerChar->PlayAttackGrunt(bIsHeavy);
}

void UHSCombatComponent::PlayHitSound()
{
	if (!OwnerChar) return;

	USoundBase* Sound = nullptr;
	switch (CurrentAttackType)
	{
	case EAttackType::EAT_Light:  Sound = Tweakables.SFX.LightHit;  break;
	case EAttackType::EAT_Heavy:  Sound = Tweakables.SFX.HeavyHit;  break;
	case EAttackType::EAT_Air:    Sound = Tweakables.SFX.AirHit;    break;
	case EAttackType::EAT_Rising: Sound = Tweakables.SFX.RisingHit; break;
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
		CamMgr->StartCameraFade(Tweakables.HitFeedback.FlashIntensity, 0.f, Tweakables.HitFeedback.FlashDuration, FLinearColor::White, false, true);
	}

	// Brief time dilation for dramatic impact (DMC3/FF16 style)
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, Tweakables.HitFeedback.TimeDilationScale);

		World->GetTimerManager().ClearTimer(TimeDilationHandle);
		World->GetTimerManager().SetTimer(TimeDilationHandle, this, &UHSCombatComponent::RestoreTimeDilation, Tweakables.HitFeedback.TimeDilationDuration, false);
	}
}

void UHSCombatComponent::RestoreTimeDilation()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
}

/*****************************************************/
/*                    Attack Magnet                  */
/*****************************************************/
// DMC-style magnet: every swing snaps the player to an ideal stand-off distance in
// front of the current target over a handful of frames, and ignores the target's
// capsule so the two never intersect mid-combo.  The slide itself is position-driven
// (SetActorLocation + sweep) with an ease-out cubic so the first frames do most of
// the travel and the last frames settle smoothly.

bool UHSCombatComponent::TryStartMagnetSlide()
{
	if (!Tweakables.Magnet.bEnabled || !OwnerChar) return false;

	AActor* Target = FindMagnetTarget();
	if (!Target) return false;

	const FVector OwnerLoc  = OwnerChar->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();

	// Direction from target back to player (this is the side the slide ends on).
	// If they're perfectly stacked, bail -- no meaningful direction to slide in.
	FVector FromTarget = OwnerLoc - TargetLoc;
	FromTarget.Z = 0.f;
	if (FromTarget.IsNearlyZero())
	{
		return false;
	}
	const FVector StandoffDir = FromTarget.GetSafeNormal();

	// Gap-close guard: in DMC the magnet is a forward gap-closer only.  If you're already
	// at or inside ideal distance, the swing fires in place -- no drift, no re-snap.
	// This is the big fix for "moving all around the enemy" through a combo: in close
	// combat, ZERO slides fire, so the player stays planted on whichever side they
	// originally approached from.
	const float CurDist2D = FVector::Dist2D(OwnerLoc, TargetLoc);
	if (Tweakables.Magnet.bOnlyCloseGaps && CurDist2D <= Tweakables.Magnet.IdealDistance + Tweakables.Magnet.GapCloseTolerance)
	{
		// Player's already well-placed.  Track the target (so per-frame clamp works) but
		// don't arm a slide.  Return true anyway so the caller skips the legacy step-in.
		MagnetTargetActor  = Target;
		bMagnetSliding     = false;
		MagnetSlideElapsed = 0.f;
		return true;
	}

	// Ideal slide destination: ideal distance out from the target on the player's side.
	// Preserve the player's Z always -- we only want XY repositioning.  Pulling Z toward
	// the target would snap the player into the floor / into the air if the capsules are
	// at different heights, or while juggling in the air.
	FVector Destination = TargetLoc + StandoffDir * Tweakables.Magnet.IdealDistance;
	Destination.Z = OwnerLoc.Z;

	// Reject slides that would drag the player further than Tweakables.Magnet.MaxSlideDistance --
	// better to whiff than teleport.
	const float SlideDist = FVector::Dist(OwnerLoc, Destination);
	if (SlideDist > Tweakables.Magnet.MaxSlideDistance)
	{
		return false;
	}

	MagnetSlideStart   = OwnerLoc;
	MagnetSlideTarget  = Destination;
	MagnetSlideElapsed = 0.f;
	bMagnetSliding     = (SlideDist > KINDA_SMALL_NUMBER);
	MagnetTargetActor  = Target;

	return true;
}

void UHSCombatComponent::UpdateMagnetSlide(float DeltaTime)
{
	if (!OwnerChar) return;

	// ── Phase 1: initial gap-close slide (only fires when we were too far) ─────
	if (bMagnetSliding)
	{
		MagnetSlideElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(MagnetSlideElapsed / FMath::Max(Tweakables.Magnet.SlideDuration, 0.01f), 0.f, 1.f);

		// Ease-out cubic: travels most of the distance early, settles smoothly.  Reads as
		// a "snap" visually while keeping the last frame of motion sub-perceptual.
		const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);

		const FVector NewLoc = FMath::Lerp(MagnetSlideStart, MagnetSlideTarget, Eased);

		// Sweep=true so enemy capsules, walls, and floors still stop us.  The slide target
		// is on the player's own side of the enemy at IdealDistance, so the sweep doesn't
		// hit the target capsule -- we settle just outside it.
		OwnerChar->SetActorLocation(NewLoc, /*bSweep=*/ true);

		if (Alpha >= 1.f)
		{
			bMagnetSliding = false;
		}
		return;
	}

	// ── Phase 2: asymmetric per-frame safety clamp ────────────────────────────
	// Push-out only, never pull-in.  If the player drifts / root-motions INTO the
	// enemy's capsule (or inside Tweakables.Magnet.MinDistance), shove them back out.  Does NOT
	// pull them back toward the enemy if they're already outside.  This is the fix
	// for "phasing through the enemy capsule" -- no matter what the attack anim's
	// root motion tries, the player can't end up closer than MinDistance.
	AActor* Target = MagnetTargetActor.Get();
	if (!Target || !bIsAttacking || Tweakables.Magnet.MinDistance <= 0.f) return;

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector OwnerLoc  = OwnerChar->GetActorLocation();

	FVector FromTarget = OwnerLoc - TargetLoc;
	const float SavedZ = FromTarget.Z;
	FromTarget.Z = 0.f;

	const float CurDist = FromTarget.Size();
	if (CurDist >= Tweakables.Magnet.MinDistance) return; // already safely outside -- no-op
	if (CurDist < KINDA_SMALL_NUMBER) return; // perfectly stacked -- direction undefined

	const FVector StandoffDir = FromTarget / CurDist;
	const FVector ClampedLoc  = TargetLoc + StandoffDir * Tweakables.Magnet.MinDistance + FVector(0.f, 0.f, SavedZ);

	OwnerChar->SetActorLocation(ClampedLoc, /*bSweep=*/ true);
}

void UHSCombatComponent::EndMagnet()
{
	bMagnetSliding     = false;
	MagnetSlideElapsed = 0.f;
	MagnetTargetActor  = nullptr;
}

AActor* UHSCombatComponent::FindMagnetTarget() const
{
	if (!OwnerChar) return nullptr;

	// Prefer the lock-on target when it's within magnet range -- this keeps the slide
	// aligned with what the camera is already framing, which is the DMC default.
	if (AActor* Locked = OwnerChar->GetLockedTarget())
	{
		if (!Locked->IsPendingKillPending())
		{
			const float Dist = FVector::Dist(OwnerChar->GetActorLocation(), Locked->GetActorLocation());
			if (Dist <= Tweakables.Magnet.SearchRadius)
			{
				return Locked;
			}
		}
	}

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	const FVector OwnerLoc = OwnerChar->GetActorLocation();
	const FVector Forward  = OwnerChar->GetActorForwardVector();

	// Sphere overlap filtered to AHSDummyEnemy -- cheap and precise enough for a handful
	// of enemies.  For hundreds we'd swap to a spatial grid; not needed at this scale.
	TArray<AActor*> Overlaps;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(OwnerChar);

	UKismetSystemLibrary::SphereOverlapActors(
		World, OwnerLoc, Tweakables.Magnet.SearchRadius,
		ObjectTypes, AHSDummyEnemy::StaticClass(),
		IgnoreActors, Overlaps);

	// Aerial swings use a wider cone so juggled enemies that have drifted off-axis still get
	// picked up -- DMC3's aerial rave autotracks this way.  On the ground we want a tighter
	// cone so you don't magnet-sideways into non-target enemies you're just passing near.
	const bool bAirborne = OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling();
	const float ActiveCone = bAirborne ? Tweakables.Magnet.AirSearchConeDegrees : Tweakables.Magnet.SearchConeDegrees;
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

