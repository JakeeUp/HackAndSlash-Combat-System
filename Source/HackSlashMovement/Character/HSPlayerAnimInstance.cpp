// Fill out your copyright notice in the Description page of Project Settings.


#include "HSPlayerAnimInstance.h"

#include "HSPlayerCharacter.h"
#include "Combat/HSCombatComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

void UHSPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerChar = Cast<AHSPlayerCharacter>(TryGetPawnOwner());
	if (OwnerChar)
	{
		OwnerMove = OwnerChar->GetCharacterMovement();
	}
}

void UHSPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerChar)
	{
		OwnerChar = Cast<AHSPlayerCharacter>(TryGetPawnOwner());
		if (OwnerChar)
		{
			OwnerMove = OwnerChar->GetCharacterMovement();
		}
		return;
	}
	if (!OwnerMove) return;

	const FVector Vel = OwnerChar->GetVelocity();
	const FVector FlatVel(Vel.X, Vel.Y, 0.f);
	Speed = FlatVel.Size();

	bIsInAir = OwnerMove->IsFalling();
	bIsAccelerating = OwnerMove->GetCurrentAcceleration().SizeSquared() > 0.f;
	bIsSprinting = OwnerChar->IsSprinting();

	Direction = UKismetAnimationLibrary::CalculateDirection(FlatVel, OwnerChar->GetActorRotation());

	if (UHSCombatComponent* Combat = OwnerChar->GetCombat())
	{
		bIsAttacking = Combat->IsAttacking();
	}

	bIsDodging = OwnerChar->IsDodging();
}
