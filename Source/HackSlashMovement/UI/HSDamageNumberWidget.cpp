// Simple widget for displaying a damage number.


#include "HSDamageNumberWidget.h"

#include "Components/TextBlock.h"


void UHSDamageNumberWidget::SetDamageText(float Damage, FLinearColor Color)
{
	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
		DamageText->SetColorAndOpacity(FSlateColor(Color));
	}
}
