// Anim Notify -- triggers a timed cinematic camera shot (DmC-style hero shot) on the
// owning player.  Drop this on the exact animation frame you want the cinematic camera
// to kick in (e.g. mid-second-hit of a heavy combo, Drive wind-up, launcher apex).
//
// All cinematic parameters are configurable per-notify instance so the same class can
// drive different shots for different attacks.  Defaults are tuned to the DmC 2013
// "ground finisher / 2nd heavy" hero-shot look: low camera, wide FOV, slight dutch,
// dolly-in with a mild orbit over the hold phase so it doesn't feel frozen.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_CinematicCamera.generated.h"


UCLASS()
class HACKSLASHMOVEMENT_API UAN_CinematicCamera : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Cinematic Camera"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/*****************************************************/
	/*         Cinematic shot parameters                 */
	/*****************************************************/

	/** Arm length at the PEAK of the shot.  Low values (150-200) = close hero shot,
	 *  200-260 = medium (both characters visible), 280+ = wide establishing.
	 *  The camera will continue to push in during the hold via DollyArmDelta. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Framing", meta = (ClampMin = "50.0"))
	float ArmLength = 210.f;

	/** Socket offset at PEAK.
	 *  X = forward bias, Y = right (positive pushes the player to the LEFT third of frame),
	 *  Z = up (NEGATIVE = camera below player).  DmC 2013 hero shots sit at roughly
	 *  hip-to-waist level on the player, NOT ankle level.  Values:
	 *    -20 to -40  = waist/hip (default, looks heroic without eating the floor)
	 *    -50 to -70  = thigh/knee (more dramatic, use for finishers)
	 *    -80+        = shin/ankle (very extreme, min-camera-height safety may fight it). */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Framing")
	FVector SocketOffset = FVector(-30.f, 90.f, -30.f);

	/** FOV override at peak.  80-95 = cinematic wide, 60-70 = telephoto punch-in.  0 = don't override. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Framing", meta = (ClampMin = "0.0", ClampMax = "170.0"))
	float FOV = 92.f;

	/** Extra pitch (degrees) layered on top of the normal lock-on look.
	 *  NEGATIVE = tilt UP (hero shot looking upward at the action). */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Framing", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float PitchBias = -15.f;

	/** Dutch angle (roll) degrees.  DmC uses ±3-6° tied to swing direction.
	 *  Positive = tilts camera clockwise from viewer's perspective. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Framing", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float RollBias = 5.f;

	/*****************************************************/
	/*              Motion during hold                   */
	/*****************************************************/

	/** How much the arm length changes over the shot's duration (sine-eased).
	 *  NEGATIVE = dolly IN (the DmC preferred motion).  Default -25 = subtle push-in. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Motion")
	float DollyArmDelta = -25.f;

	/** How much the socket X advances over the duration (sine-eased).
	 *  Positive = the camera rig creeps forward during the shot. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Motion")
	float DollyXDelta = 20.f;

	/** Degrees of subtle yaw orbit over the duration (extra camera rotation offset).
	 *  5-10° gives the DmC "sweep" feel.  Sign matches roll direction for a consistent arc. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Motion", meta = (ClampMin = "-30.0", ClampMax = "30.0"))
	float OrbitYawDelta = 6.f;

	/*****************************************************/
	/*                    Timing                         */
	/*****************************************************/

	/** Seconds to blend in from the current framing.  Fast (0.08-0.12) = punchy cut-in. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Timing", meta = (ClampMin = "0.01"))
	float BlendIn = 0.1f;

	/** Seconds to hold at full cinematic framing.  DmC flourishes: 0.2-0.3.  KO/finisher shots: 0.4-0.5. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Timing", meta = (ClampMin = "0.0"))
	float Hold = 0.4f;

	/** Seconds to blend back to normal framing.  Slightly longer than blend-in feels natural. */
	UPROPERTY(EditAnywhere, Category = "Cinematic|Timing", meta = (ClampMin = "0.01"))
	float BlendOut = 0.3f;
};
