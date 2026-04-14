// Style Rank system -- DMC-style meter that rewards varied, aggressive play.


#include "HSStyleComponent.h"


UHSStyleComponent::UHSStyleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Default rank thresholds: D=0, C=200, B=500, A=1000, S=1800, SS=2800, SSS=4000
	RankThresholds = { 0.f, 200.f, 500.f, 1000.f, 1800.f, 2800.f, 4000.f };
}

void UHSStyleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Combo timeout
	if (ComboCount > 0)
	{
		ComboTimer -= DeltaTime;
		if (ComboTimer <= 0.f)
		{
			ComboCount = 0;
			ComboTimer = 0.f;
			OnComboCountChanged.Broadcast(0);
		}
	}

	// Style point decay -- higher ranks decay faster to pressure the player
	if (StylePoints > 0.f)
	{
		const int32 RankIndex = static_cast<int32>(CurrentRank);
		const float Decay = BaseDecayRate * (1.f + RankIndex * DecayScalePerRank);
		StylePoints = FMath::Max(0.f, StylePoints - Decay * DeltaTime);
		UpdateRank();
	}
}

void UHSStyleComponent::RegisterHit(float DamageDealt)
{
	// Combo counter
	ComboCount++;
	ComboTimer = ComboTimeout;
	OnComboCountChanged.Broadcast(ComboCount);

	// Base points with variety bonus
	float Points = HitBasePoints;

	// Check if this attack type differs from the last -- reward variety like DMC
	const uint8 CurrentType = static_cast<uint8>(ComboCount % 3); // Simple variety proxy
	if (LastHitAttackType != 255 && CurrentType != LastHitAttackType)
	{
		Points *= VarietyMultiplier;
	}
	LastHitAttackType = CurrentType;

	AddPoints(Points);
}

void UHSStyleComponent::RegisterDodge()
{
	AddPoints(DodgePoints);
}

void UHSStyleComponent::RegisterDamageTaken()
{
	StylePoints = FMath::Max(0.f, StylePoints - DamagePenalty);
	UpdateRank();

	// Reset combo
	ComboCount = 0;
	ComboTimer = 0.f;
	OnComboCountChanged.Broadcast(0);
}

void UHSStyleComponent::RegisterComboFinisher(int32 ChainLength)
{
	if (ChainLength > 1)
	{
		AddPoints(ComboFinisherPerHit * ChainLength);
	}
}

float UHSStyleComponent::GetStylePointsNormalized() const
{
	const int32 RankIndex = static_cast<int32>(CurrentRank);
	const int32 NextIndex = RankIndex + 1;

	if (!RankThresholds.IsValidIndex(NextIndex))
	{
		// Already at max rank -- normalize within last tier
		const float LastThreshold = RankThresholds.IsValidIndex(RankIndex) ? RankThresholds[RankIndex] : 0.f;
		const float Range = (RankIndex > 0 && RankThresholds.IsValidIndex(RankIndex - 1))
			? (LastThreshold - RankThresholds[RankIndex - 1])
			: 1000.f;
		return FMath::Clamp((StylePoints - LastThreshold) / Range, 0.f, 1.f);
	}

	const float Low = RankThresholds[RankIndex];
	const float High = RankThresholds[NextIndex];
	return FMath::Clamp((StylePoints - Low) / (High - Low), 0.f, 1.f);
}

void UHSStyleComponent::AddPoints(float Points)
{
	StylePoints += Points;
	UpdateRank();
}

void UHSStyleComponent::UpdateRank()
{
	EStyleRank NewRank = EStyleRank::D;

	for (int32 i = RankThresholds.Num() - 1; i >= 0; i--)
	{
		if (StylePoints >= RankThresholds[i])
		{
			NewRank = static_cast<EStyleRank>(i);
			break;
		}
	}

	if (NewRank != CurrentRank)
	{
		CurrentRank = NewRank;
		OnStyleRankChanged.Broadcast(CurrentRank);
	}
}

float UHSStyleComponent::GetMaxPointsForCurrentRank() const
{
	const int32 NextIndex = static_cast<int32>(CurrentRank) + 1;
	if (RankThresholds.IsValidIndex(NextIndex))
	{
		return RankThresholds[NextIndex];
	}
	return RankThresholds.Last() + 1000.f;
}
