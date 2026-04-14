// Homing projectile that tracks the locked-on target (FF16 fire bolt style).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSHomingProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UCameraShakeBase;


UCLASS()
class HACKSLASHMOVEMENT_API AHSHomingProjectile : public AActor
{
	GENERATED_BODY()

public:
	AHSHomingProjectile();

	/** Set the homing target and the actor who fired this projectile. */
	void FireAtTarget(AActor* Target, AActor* Instigator, float Damage);

protected:
	UPROPERTY(VisibleAnywhere)
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere)
	UProjectileMovementComponent* ProjectileMovement;

	/** Visible glowing sphere mesh. Assign a sphere mesh + emissive material in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* ProjectileMesh;

	/** Point light for the glow effect. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UPointLightComponent* GlowLight;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float ProjectileSpeed = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float HomingAcceleration = 10000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float LifeSpan = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float ProjectileRadius = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	FLinearColor GlowColor = FLinearColor(0.2f, 0.5f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float GlowIntensity = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MeshScale = 0.3f;

	/** Camera shake on projectile impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Camera Shake")
	TSubclassOf<UCameraShakeBase> ImpactCameraShake;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Camera Shake")
	float ImpactShakeScale = 0.4f;

private:
	float CachedDamage = 0.f;
	AActor* CachedInstigator = nullptr;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyDamageToActor(AActor* Target);
};
