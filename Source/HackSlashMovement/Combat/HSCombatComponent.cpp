#include "HSCombatComponent.h"

#include "Character/HSPlayerCharacter.h"
#include "Character/HSDummyEnemy.h"
#include "Combat/HSDamageable.h"
#include "Combat/HSStyleComponent.h"
#include "Combat/HSDynamicCameraComponent.h"
#include "Combat/HSAttackMagnetComponent.h"
#include "Combat/HSHitFeedbackComponent.h"

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
	UHSAttackMagnetComponent* Mag = OwnerChar->GetMagnet();
	const bool bMagnetArmed = (Type != EAttackType::EAT_Rising) && Mag && Mag->TryStartSlide();

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

	if (OwnerChar)
	{
		if (UHSAttackMagnetComponent* Mag = OwnerChar->GetMagnet())
		{
			Mag->EndSlide();
		}
	}
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

	if (bLandedHit && OwnerChar)
	{
		if (UHSHitFeedbackComponent* HF = OwnerChar->GetHitFeedback())
		{
			HF->OnHitLanded(CurrentAttackType, HitWeight, OwnerChar->GetActorForwardVector());
		}
	}
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


