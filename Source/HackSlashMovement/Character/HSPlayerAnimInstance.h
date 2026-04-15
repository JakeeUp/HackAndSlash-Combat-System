#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HSPlayerAnimInstance.generated.h"


class AHSPlayerCharacter;
class UCharacterMovementComponent;


UCLASS()
class HACKSLASHMOVEMENT_API UHSPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/*****************************************************/
	/*                        Owner                      */
	/*****************************************************/
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Owner")
	class AHSPlayerCharacter* OwnerChar;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Owner")
	class UCharacterMovementComponent* OwnerMove;

	/*****************************************************/
	/*                     Locomotion                    */
	/*****************************************************/
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Combat")
	bool bIsAttacking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Combat")
	bool bIsDodging = false;
};
