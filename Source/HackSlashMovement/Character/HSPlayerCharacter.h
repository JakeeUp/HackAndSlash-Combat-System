// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSPlayerCharacter.generated.h"


class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class UHSCombatComponent;
class UHSStyleComponent;
class UHSStyleHUD;
class UHSLockOnReticle;
class UUserWidget;
class UWidgetComponent;
struct FInputActionValue;


UCLASS()
class HACKSLASHMOVEMENT_API AHSPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AHSPlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Reset air dodge counter when we touch ground
	virtual void Landed(const FHitResult& Hit) override;

	// Re-attach weapon mesh to the configured socket. Runs in editor + at runtime.
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UHSCombatComponent* Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UHSStyleComponent* Style;

	/** Socket on the character's hand bone where the weapon attaches. Defaults to Weapon_R (SwordAnimsetPro skeleton). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Weapon")
	FName WeaponSocketName = TEXT("Weapon_R");

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE AActor* GetLockedTarget() const { return LockedTarget; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE bool IsLockedOn() const { return LockedTarget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE bool IsDodging() const { return bIsDodging; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE UHSCombatComponent* GetCombat() const { return Combat; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE UHSStyleComponent* GetStyle() const { return Style; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE FVector2D GetMoveInputCached() const { return moveInputCached; }

protected:
	/*****************************************************/
	/*                    Configurations                 */
	/*****************************************************/
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|HUD")
	TSubclassOf<UUserWidget> StyleHUDClass;

	/** Max distance to find a lock-on target. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnRange = 2000.f;

	/** Camera interpolation speed when locked on. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnInterpSpeed = 8.f;

	/** Camera boom socket offset when locked on. Reduced Y keeps action centered; Z raises the POV. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	FVector LockOnCameraOffset = FVector(0.f, 60.f, 120.f);

	/** Arm length when grounded and locked on. FF16 keeps a wide shot so both characters are visible. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnArmLengthGround = 700.f;

	/** Arm length during air combos. Pulls back further so you can see the full action. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnArmLengthAir = 850.f;

	/** Pitch offset to angle the camera down during lock-on. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnPitchOffset = -20.f;

	/** Minimum pitch the camera can reach during lock-on (prevents looking straight up). Negative = looking down. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnPitchMin = -45.f;

	/** Maximum pitch during lock-on (prevents camera going under the ground). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnPitchMax = 10.f;

	/** The camera focus point uses this fixed height above the player, not the enemy's actual Z.
	 *  DMC3 pattern: camera height stays stable, doesn't chase the enemy vertically. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnFocusHeight = 120.f;

	/** Widget class for the lock-on reticle that appears on the enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	TSubclassOf<UHSLockOnReticle> LockOnReticleClass;

	/** Height offset above the enemy's root for the reticle. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnReticleHeightOffset = 100.f;

	/** How much the camera look-at point shifts toward the enemy horizontally (0 = player, 1 = enemy). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockOnFocusBias = 0.45f;

	/** Speed at which the arm length adjusts between ground and air distances. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnArmInterpSpeed = 5.f;

	/** Minimum height the camera must stay above the player's feet during lock-on.
	 *  DMC3 pattern: camera never goes below this height to prevent terrain clipping. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnMinCameraHeight = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	TSubclassOf<class AHSHomingProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	float ProjectileDamage = 15.f;

	/** Offset from actor location where the projectile spawns. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	FVector ProjectileSpawnOffset = FVector(80.f, 30.f, 50.f);

	/** Animation for the right arm when firing a projectile. Plays on UpperBody slot. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	UAnimMontage* ProjectileCastMontage;

	/** Camera shake when firing a projectile (press Q). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	float FireShakeScale = 0.5f;

	/*****************************************************/
	/*                  Enemy Pull (Snatch)              */
	/*****************************************************/

	/** Animation the player plays when pulling the enemy in. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Pull")
	UAnimMontage* PullMontage;

	/** How fast the enemy gets pulled toward the player. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Pull")
	float PullForce = 1800.f;

	/** Slight vertical lift so the enemy doesn't scrape the ground. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Pull")
	float PullLift = 150.f;

	/** Max range to pull an enemy from. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Pull")
	float PullRange = 1500.f;

	/** Camera shake on pull. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Pull")
	TSubclassOf<UCameraShakeBase> PullCameraShake;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Pull")
	float PullShakeScale = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement")
	float WalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Movement")
	float SprintSpeed = 950.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge")
	TArray<UAnimMontage*> DodgeMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge")
	float DodgePlayRate = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge")
	int32 MaxAirDodges = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge")
	float AirDodgeLaunchSpeed = 1100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge")
	float AirDodgeVerticalLift = 0.f;

	/** After this fraction of the dodge has played, movement input cancels the dodge. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DodgeCancelAfterFraction = 0.55f;

	/** Blend-out time when dodge is canceled by movement input. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Dodge")
	float DodgeCancelBlendOut = 0.15f;

	/*****************************************************/
	/*                        State                      */
	/*****************************************************/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsSprinting = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsDodging = false;

	/** True after a dodge has been input-cancelled but its montage is still blending out.
	 *  Move() uses this to skip re-running TryCancelDodgeFromInput while still applying input. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bDodgeRecoveryActive = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	int32 AirDodgesUsed = 0;

private:
	/*****************************************************/
	/*                       Input                       */
	/*****************************************************/
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* inputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* moveInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* lookInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* jumpInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* sprintInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* lightAttackInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* heavyAttackInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* dodgeInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* lockOnInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* projectileInputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* pullInputAction;

	UPROPERTY()
	AActor* LockedTarget = nullptr;

	FVector DefaultCameraOffset;
	float DefaultArmLength = 0.f;
	bool bCameraDefaultsSaved = false;

	/** Where the camera offset and arm length are interpolating toward. */
	FVector DesiredCameraOffset;
	float DesiredArmLength = 0.f;

	/** The reticle widget component attached to the current lock-on target. */
	UPROPERTY()
	UWidgetComponent* LockOnReticleComp = nullptr;

	void ShowLockOnReticle(AActor* Target);
	void HideLockOnReticle();

	// Cached so dodge direction can use the latest stick/WASD value
	FVector2D moveInputCached;

	UFUNCTION()
	void Move(const FInputActionValue& InputValue);

	UFUNCTION()
	void MoveCompleted(const FInputActionValue& InputValue);

	UFUNCTION()
	void Look(const FInputActionValue& InputValue);

	UFUNCTION()
	void StartSprint();

	UFUNCTION()
	void StopSprint();

	UFUNCTION()
	void LightAttack();

	UFUNCTION()
	void HeavyAttack();

	UFUNCTION()
	void Dodge();

	UFUNCTION()
	void ToggleLockOn();

	AActor* FindLockOnTarget() const;

	UFUNCTION()
	void FireProjectile();

	UFUNCTION()
	void JumpCancel();

	UFUNCTION()
	void PullEnemy();

	/*****************************************************/
	/*                       Helpers                      */
	/*****************************************************/
	/** Returns true when the stick is tilted away from the locked target (back-tilt for rising attack). */
	bool IsBackTiltInput() const;

	FVector ResolveCameraRelativeInputDirection() const;
	UAnimMontage* GetDodgeMontage(int32 Index) const;
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool TryCancelDodgeFromInput(const FVector2D& Input);
};
