// Style Rank system -- DMC-style meter that rewards varied, aggressive play.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSStyleComponent.generated.h"


UENUM(BlueprintType)
enum class EStyleRank : uint8
{
	D   UMETA(DisplayName = "D - Dull"),
	C   UMETA(DisplayName = "C - Crazy"),
	B   UMETA(DisplayName = "B - Blast"),
	A   UMETA(DisplayName = "A - Atomic"),
	S   UMETA(DisplayName = "S - Showtime"),
	SS  UMETA(DisplayName = "SS - Sensational"),
	SSS UMETA(DisplayName = "SSS - Smokin Sexy Style")
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStyleRankChanged, EStyleRank, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboCountChanged, int32, NewCount);


UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HACKSLASHMOVEMENT_API UHSStyleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSStyleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/*****************************************************/
	/*                   Score Events                    */
	/*****************************************************/

	/** Call when an attack lands on an enemy. */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void RegisterHit(float DamageDealt);

	/** Call when the player dodges an attack at close range (just-dodge / table-hopper). */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void RegisterDodge();

	/** Call when the player takes damage -- drops rank hard. */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void RegisterDamageTaken();

	/** Call when a combo chain ends naturally (attack finished notify). */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void RegisterComboFinisher(int32 ChainLength);

	/*****************************************************/
	/*                     Getters                       */
	/*****************************************************/

	UFUNCTION(BlueprintPure, Category = "Style")
	FORCEINLINE EStyleRank GetCurrentRank() const { return CurrentRank; }

	UFUNCTION(BlueprintPure, Category = "Style")
	FORCEINLINE float GetStylePoints() const { return StylePoints; }

	UFUNCTION(BlueprintPure, Category = "Style")
	FORCEINLINE float GetStylePointsNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Style")
	FORCEINLINE int32 GetComboCount() const { return ComboCount; }

	UFUNCTION(BlueprintPure, Category = "Style")
	FORCEINLINE float GetComboTimer() const { return ComboTimer; }

	/*****************************************************/
	/*                   Delegates                       */
	/*****************************************************/

	UPROPERTY(BlueprintAssignable, Category = "Style")
	FOnStyleRankChanged OnStyleRankChanged;

	UPROPERTY(BlueprintAssignable, Category = "Style")
	FOnComboCountChanged OnComboCountChanged;

protected:
	/*****************************************************/
	/*                 Configurations                    */
	/*****************************************************/

	/** Points required to reach each rank. Index maps to EStyleRank (D=0 needs 0, C=1 needs threshold[1], etc). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Thresholds")
	TArray<float> RankThresholds;

	/** Base points awarded per hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Scoring")
	float HitBasePoints = 50.f;

	/** Multiplier bonus for using a different attack type than the previous hit (rewards variety). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Scoring")
	float VarietyMultiplier = 1.5f;

	/** Points awarded for a close-range dodge. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Scoring")
	float DodgePoints = 150.f;

	/** Points deducted when taking damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Scoring")
	float DamagePenalty = 500.f;

	/** Bonus points per hit in the combo chain when a combo finisher lands. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Scoring")
	float ComboFinisherPerHit = 25.f;

	/** Points lost per second (idle decay). Higher ranks decay faster. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Decay")
	float BaseDecayRate = 30.f;

	/** Decay multiplier per rank tier above D. So rank S (index 4) decays at BaseDecayRate * (1 + 4 * DecayScalePerRank). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Decay")
	float DecayScalePerRank = 0.4f;

	/** Seconds of no hits before the combo counter resets. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Combo")
	float ComboTimeout = 3.f;

	/*****************************************************/
	/*                      State                        */
	/*****************************************************/

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	EStyleRank CurrentRank = EStyleRank::D;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	float StylePoints = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	int32 ComboCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	float ComboTimer = 0.f;

	/** Tracks the last attack type that scored a hit, for variety bonus. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	uint8 LastHitAttackType = 255;

private:
	void AddPoints(float Points);
	void UpdateRank();
	float GetMaxPointsForCurrentRank() const;
};
