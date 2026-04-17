#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "HSEnemyAIController.generated.h"


/**
 * Minimal AIController for AHSDummyEnemy.
 *
 * All combat logic lives in UHSEnemyCombatAI.  This controller exists solely
 * to own the PathFollowingComponent so that MoveToActor / MoveToLocation
 * calls in the combat AI get NavMesh pathfinding for free.
 */
UCLASS()
class HACKSLASHMOVEMENT_API AHSEnemyAIController : public AAIController
{
	GENERATED_BODY()
};
