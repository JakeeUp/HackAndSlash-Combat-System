#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "HSRandomSoundNotify.generated.h"

/**
 * General-purpose anim notify that plays one randomly chosen sound from a pool.
 * Stack multiple instances on the same frame to layer sounds simultaneously.
 * Works on any skeletal mesh, not just the player.
 */
UCLASS(meta = (DisplayName = "Play Random Sound"))
class HACKSLASHMOVEMENT_API UHSRandomSoundNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** Pool of sounds to pick from. One is chosen at random each time the notify fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TArray<USoundBase*> Sounds;

	/** Volume multiplier applied to the chosen sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.f;

	/** Pitch multiplier. Set a range via two notifies or leave at 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.1"))
	float PitchMultiplier = 1.f;

	/** If true, sound plays attached to the owner (moves with them).
	 *  If false, sound is spawned at the owner's location but stays in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	bool bFollowOwner = false;
};
