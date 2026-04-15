// FF16-style player status HUD: character portrait, HP bar, MP bar, level display.


#include "HSPlayerHUD.h"

#include "Character/HSPlayerCharacter.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"


void UHSPlayerHUD::NativeConstruct()
{
	Super::NativeConstruct();

	CachedPlayer = Cast<AHSPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));

	if (CachedPlayer)
	{
		DisplayedHealthPercent = CachedPlayer->GetHealthPercent();
		DisplayedMPPercent = CachedPlayer->GetMPPercent();
		LastHealthPercent = DisplayedHealthPercent;

		if (LevelText)
		{
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv. %d"), CachedPlayer->GetPlayerLevel())));
		}
	}
}

void UHSPlayerHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CachedPlayer) return;

	// Smooth HP bar
	const float TargetHP = CachedPlayer->GetHealthPercent();
	DisplayedHealthPercent = FMath::FInterpTo(DisplayedHealthPercent, TargetHP, InDeltaTime, HealthInterpSpeed);
	if (HealthBar)
	{
		HealthBar->SetPercent(DisplayedHealthPercent);
	}

	// Pulse when health drops
	if (TargetHP < LastHealthPercent - KINDA_SMALL_NUMBER)
	{
		CurrentScale = DamagePulseScale;
	}
	LastHealthPercent = TargetHP;

	// Smooth MP bar
	const float TargetMP = CachedPlayer->GetMPPercent();
	DisplayedMPPercent = FMath::FInterpTo(DisplayedMPPercent, TargetMP, InDeltaTime, MPInterpSpeed);
	if (MPBar)
	{
		MPBar->SetPercent(DisplayedMPPercent);
	}

	// HP number text
	if (HPText)
	{
		HPText->SetText(FText::FromString(FString::Printf(TEXT("HP  %d"), FMath::RoundToInt(CachedPlayer->GetCurrentHealth()))));
	}

	// FF16 Movement Sway — shift the HUD opposite to player velocity
	FVector2D TargetSway = FVector2D::ZeroVector;
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

	// Scale recovery (pulse snaps back to 1.0)
	CurrentScale = FMath::FInterpTo(CurrentScale, 1.f, InDeltaTime, ScaleRecoverSpeed);

	// Apply sway + scale
	SetRenderTranslation(FVector2D(CurrentSwayOffset.X, CurrentSwayOffset.Y));
	SetRenderScale(FVector2D(CurrentScale, CurrentScale));
}
