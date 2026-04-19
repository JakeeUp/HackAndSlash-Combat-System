// FF16-style player status HUD: character portrait, HP bar, MP bar, level display.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSPlayerHUD.generated.h"


class AHSPlayerCharacter;
class UHSXPRingWidget;


UCLASS()
class HACKSLASHMOVEMENT_API UHSPlayerHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/*****************************************************/
	/*       Bind these to your WBP widgets              */
	/*****************************************************/

	/** Health bar. Bind to a ProgressBar widget. */
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

	/** MP bar. Bind to a ProgressBar widget. */
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* MPBar;

	/** "Lv. X" text. Bind to a TextBlock widget. */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* LevelText;

	/** "HP XXXX" text. Bind to a TextBlock widget. */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* HPText;

	/** Character portrait image. Bind to an Image widget. */
	UPROPERTY(meta = (BindWidget))
	class UImage* CharacterPortrait;

	/** XP ring surrounding the portrait.  Bind to a UHSXPRingWidget widget named XPRing.
	 *  Optional -- HUD compiles without it if you haven't added it yet. */
	UPROPERTY(meta = (BindWidgetOptional))
	class UHSXPRingWidget* XPRing;

	/*****************************************************/
	/*                   Config                          */
	/*****************************************************/

	/** How fast the HP bar interpolates toward the actual value. */
	UPROPERTY(EditDefaultsOnly, Category = "Player HUD")
	float HealthInterpSpeed = 5.f;

	/** How fast the MP bar interpolates toward the actual value. */
	UPROPERTY(EditDefaultsOnly, Category = "Player HUD")
	float MPInterpSpeed = 8.f;

	/*****************************************************/
	/*              FF16 Movement Sway                   */
	/*****************************************************/

	/** Max pixel offset the HUD sways opposite to player movement. */
	UPROPERTY(EditDefaultsOnly, Category = "Player HUD|Sway")
	float SwayMaxOffset = 15.f;

	/** How fast the sway interpolates toward the target offset. */
	UPROPERTY(EditDefaultsOnly, Category = "Player HUD|Sway")
	float SwayInterpSpeed = 6.f;

	/*****************************************************/
	/*              Hit Pulse                             */
	/*****************************************************/

	/** Scale the HUD pulses to when the player takes damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Player HUD|Pulse")
	float DamagePulseScale = 1.15f;

	/** How fast the scale snaps back to 1.0 after a pulse. */
	UPROPERTY(EditDefaultsOnly, Category = "Player HUD|Pulse")
	float ScaleRecoverSpeed = 8.f;

private:
	UPROPERTY()
	AHSPlayerCharacter* CachedPlayer;

	float DisplayedHealthPercent = 1.f;
	float DisplayedMPPercent = 1.f;
	float LastHealthPercent = 1.f;

	// Sway state
	FVector2D CurrentSwayOffset = FVector2D::ZeroVector;

	// Scale state (pulse)
	float CurrentScale = 1.f;
};
