// Animation instance for the dummy enemy -- drives the ABP state machine.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HSDummyEnemy.h"
#include "HSEnemyCombatAI.h"
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

	/** Pulsed true the moment a launcher hit lands -- use as an immediate transition
	 *  trigger in the ABP to jump to the air hit-react state with no delay.
	 *  Cleared once StartDropLoop fires (enemy is now in the falling loop). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	bool bJustLaunched = false;

	/** True when the enemy is dead. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	bool bIsDead = false;

	/** Horizontal speed for locomotion blending. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	float Speed = 0.f;

	/** Angle (degrees) between the actor's forward vector and its velocity.
	 *  0 = moving forward, 90 = strafing right, -90 = strafing left, 180 = backing up.
	 *  Used as the X axis on the 2D locomotion blend space. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	float Direction = 0.f;

	/** Current AI state -- used to drive the windup telegraph pose. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|State")
	EAIState AIState = EAIState::Idle;
};
