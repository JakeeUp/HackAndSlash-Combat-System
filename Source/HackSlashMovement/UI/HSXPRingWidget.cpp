// XP ring drawn around the character portrait in the HUD.

#include "HSXPRingWidget.h"

#include "Character/HSPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"


void UHSXPRingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		CachedPlayer = Cast<AHSPlayerCharacter>(PC->GetPawn());
	}

	if (CachedPlayer)
	{
		LastKnownLevel   = CachedPlayer->GetPlayerLevel();
		DisplayedProgress = CachedPlayer->GetXPPercent();
		Progress          = DisplayedProgress;
	}
}

void UHSXPRingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CachedPlayer) return;

	// Read target progress from the player.
	Progress = CachedPlayer->GetXPPercent();

	// Detect level-up: progress wraps back toward 0 and level increases.
	const int32 CurrentLevel = CachedPlayer->GetPlayerLevel();
	if (CurrentLevel > LastKnownLevel)
	{
		LastKnownLevel = CurrentLevel;
		PulseScale     = LevelUpPulseScale;

		// Snap display to 0 so the fill starts fresh for the new level.
		DisplayedProgress = 0.f;
	}

	// Smooth fill animation -- fast enough to feel reactive but not instant.
	DisplayedProgress = FMath::FInterpTo(DisplayedProgress, Progress, InDeltaTime, FillInterpSpeed);

	// Decay the level-up pulse back to neutral.
	if (PulseScale > 1.f)
	{
		PulseScale = FMath::FInterpTo(PulseScale, 1.f, InDeltaTime, LevelUpPulseDecay);
	}
}

int32 UHSXPRingWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size   = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;

	// Apply level-up pulse by shrinking the effective radius slightly.
	const float BaseRadius = FMath::Min(Size.X, Size.Y) * 0.5f - RingPadding;
	const float Radius     = BaseRadius / PulseScale;   // scale up = visually larger, achieved by reducing radius offset... actually let's just scale thickness
	// Simpler: keep radius the same, just make the ring thicker during pulse.
	const float PulsedThickness = Thickness * PulseScale;

	// Arc runs clockwise from 12 o'clock.
	// In Slate, Y increases downward, so clockwise from top = starting at -π/2 and increasing angle.
	constexpr float StartAngle = -PI * 0.5f;
	const float     EndAngle   = StartAngle + TWO_PI;

	// ── Background ring (full 360°) ──────────────────────────────────────────
	{
		TArray<FVector2D> BgPoints = BuildArcPoints(Center, BaseRadius, StartAngle, EndAngle, Segments);
		FSlateDrawElement::MakeLines(
			OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(),
			BgPoints,
			ESlateDrawEffect::None,
			BackgroundColor,
			true,
			BackgroundThickness
		);
	}

	if (DisplayedProgress > 0.001f)
	{
		const float FillEnd = StartAngle + TWO_PI * FMath::Clamp(DisplayedProgress, 0.f, 1.f);
		const int32 FillSegs = FMath::Max(2, FMath::RoundToInt(static_cast<float>(Segments) * FMath::Clamp(DisplayedProgress, 0.f, 1.f)));

		// ── Glow pass (wider, transparent) ──────────────────────────────────
		if (bDrawGlow)
		{
			TArray<FVector2D> GlowPoints = BuildArcPoints(Center, BaseRadius, StartAngle, FillEnd, FillSegs);
			FLinearColor GlowColor = FillColor;
			GlowColor.A = GlowOpacity;
			FSlateDrawElement::MakeLines(
				OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(),
				GlowPoints,
				ESlateDrawEffect::None,
				GlowColor,
				true,
				PulsedThickness + GlowExtraThickness
			);
		}

		// ── Fill arc ─────────────────────────────────────────────────────────
		{
			TArray<FVector2D> FillPoints = BuildArcPoints(Center, BaseRadius, StartAngle, FillEnd, FillSegs);
			FSlateDrawElement::MakeLines(
				OutDrawElements, Layer + 1,
				AllottedGeometry.ToPaintGeometry(),
				FillPoints,
				ESlateDrawEffect::None,
				FillColor,
				true,
				PulsedThickness
			);
		}
	}

	return Layer + 2;
}

TArray<FVector2D> UHSXPRingWidget::BuildArcPoints(
	FVector2D Center,
	float Radius,
	float StartAngle,
	float EndAngle,
	int32 NumSegs)
{
	TArray<FVector2D> Points;
	Points.Reserve(NumSegs + 1);

	for (int32 i = 0; i <= NumSegs; ++i)
	{
		const float T     = static_cast<float>(i) / static_cast<float>(NumSegs);
		const float Angle = FMath::Lerp(StartAngle, EndAngle, T);
		Points.Emplace(
			Center.X + Radius * FMath::Cos(Angle),
			Center.Y + Radius * FMath::Sin(Angle)
		);
	}

	return Points;
}
