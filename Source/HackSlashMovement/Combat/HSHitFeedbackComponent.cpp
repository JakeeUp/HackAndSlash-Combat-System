#include "HSHitFeedbackComponent.h"

#include "Character/HSPlayerCharacter.h"
#include "Combat/HSCombatComponent.h"
#include "Combat/HSDamageable.h"
#include "Combat/HSDynamicCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UHSHitFeedbackComponent::UHSHitFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSHitFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<AHSPlayerCharacter>(GetOwner());

	if (OwnerChar && OwnerChar->FollowCamera)
	{
		DefaultCameraFOV = OwnerChar->FollowCamera->FieldOfView;
	}
}

void UHSHitFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

/*****************************************************/
/*                   Hit Response                    */
/*****************************************************/

void UHSHitFeedbackComponent::OnHitLanded(EAttackType AttackType, EHitWeight HitWeight, const FVector& SwingDirection)
{
	if (!OwnerChar) return;

	if (Config.CameraShake)
	{
		float ShakeScale = Config.LightShakeScale;
		if (AttackType == EAttackType::EAT_Heavy) ShakeScale = Config.HeavyShakeScale;
		else if (AttackType == EAttackType::EAT_Air) ShakeScale = Config.AirShakeScale;

		if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
		{
			PC->ClientStartCameraShake(Config.CameraShake, ShakeScale);
		}
	}

	CurrentFOVCompression = FMath::Min(CurrentFOVCompression + Config.FOVCompressionPerHit, Config.MaxFOVCompression);

	if (UHSDynamicCameraComponent* DynCam = OwnerChar->GetDynamicCamera())
	{
		float TraumaAmount = 0.12f;
		float PitchKick    = 0.f;
		float KickHoriz    = 8.f;
		float KickUp       = 2.f;

		switch (HitWeight)
		{
		case EHitWeight::EHW_Light:    TraumaAmount = 0.12f; KickHoriz = 8.f;  KickUp = 2.f; break;
		case EHitWeight::EHW_Heavy:    TraumaAmount = 0.22f; KickHoriz = 14.f; KickUp = 4.f; break;
		case EHitWeight::EHW_Finisher: TraumaAmount = 0.30f; PitchKick = 3.f; KickHoriz = 20.f; KickUp = 6.f; break;
		case EHitWeight::EHW_Launcher: TraumaAmount = 0.30f; PitchKick = 6.f; KickHoriz = 22.f; KickUp = 8.f; break;
		default: break;
		}

		if (AttackType == EAttackType::EAT_Heavy)
		{
			TraumaAmount = FMath::Min(TraumaAmount + 0.04f, 1.f);
			KickHoriz   += 3.f;
		}

		DynCam->AddTrauma(TraumaAmount);
		if (PitchKick > 0.f)
		{
			DynCam->AddPitchKick(PitchKick);
		}

		const FVector KickImpulse = (-SwingDirection * KickHoriz)
		                          + FVector(0.f, 0.f, KickUp);
		DynCam->AddPositionalKick(KickImpulse);
	}
}

/*****************************************************/
/*                  FOV Compression                  */
/*****************************************************/

void UHSHitFeedbackComponent::UpdateFOVCompression(float DeltaTime)
{
	if (!OwnerChar || !OwnerChar->FollowCamera) return;

	CurrentFOVCompression = FMath::FInterpTo(CurrentFOVCompression, 0.f, DeltaTime, Config.FOVRecoverySpeed);
	OwnerChar->FollowCamera->FieldOfView = DefaultCameraFOV - CurrentFOVCompression;
}

/*****************************************************/
/*                  Screen Effects                   */
/*****************************************************/

void UHSHitFeedbackComponent::ApplyScreenHitEffect()
{
	if (!OwnerChar) return;

	APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController());
	if (!PC) return;

	if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
	{
		CamMgr->StartCameraFade(Config.FlashIntensity, 0.f, Config.FlashDuration, FLinearColor::White, false, true);
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, Config.TimeDilationScale);

		World->GetTimerManager().ClearTimer(TimeDilationHandle);
		World->GetTimerManager().SetTimer(TimeDilationHandle, this, &UHSHitFeedbackComponent::RestoreTimeDilation, Config.TimeDilationDuration, false);
	}
}

void UHSHitFeedbackComponent::RestoreTimeDilation()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
}
