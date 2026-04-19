// Anim Notify -- spawns a ground-travelling energy wave (Dante "Drive" style).
// Place on the sword-swing frame of your chosen attack montage.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/HSDamageable.h"
#include "AN_DriveWave.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UAN_DriveWave : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Drive Wave"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** Blueprint class of the wave actor to spawn.  Assign BP_DriveWave in the notify properties. */
	UPROPERTY(EditAnywhere, Category = "Drive Wave")
	TSubclassOf<class AHSDriveWave> WaveClass;

	/** Damage dealt to each enemy the wave passes through. */
	UPROPERTY(EditAnywhere, Category = "Drive Wave", meta = (ClampMin = "0.0"))
	float DamageAmount = 60.f;

	/** Hit weight applied to each enemy hit.  Heavy gives a big stagger; Finisher sends them flying. */
	UPROPERTY(EditAnywhere, Category = "Drive Wave")
	EHitWeight HitWeight = EHitWeight::EHW_Heavy;
};
