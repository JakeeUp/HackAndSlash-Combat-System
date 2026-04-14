// Floating damage number that spawns at hit location, floats up, and fades out.


#include "HSDamageNumber.h"
#include "HSDamageNumberWidget.h"

#include "Components/WidgetComponent.h"


AHSDamageNumber::AHSDamageNumber()
{
	PrimaryActorTick.bCanEverTick = true;

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	RootComponent = WidgetComp;

	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetDrawAtDesiredSize(true);
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->SetGenerateOverlapEvents(false);

	// Random float direction (mostly up, slight horizontal spread)
	FloatDirection = FVector(0.f, 0.f, 1.f);
}

void AHSDamageNumber::Initialize(float Damage, FLinearColor Color)
{
	// Random offset so numbers don't stack directly on top of each other
	const FVector Offset(
		FMath::FRandRange(-RandomSpreadX, RandomSpreadX),
		FMath::FRandRange(-RandomSpreadY, RandomSpreadY),
		FMath::FRandRange(0.f, 30.f)
	);
	SetActorLocation(GetActorLocation() + Offset);

	if (UHSDamageNumberWidget* DmgWidget = Cast<UHSDamageNumberWidget>(WidgetComp->GetWidget()))
	{
		DmgWidget->SetDamageText(Damage, Color);
	}
}

void AHSDamageNumber::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;

	// Float upward
	FVector Loc = GetActorLocation();
	Loc += FloatDirection * FloatSpeed * DeltaTime;
	SetActorLocation(Loc);

	// Fade out
	if (Age >= FadeStartAt && WidgetComp)
	{
		const float Alpha = 1.f - FMath::Clamp((Age - FadeStartAt) / (Lifetime - FadeStartAt), 0.f, 1.f);
		WidgetComp->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Alpha));
	}

	// Self-destruct
	if (Age >= Lifetime)
	{
		Destroy();
	}
}
