// HUD widget base class for the DMC-style style rank + combo counter display.


#include "HSStyleHUD.h"

#include "Character/HSPlayerCharacter.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"


void UHSStyleHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Default fill colors (DMC gradient: blue tiers → gold tiers)
	if (RankFillColors.Num() == 0)
	{
		RankFillColors.Add(EStyleRank::D,   FLinearColor(0.3f, 0.4f, 0.6f));    // Steel blue
		RankFillColors.Add(EStyleRank::C,   FLinearColor(0.2f, 0.5f, 1.0f));    // Blue
		RankFillColors.Add(EStyleRank::B,   FLinearColor(0.3f, 0.6f, 1.0f));    // Bright blue
		RankFillColors.Add(EStyleRank::A,   FLinearColor(0.8f, 0.7f, 0.2f));    // Gold transition
		RankFillColors.Add(EStyleRank::S,   FLinearColor(1.0f, 0.85f, 0.1f));   // Gold
		RankFillColors.Add(EStyleRank::SS,  FLinearColor(1.0f, 0.8f, 0.0f));    // Deep gold
		RankFillColors.Add(EStyleRank::SSS, FLinearColor(1.0f, 0.75f, 0.0f));   // Rich gold
	}

	if (RankOutlineColors.Num() == 0)
	{
		// Per-rank dark-complement outlines: darker, slightly-shifted sibling of
		// each rank's fill color. Keeps the cool → warm gradient alive without
		// making outlines fight the fill.
		RankOutlineColors.Add(EStyleRank::D,   FLinearColor(0.08f, 0.1f, 0.15f));   // dark slate
		RankOutlineColors.Add(EStyleRank::C,   FLinearColor(0.05f, 0.1f, 0.2f));    // dark navy
		RankOutlineColors.Add(EStyleRank::B,   FLinearColor(0.05f, 0.12f, 0.25f));  // deep blue
		RankOutlineColors.Add(EStyleRank::A,   FLinearColor(0.2f, 0.15f, 0.02f));   // dark amber
		RankOutlineColors.Add(EStyleRank::S,   FLinearColor(0.25f, 0.18f, 0.0f));   // dark gold
		RankOutlineColors.Add(EStyleRank::SS,  FLinearColor(0.3f, 0.2f, 0.0f));     // bronze
		RankOutlineColors.Add(EStyleRank::SSS, FLinearColor(0.35f, 0.2f, 0.0f));    // deep bronze
	}

	// Create the dynamic material instance for the rank letter
	if (RankLetterMaterial && RankImage)
	{
		RankMaterialInstance = UMaterialInstanceDynamic::Create(RankLetterMaterial, this);
		RankImage->SetBrushFromMaterial(RankMaterialInstance);
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

	// Smoothly animate the fill percent on the rank letter
	if (CachedStyle && RankMaterialInstance)
	{
		const float TargetFill = CachedStyle->GetStylePointsNormalized();
		DisplayedFillPercent = FMath::FInterpTo(DisplayedFillPercent, TargetFill, InDeltaTime, 5.f);
		RankMaterialInstance->SetScalarParameterValue(TEXT("FillPercent"), DisplayedFillPercent);
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

	// FF16 Movement Sway
	FVector2D TargetSway = FVector2D::ZeroVector;
	if (CachedPlayer)
	{
		const FVector Velocity = CachedPlayer->GetVelocity();
		if (APlayerController* PC = Cast<APlayerController>(CachedPlayer->GetController()))
		{
			const FRotator CamRot(0.f, PC->GetControlRotation().Yaw, 0.f);
			const FVector CamRight = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Y);
			const FVector CamForward = FRotationMatrix(CamRot).GetUnitAxis(EAxis::X);

			const float RightAmount = FVector::DotProduct(Velocity, CamRight);
			const float ForwardAmount = FVector::DotProduct(Velocity, CamForward);

			const float MaxSpeed = CachedPlayer->GetCharacterMovement()->MaxWalkSpeed;
			if (MaxSpeed > 0.f)
			{
				TargetSway.X = -FMath::Clamp(RightAmount / MaxSpeed, -1.f, 1.f) * SwayMaxOffset;
				TargetSway.Y = -FMath::Clamp(ForwardAmount / MaxSpeed, -1.f, 1.f) * SwayMaxOffset * 0.5f;
			}
		}
	}

	CurrentSwayOffset = FMath::Vector2DInterpTo(CurrentSwayOffset, TargetSway, InDeltaTime, SwayInterpSpeed);

	// Scale recovery (slam/pulse snap back to 1.0)
	CurrentScale = FMath::FInterpTo(CurrentScale, 1.f, InDeltaTime, ScaleRecoverSpeed);

	// Apply sway + scale
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

	// Reset fill to 0 on rank change so it starts filling fresh
	DisplayedFillPercent = 0.f;
}

void UHSStyleHUD::OnComboChanged(int32 NewCount)
{
	if (NewCount > 0)
	{
		ComboVisibleTimer = ComboFadeDelay;
		UpdateComboDisplay(NewCount);

		if (CurrentScale < HitPulseScale)
		{
			CurrentScale = HitPulseScale;
		}

		// 10-hit milestone slam
		if (NewCount % 10 == 0)
		{
			CurrentScale = MilestoneSlamScale;
		}
	}
	else
	{
		ComboVisibleTimer = 0.5f;
	}

	LastComboCount = NewCount;
}

void UHSStyleHUD::UpdateRankDisplay()
{
	if (!CachedStyle || !RankMaterialInstance) return;

	const EStyleRank Rank = CachedStyle->GetCurrentRank();

	// Swap the letter texture
	if (const UTexture2D* const* Tex = RankTextures.Find(Rank))
	{
		RankMaterialInstance->SetTextureParameterValue(TEXT("LetterTexture"), const_cast<UTexture2D*>(*Tex));
	}

	// Set fill color
	if (const FLinearColor* Color = RankFillColors.Find(Rank))
	{
		RankMaterialInstance->SetVectorParameterValue(TEXT("FillColor"), *Color);
	}

	// Set outline color
	if (const FLinearColor* Color = RankOutlineColors.Find(Rank))
	{
		RankMaterialInstance->SetVectorParameterValue(TEXT("OutlineColor"), *Color);
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
