#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "LightkeeperAIController.generated.h"

class USensoryComponent;
class APatrolWaypoint;

UCLASS()
class LIGHTKEEPER_API ALightkeeperAIController : public AAIController
{
	GENERATED_BODY()

public:
	ALightkeeperAIController();

	void StartPatrol();
	void PauseAndFindNextPatrolPoint();

	void ChaseTargetActor(AActor* TargetActor, float AcceptanceRadius = 75.0f);
	void MoveToInvestigationPoint(FVector TargetLocation, float AcceptanceRadius = 50.0f);
	void FleeAwayFromThreat(FVector ThreatLocation);
	void SearchAreaAroundLocation(FVector CenterLocation, float SearchRadius = 700.0f);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	UPROPERTY(EditDefaultsOnly, Category = "Lightkeeper|AI|Patrol")
	float MinPatrolWaitTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lightkeeper|AI|Patrol")
	float MaxPatrolWaitTime = 4.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Lightkeeper|AI|Patrol")
	float PatrolRadius = 1200.0f;

private:
	// Zbuforowany komponent zmysłów (Zero-Cast Architecture):
	UPROPERTY()
	TWeakObjectPtr<USensoryComponent> CachedSensoryComp;

	FTimerHandle PatrolWaitTimerHandle;
	float LastChaseRepathTime = 0.0f;
	int32 CurrentWaypointIndex = 0;
	int32 ActiveTargetWaypointIndex = 0;
	int32 PatrolDirection = 1;

	void ExecutePatrolStep();
	void ExecuteMoveToRandomLocation();
	void ExecuteMoveToNextWaypoint(const TArray<AActor*>& Waypoints, bool bIsPingPong);
	void ReturnToStationaryGuardPost(const FVector& GuardLoc, const FRotator& GuardRot);
};