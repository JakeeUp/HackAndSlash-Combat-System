// HUD widget base class for the DMC-style style rank + combo counter display.


#include "HSStyleHUD.h"

#include "Character/HSPlayerCharacter.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"


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
	if (AHSPlayerCharacter* Player = Cast<AHSPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		CachedStyle = Player->GetStyle();

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
}

void UHSStyleHUD::OnRankChanged(EStyleRank NewRank)
{
	UpdateRankDisplay();
}

void UHSStyleHUD::OnComboChanged(int32 NewCount)
{
	if (NewCount > 0)
	{
		ComboVisibleTimer = ComboFadeDelay;
		UpdateComboDisplay(NewCount);
	}
	else
	{
		// Don't hide immediately -- let the timer handle fade
		ComboVisibleTimer = 0.5f;
	}
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
