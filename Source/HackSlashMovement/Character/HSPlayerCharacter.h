#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
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
class UNiagaraSystem;
class USoundBase;
class UAudioComponent;
struct FInputActionValue;


/** A pool of footstep sounds for a single surface. One is picked at random per step. */
USTRUCT(BlueprintType)
struct FHSFootstepSoundSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<USoundBase*> Sounds;
};


UCLASS()
class HACKSLASHMOVEMENT_API AHSPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHSPlayerCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void OnJumped_Implementation() override;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* BGMAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* CombatBGMAudio;

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
	FORCEINLINE bool IsDoubleJumping() const { return bIsDoubleJumping; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE UHSCombatComponent* GetCombat() const { return Combat; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE UHSStyleComponent* GetStyle() const { return Style; }

	UFUNCTION(BlueprintPure, Category = "State")
	FORCEINLINE FVector2D GetMoveInputCached() const { return moveInputCached; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealthPercent() const { return (MaxHealth > 0.f) ? (CurrentHealth / MaxHealth) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE float GetCurrentMP() const { return CurrentMP; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE float GetMaxMP() const { return MaxMP; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetMPPercent() const { return (MaxMP > 0.f) ? (CurrentMP / MaxMP) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE int32 GetPlayerLevel() const { return PlayerLevel; }

	/** Called by UHSFootstepNotify on foot-plant frames. Traces down from the foot
	 *  bone, reads the physical surface, and spawns the matching VFX + SFX. */
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	void PlayFootstep(FName FootBone);

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE float GetCurrentXP() const { return CurrentXP; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	FORCEINLINE float GetXPToNextLevel() const { return XPToNextLevel; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetXPPercent() const { return (XPToNextLevel > 0.f) ? (CurrentXP / XPToNextLevel) : 0.f; }

	/** Add XP and handle level-up(s).  Called by AHSXPOrb on collection. */
	void AddXP(float Amount);

	/** Called by HSCombatComponent when an attack swing starts. bIsHeavy selects
	 *  the heavy grunt pool; false uses the light pool (covers air + rising too). */
	void PlayAttackGrunt(bool bIsHeavy);

	/** Called by UHSEnemyCombatAI when an enemy attack connects.
	 *  Applies damage, brief screen flash, and a short invincibility window. */
	void ReceiveEnemyAttack(float Damage);

protected:
	/*****************************************************/
	/*                    Configurations                 */
	/*****************************************************/
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|HUD")
	TSubclassOf<UUserWidget> StyleHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|HUD")
	TSubclassOf<UUserWidget> PlayerHUDClass;

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

	/** Minimum distance from the locked target that forward movement input will close.
	 *  DMC3/FF16 "pocket": inside this radius, the toward-enemy component of movement
	 *  input is canceled so you orbit instead of ramming into the enemy's capsule.
	 *  Strafing and backing up still work. Tune to ~ playerCapsuleRadius + enemyCapsuleRadius + 50. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn", meta = (ClampMin = "0.0"))
	float LockOnMinDistance = 150.f;

	/** Width of the soft-zone outside LockOnMinDistance where the toward-enemy
	 *  component is gradually reduced (not fully canceled). 0 = hard clamp. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn", meta = (ClampMin = "0.0"))
	float LockOnApproachBuffer = 40.f;

	/** Minimum height the camera must stay above the player's feet during lock-on.
	 *  DMC3 pattern: camera never goes below this height to prevent terrain clipping. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|LockOn")
	float LockOnMinCameraHeight = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	TSubclassOf<class AHSHomingProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	float ProjectileDamage = 15.f;

	/** MP cost per projectile cast. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	float ProjectileMPCost = 30.f;

	/** Offset from actor location where the projectile spawns. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	FVector ProjectileSpawnOffset = FVector(80.f, 30.f, 50.f);

	/** Animation for the right arm when firing a projectile. Plays on UpperBody slot. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	UAnimMontage* ProjectileCastMontage;

	/** Sound played when Q fires -- the cast/launch sound on the player side. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Projectile")
	USoundBase* ProjectileCastSound = nullptr;

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

	/** Camera shake played when the player receives a hit from an enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combat")
	TSubclassOf<UCameraShakeBase> HitReceiveCameraShake;

	/** Scale for the receive-hit camera shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combat")
	float HitReceiveShakeScale = 0.8f;

	/** How long the player is invincible after receiving a hit (prevents multi-hit from the same attack). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combat", meta = (ClampMin = "0.0"))
	float HitInvincibilityDuration = 0.5f;

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
	/*                      Footstep                     */
	/*****************************************************/

	/** Per-surface footstep VFX. Key = physical surface type (SurfaceType1..SurfaceTypeN),
	 *  value = Niagara system to spawn at the foot impact point. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep")
	TMap<TEnumAsByte<EPhysicalSurface>, UNiagaraSystem*> SurfaceFootstepVFX;

	/** Per-surface footstep SFX. Key = physical surface type, value = pool of sounds
	 *  (a random one is picked per step, so walking doesn't sound looped). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep")
	TMap<TEnumAsByte<EPhysicalSurface>, FHSFootstepSoundSet> SurfaceFootstepSFX;

	/** Fallback VFX used when the traced surface isn't in SurfaceFootstepVFX. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep")
	UNiagaraSystem* DefaultFootstepVFX = nullptr;

	/** Fallback SFX pool used when the traced surface isn't in SurfaceFootstepSFX. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep")
	TArray<USoundBase*> DefaultFootstepSFX;

	/** How far below the foot bone to trace when looking for the surface under the foot. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep", meta = (ClampMin = "1.0"))
	float FootstepTraceDistance = 60.f;

	/** How far ABOVE the foot bone to start the trace. Needs to be tall enough that the
	 *  trace start is above any thin surface the foot might clip into (like a water plane
	 *  the foot dips below during a sprint stride). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep", meta = (ClampMin = "0.0"))
	float FootstepTraceStartHeight = 40.f;

	/** Volume multiplier for footstep SFX. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Footstep", meta = (ClampMin = "0.0"))
	float FootstepVolumeMultiplier = 1.f;

	/*****************************************************/
	/*                        BGM                        */
	/*****************************************************/

	/** Looping exploration music. Plays during normal traversal. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM")
	USoundBase* BGMTrack = nullptr;

	/** Looping combat music. Crossfades in when enemies are nearby. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM")
	USoundBase* CombatBGMTrack = nullptr;

	/** Volume for the exploration BGM track. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM", meta = (ClampMin = "0.0"))
	float BGMVolume = 0.6f;

	/** Volume for the combat BGM track. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM", meta = (ClampMin = "0.0"))
	float CombatBGMVolume = 0.6f;

	/** Seconds to fade the exploration BGM in at level start. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM", meta = (ClampMin = "0.0"))
	float BGMFadeInDuration = 2.f;

	/** How long the crossfade between exploration and combat tracks takes. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM", meta = (ClampMin = "0.0"))
	float BGMCrossfadeDuration = 1.5f;

	/** Radius around the player to check for living enemies. Combat music
	 *  kicks in when at least one enemy is within this range. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM", meta = (ClampMin = "0.0"))
	float CombatMusicRange = 1500.f;

	/** How long (seconds) to stay in combat music after the last nearby enemy
	 *  dies or leaves range -- prevents rapid ping-ponging on the edge. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|BGM", meta = (ClampMin = "0.0"))
	float CombatMusicLingerTime = 4.f;

	/*****************************************************/
	/*                   Attack Grunts                   */
	/*****************************************************/

	/** Voice grunts for light/air/rising attacks. One is picked at random per swing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Grunts")
	TArray<USoundBase*> LightAttackGrunts;

	/** Voice grunts for heavy attacks. One is picked at random per swing. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Grunts")
	TArray<USoundBase*> HeavyAttackGrunts;

	/** Volume multiplier for grunt sounds. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Grunts", meta = (ClampMin = "0.0"))
	float GruntVolumeMultiplier = 1.f;

	/*****************************************************/
	/*                    Player Stats                   */
	/*****************************************************/

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Stats", meta = (ClampMin = "1"))
	float MaxHealth = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Stats", meta = (ClampMin = "1"))
	float MaxMP = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Stats", meta = (ClampMin = "1"))
	int32 PlayerLevel = 1;

	/** XP required to level up at level 1.  Each subsequent level scales by XPScalePerLevel. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Stats", meta = (ClampMin = "1.0"))
	float BaseXPToLevel = 100.f;

	/** Multiplier applied to XPToNextLevel on each level-up. 1.5 = 50% more XP per level. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Stats", meta = (ClampMin = "1.0"))
	float XPScalePerLevel = 1.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Stats")
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Stats")
	float CurrentMP = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Stats")
	float CurrentXP = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Stats")
	float XPToNextLevel = 0.f;

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

	/** True for the duration of the second (air) jump until landing. Drives the double-jump ABP state. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bIsDoubleJumping = false;

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

	/** Proximity check fired on a repeating timer -- switches music when enemies enter/leave range. */
	void UpdateCombatMusicState();
	void EnterCombatMusic();
	void ExitCombatMusic();

	bool bIsInCombatMusic = false;

	/** Repeating timer for the proximity check (every 0.75s). */
	FTimerHandle CombatMusicCheckHandle;

	/** Delays the switch back to exploration music after enemies leave range. */
	FTimerHandle CombatLingerHandle;

	/** Set during the invincibility window after the player is hit. */
	bool bIsInvincible = false;
	FTimerHandle InvincibilityTimerHandle;

	FVector ResolveCameraRelativeInputDirection() const;
	UAnimMontage* GetDodgeMontage(int32 Index) const;
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool TryCancelDodgeFromInput(const FVector2D& Input);
};
