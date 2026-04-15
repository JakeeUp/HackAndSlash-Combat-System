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

	if (UCharacterMovementComponent* Movement = OwnerEnemy->GetCharacterMovement())
	{
		bIsInAir = Movement->IsFalling();
	}

	const FVector Vel = OwnerEnemy->GetVelocity();
	Speed = FVector(Vel.X, Vel.Y, 0.f).Size();
}
