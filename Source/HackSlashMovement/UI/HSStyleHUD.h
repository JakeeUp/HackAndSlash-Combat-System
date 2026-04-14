// HUD widget base class for the DMC-style style rank + combo counter display.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/HSStyleComponent.h"
#include "HSStyleHUD.generated.h"


class AHSPlayerCharacter;


UCLASS()
class HACKSLASHMOVEMENT_API UHSStyleHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/*****************************************************/
	/*       Bind these to your WBP widgets              */
	/*****************************************************/

	/** The big style rank letter (D, C, B, A, S, SS, SSS). Bind to a Text widget. */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* RankText;

	/** Combo hit counter number. Bind to a Text widget. */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ComboCountText;

	/** "HITS" label below the combo count. Bind to a Text widget. */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ComboLabel;

	/** Progress bar showing how close you are to the next rank. Bind to a ProgressBar widget. */
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* RankProgressBar;

	/*****************************************************/
	/*                   Config                          */
	/*****************************************************/

	/** Map of rank enum to display string. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD")
	TMap<EStyleRank, FString> RankDisplayNames;

	/** How long the combo counter stays visible after the last hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD")
	float ComboFadeDelay = 2.f;

	/*****************************************************/
	/*              FF16 Movement Sway                   */
	/*****************************************************/

	/** Max pixel offset the HUD sways opposite to player movement. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD|Sway")
	float SwayMaxOffset = 15.f;

	/** How fast the sway interpolates toward the target offset. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD|Sway")
	float SwayInterpSpeed = 6.f;

	/*****************************************************/
	/*              DMC Rank Slam + Hit Pulse            */
	/*****************************************************/

	/** Scale the HUD slams to on rank change. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD|Slam")
	float RankSlamScale = 1.4f;

	/** Scale the HUD pulses to on each hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD|Slam")
	float HitPulseScale = 1.1f;

	/** Scale for the 10-hit milestone slam. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD|Slam")
	float MilestoneSlamScale = 1.25f;

	/** How fast the scale snaps back to 1.0 after a slam/pulse. */
	UPROPERTY(EditDefaultsOnly, Category = "Style HUD|Slam")
	float ScaleRecoverSpeed = 8.f;

private:
	UPROPERTY()
	UHSStyleComponent* CachedStyle;

	UPROPERTY()
	AHSPlayerCharacter* CachedPlayer;

	float ComboVisibleTimer = 0.f;
	int32 LastComboCount = 0;

	// Sway state
	FVector2D CurrentSwayOffset = FVector2D::ZeroVector;

	// Scale state (slam/pulse)
	float CurrentScale = 1.f;
	EStyleRank LastRank = EStyleRank::D;

	UFUNCTION()
	void OnRankChanged(EStyleRank NewRank);

	UFUNCTION()
	void OnComboChanged(int32 NewCount);

	void UpdateRankDisplay();
	void UpdateComboDisplay(int32 Count);

	FString GetRankString(EStyleRank Rank) const;
};
