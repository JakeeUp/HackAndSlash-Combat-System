// HUD widget base class for the DMC-style style rank + combo counter display.


#include "HSStyleHUD.h"

#include "Character/HSPlayerCharacter.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"


void UHSStyleHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Default rank display names
	if (RankDisplayNames.Num() == 0)
	{
		RankDisplayNames.Add(EStyleRank::D,   TEXT("D"));
		RankDisplayNames.Add(EStyleRank::C,   TEXT("C"));
		RankDisplayNames.Add(EStyleRank::B,   TEXT("B"));
		RankDisplayNames.Add(EStyleRank::A,   TEXT("A"));
		RankDisplayNames.Add(EStyleRank::S,   TEXT("S"));
		RankDisplayNames.Add(EStyleRank::SS,  TEXT("SS"));
		RankDisplayNames.Add(EStyleRank::SSS, TEXT("SSS"));
	}

	// Find the player's style component
	CachedPlayer = Cast<AHSPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (CachedPlayer)
	{
		CachedStyle = CachedPlayer->GetStyle();

		if (CachedStyle)
		{
			CachedStyle->OnStyleRankChanged.AddDynamic(this, &UHSStyleHUD::OnRankChanged);
			CachedStyle->OnComboCountChanged.AddDynamic(this, &UHSStyleHUD::OnComboChanged);
		}
	}

	// Initial state
	UpdateRankDisplay();
	UpdateComboDisplay(0);
}

void UHSStyleHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update progress bar smoothly
	if (CachedStyle && RankProgressBar)
	{
		RankProgressBar->SetPercent(CachedStyle->GetStylePointsNormalized());
	}

	// Fade combo display after timeout
	if (ComboVisibleTimer > 0.f)
	{
		ComboVisibleTimer -= InDeltaTime;
		if (ComboVisibleTimer <= 0.f)
		{
			UpdateComboDisplay(0);
		}
	}

	// --- FF16 Movement Sway ---
	// Shift the HUD opposite to the player's horizontal velocity
	FVector2D TargetSway = FVector2D::ZeroVector;
	if (CachedPlayer)
	{
		const FVector Velocity = CachedPlayer->GetVelocity();
		// Get camera-relative horizontal direction
		if (APlayerController* PC = Cast<APlayerController>(CachedPlayer->GetController()))
		{
			const FRotator CamRot(0.f, PC->GetControlRotation().Yaw, 0.f);
			const FVector CamRight = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Y);
			const FVector CamForward = FRotationMatrix(CamRot).GetUnitAxis(EAxis::X);

			// Project velocity onto camera axes and invert (HUD moves opposite)
			const float RightAmount = FVector::DotProduct(Velocity, CamRight);
			const float ForwardAmount = FVector::DotProduct(Velocity, CamForward);

			// Normalize by max walk speed so the sway is proportional
			const float MaxSpeed = CachedPlayer->GetCharacterMovement()->MaxWalkSpeed;
			if (MaxSpeed > 0.f)
			{
				TargetSway.X = -FMath::Clamp(RightAmount / MaxSpeed, -1.f, 1.f) * SwayMaxOffset;
				TargetSway.Y = -FMath::Clamp(ForwardAmount / MaxSpeed, -1.f, 1.f) * SwayMaxOffset * 0.5f;
			}
		}
	}

	CurrentSwayOffset = FMath::Vector2DInterpTo(CurrentSwayOffset, TargetSway, InDeltaTime, SwayInterpSpeed);

	// --- Scale recovery (slam/pulse snap back to 1.0) ---
	CurrentScale = FMath::FInterpTo(CurrentScale, 1.f, InDeltaTime, ScaleRecoverSpeed);

	// Apply sway + scale to the widget's render transform
	SetRenderTranslation(FVector2D(CurrentSwayOffset.X, CurrentSwayOffset.Y));
	SetRenderScale(FVector2D(CurrentScale, CurrentScale));
}

void UHSStyleHUD::OnRankChanged(EStyleRank NewRank)
{
	UpdateRankDisplay();

	// Only slam on rank UP, not down
	if (NewRank > LastRank)
	{
		CurrentScale = RankSlamScale;
	}
	LastRank = NewRank;
}

void UHSStyleHUD::OnComboChanged(int32 NewCount)
{
	if (NewCount > 0)
	{
		ComboVisibleTimer = ComboFadeDelay;
		UpdateComboDisplay(NewCount);

		// Hit pulse on every hit
		if (CurrentScale < HitPulseScale)
		{
			CurrentScale = HitPulseScale;
		}

		// 10-hit milestone slam (DMC style)
		if (NewCount % 10 == 0)
		{
			CurrentScale = MilestoneSlamScale;
		}
	}
	else
	{
		// Don't hide immediately -- let the timer handle fade
		ComboVisibleTimer = 0.5f;
	}

	LastComboCount = NewCount;
}

void UHSStyleHUD::UpdateRankDisplay()
{
	if (!CachedStyle) return;

	const EStyleRank Rank = CachedStyle->GetCurrentRank();

	if (RankText)
	{
		RankText->SetText(FText::FromString(GetRankString(Rank)));
	}
}

void UHSStyleHUD::UpdateComboDisplay(int32 Count)
{
	if (ComboCountText)
	{
		if (Count > 0)
		{
			ComboCountText->SetText(FText::AsNumber(Count));
			ComboCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ComboCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ComboLabel)
	{
		ComboLabel->SetVisibility(Count > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FString UHSStyleHUD::GetRankString(EStyleRank Rank) const
{
	if (const FString* Found = RankDisplayNames.Find(Rank))
	{
		return *Found;
	}

	// Fallback
	switch (Rank)
	{
	case EStyleRank::D:   return TEXT("D");
	case EStyleRank::C:   return TEXT("C");
	case EStyleRank::B:   return TEXT("B");
	case EStyleRank::A:   return TEXT("A");
	case EStyleRank::S:   return TEXT("S");
	case EStyleRank::SS:  return TEXT("SS");
	case EStyleRank::SSS: return TEXT("SSS");
	default:              return TEXT("?");
	}
}
