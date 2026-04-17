// Animation instance for the dummy enemy -- drives the ABP state machine.


#include "HSDummyEnemyAnimInstance.h"

#include "HSDummyEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"


void UHSDummyEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerEnemy = Cast<AHSDummyEnemy>(TryGetPawnOwner());
}

void UHSDummyEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerEnemy)
	{
		OwnerEnemy = Cast<AHSDummyEnemy>(TryGetPawnOwner());
		if (!OwnerEnemy) return;
	}

	EnemyState = OwnerEnemy->GetEnemyState();
	bIsDead = OwnerEnemy->IsDead();
	bInDropLoop = OwnerEnemy->IsInDropLoop();
	bJustLaunched = OwnerEnemy->IsJustLaunched();

	if (UCharacterMovementComponent* Movement = OwnerEnemy->GetCharacterMovement())
	{
		bIsInAir = Movement->IsFalling();
	}

	const FVector Vel = OwnerEnemy->GetVelocity();
	const FVector FlatVel(Vel.X, Vel.Y, 0.f);
	Speed = FlatVel.Size();

	// Direction: angle between actor forward and velocity direction.
	// Atan2(right component, forward component) gives -180..180.
	if (Speed > 1.f)
	{
		Direction = FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(FlatVel, OwnerEnemy->GetActorRightVector()),
			FVector::DotProduct(FlatVel, OwnerEnemy->GetActorForwardVector())
		));
	}
	else
	{
		Direction = 0.f; // no velocity -- don't let direction snap around
	}

	if (OwnerEnemy->CombatAI)
	{
		AIState = OwnerEnemy->CombatAI->GetAIState();
	}
}
