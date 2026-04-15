// Animation instance for the dummy enemy -- drives the ABP state machine.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HSDummyEnemy.h"
#include "HSDummyEnemyAnimInstance.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UHSDummyEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Owner")
	class AHSDummyEnemy* OwnerEnemy;

	/*****************************************************/
	/*          State variables for the ABP              */
	/*****************************************************/

	/** Current enemy state (Idle, Airborne, Down). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	EEnemyState EnemyState = EEnemyState::EES_Idle;

	/** True while the enemy is in the air (CharacterMovement::IsFalling). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	bool bIsInAir = false;

	/** True while the hit drop loop should play (airborne, not being hit). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	bool bInDropLoop = false;

	/** True when the enemy is dead. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	bool bIsDead = false;

	/** Horizontal speed for locomotion blending (if needed later). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	float Speed = 0.f;
};
