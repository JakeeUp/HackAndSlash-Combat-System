// XP ring drawn around the character portrait in the HUD.
// Drop this widget in WBP_PlayerHUD, size it to wrap the portrait,
// and it will auto-fill clockwise from 12 o'clock as XP comes in.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSXPRingWidget.generated.h"


class AHSPlayerCharacter;


UCLASS()
class HACKSLASHMOVEMENT_API UHSXPRingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Force a specific fill fraction (0–1).  Normally driven automatically from the player. */
	UFUNCTION(BlueprintCallable, Category = "XP Ring")
	void SetProgress(float InProgress) { Progress = FMath::Clamp(InProgress, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "XP Ring")
	float GetProgress() const { return Progress; }

protected:
	/*****************************************************/
	/*               Visual Configuration                */
	/*****************************************************/

	/** Color of the filled XP arc. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring")
	FLinearColor FillColor = FLinearColor(0.1f, 0.6f, 1.f, 1.f);    // cyan-blue

	/** Color of the background (empty) ring. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring")
	FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.55f);

	/** Line thickness of the XP fill arc (pixels). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "1.0"))
	float Thickness = 4.f;

	/** Thickness of the background ring (can be thinner for a subtler look). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "1.0"))
	float BackgroundThickness = 2.f;

	/** How many line segments approximate the full 360° circle.  More = smoother. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "12", ClampMax = "128"))
	int32 Segments = 64;

	/** Inset from the widget edge so the ring line doesn't clip the border. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "0.0"))
	float RingPadding = 4.f;

	/** Draw a slightly thicker, dimmer arc behind the fill arc to simulate a glow. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring")
	bool bDrawGlow = true;

	/** Extra thickness added on each side for the glow pass. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "0.0"))
	float GlowExtraThickness = 6.f;

	/** Opacity of the glow layer (0 = invisible, 1 = fully opaque). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlowOpacity = 0.25f;

	/** How fast the displayed progress interpolates toward the actual value.
	 *  Gives a smooth fill animation as XP orbs are collected. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "0.1"))
	float FillInterpSpeed = 5.f;

	/** Brief scale pulse played on the ring when the player levels up. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "1.0"))
	float LevelUpPulseScale = 1.2f;

	/** How fast the level-up scale pulse decays back to 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurations|Ring", meta = (ClampMin = "0.1"))
	float LevelUpPulseDecay = 10.f;

private:
	/** Current fill fraction (0–1), smoothly interpolated toward Progress. */
	float DisplayedProgress = 0.f;

	/** True fill fraction set from player data each tick. */
	float Progress = 0.f;

	/** Previous player level -- used to detect level-up events. */
	int32 LastKnownLevel = 0;

	/** Current scale multiplier for the level-up pulse effect. */
	float PulseScale = 1.f;

	UPROPERTY()
	AHSPlayerCharacter* CachedPlayer = nullptr;

	/** Build a list of arc points in local widget space.
	 *  @param Center      Widget center in local pixels.
	 *  @param Radius      Arc radius in pixels.
	 *  @param StartAngle  Start angle in radians (0 = right, -π/2 = top).
	 *  @param EndAngle    End angle in radians (clockwise from StartAngle).
	 *  @param NumSegs     Number of line segments to generate. */
	static TArray<FVector2D> BuildArcPoints(
		FVector2D Center,
		float Radius,
		float StartAngle,
		float EndAngle,
		int32 NumSegs);
};
