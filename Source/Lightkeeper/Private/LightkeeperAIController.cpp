#include "LightkeeperAIController.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "BaseEnemyCharacter.h"
#include "SensoryComponent.h"
#include "PatrolWaypoint.h"
#include "Kismet/GameplayStatics.h"

ALightkeeperAIController::ALightkeeperAIController()
{
	bAttachToPawn = true;
}

void ALightkeeperAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		CachedSensoryComp = InPawn->FindComponentByClass<USensoryComponent>();

		// Odpalamy patrol w następnej klatce po pełnym załadowaniu świata:
		//GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ALightkeeperAIController::StartPatrol);
	}
}

void ALightkeeperAIController::OnUnPossess()
{
	CachedSensoryComp.Reset();
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

	Super::OnUnPossess();
}

void ALightkeeperAIController::StartPatrol()
{
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);
	StopMovement();
	ExecutePatrolStep();
}

void ALightkeeperAIController::PauseAndFindNextPatrolPoint()
{
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

	float RandomWait = FMath::RandRange(MinPatrolWaitTime, MaxPatrolWaitTime);
	GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &ALightkeeperAIController::ExecutePatrolStep, RandomWait, false);
}

void ALightkeeperAIController::ExecutePatrolStep()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	// Weryfikacja stanu zmysłów bez rzutowania na konkretne ciało:
	if (CachedSensoryComp.IsValid())
	{
		EAIBehaviorState State = CachedSensoryComp->CurrentState;
		if (State != EAIBehaviorState::Idle_Patrol && State != EAIBehaviorState::Cautious)
		{
			return;
		}
	}

	// Odpytujemy konfigurację patrolu z ciała wroga (jeśli istnieje):
	if (ABaseEnemyCharacter* EnemyChar = Cast<ABaseEnemyCharacter>(ControlledPawn))
	{
		if (EnemyChar->PatrolMode == EEnemyPatrolMode::Stationary_Guard)
		{
			ReturnToStationaryGuardPost(EnemyChar->HomeGuardLocation, EnemyChar->HomeGuardRotation);
			return;
		}

		if (EnemyChar->PatrolMode == EEnemyPatrolMode::Waypoint_Route)
		{
			ExecuteMoveToNextWaypoint(EnemyChar->PatrolWaypoints, EnemyChar->bPatrolPingPong);
			return;
		}
	}

	// Domyślny losowy krok (Free Roam):
	ExecuteMoveToRandomLocation();
}

void ALightkeeperAIController::ReturnToStationaryGuardPost(const FVector& GuardLoc, const FRotator& GuardRot)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	float DistToPost = FVector::Dist(ControlledPawn->GetActorLocation(), GuardLoc);

	if (DistToPost > 100.0f)
	{
		MoveToLocation(GuardLoc, 40.0f, true, true, true);
	}
	else
	{
		StopMovement();
		ControlledPawn->SetActorRotation(GuardRot);
	}
}

void ALightkeeperAIController::ExecuteMoveToNextWaypoint(const TArray<AActor*>& Waypoints, bool bIsPingPong)
{
	int32 TotalWaypoints = Waypoints.Num();
	if (TotalWaypoints == 0)
	{
		ExecuteMoveToRandomLocation();
		return;
	}

	if (!Waypoints.IsValidIndex(CurrentWaypointIndex) || !Waypoints[CurrentWaypointIndex])
	{
		CurrentWaypointIndex = 0;
		PatrolDirection = 1;
	}

	// 1. ZAPAMIĘTUJEMY DOKŁADNIE TEN PUNKT, DO KTÓREGO TERAZ IDZIEMY:
	ActiveTargetWaypointIndex = CurrentWaypointIndex;
	AActor* TargetWaypoint = Waypoints[ActiveTargetWaypointIndex];

	// 2. WYLICZAMY NASTĘPNY INDEKS NA PRZYSZŁOŚĆ:
	if (bIsPingPong)
	{
		int32 NextIndex = CurrentWaypointIndex + PatrolDirection;

		if (NextIndex >= TotalWaypoints)
		{
			PatrolDirection = -1;
			CurrentWaypointIndex = FMath::Max(0, TotalWaypoints - 2);
		}
		else if (NextIndex < 0)
		{
			PatrolDirection = 1;
			CurrentWaypointIndex = FMath::Min(1, TotalWaypoints - 1);
		}
		else
		{
			CurrentWaypointIndex = NextIndex;
		}
	}
	else
	{
		CurrentWaypointIndex = (CurrentWaypointIndex + 1) % TotalWaypoints;
	}

	// 3. IDZIEMY DO CELU:
	if (TargetWaypoint)
	{
		MoveToLocation(TargetWaypoint->GetActorLocation(), 40.0f, true, true, true);
	}
}

void ALightkeeperAIController::SearchAreaAroundLocation(FVector CenterLocation, float SearchRadius)
{
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation SearchPoint;
		// 1. Próba znalezienia punktu w zadanym promieniu:
		if (NavSys->GetRandomReachablePointInRadius(CenterLocation, SearchRadius, SearchPoint))
		{
			MoveToLocation(SearchPoint.Location, 40.0f, true, true, true);
		}
		else
		{
			// 2. FALLBACK: Jeśli punkt był nieosiągalny -> losujemy punkt wokół własnych nóg, by nie stać w miejscu:
			if (NavSys->GetRandomReachablePointInRadius(ControlledPawn->GetActorLocation(), 400.0f, SearchPoint))
			{
				MoveToLocation(SearchPoint.Location, 40.0f, true, true, true);
			}
		}
	}
}

void ALightkeeperAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !CachedSensoryComp.IsValid()) return;

	EAIBehaviorState State = CachedSensoryComp->CurrentState;

	if (State == EAIBehaviorState::Flee_Panic)
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			FleeAwayFromThreat(PlayerPawn->GetActorLocation());
		}
		return;
	}

	// W STANIE CZUJNOŚCI (CAUTIOUS): Natychmiast szuka kolejnego kąta do sprawdzenia:
	if (State == EAIBehaviorState::Cautious)
	{
		GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, [this, ControlledPawn]()
			{
				if (CachedSensoryComp.IsValid() && CachedSensoryComp->CurrentState == EAIBehaviorState::Cautious)
				{
					SearchAreaAroundLocation(ControlledPawn->GetActorLocation(), 550.0f);
				}
			}, 0.6f, false);
		return;
	}

	if (State == EAIBehaviorState::Idle_Patrol && Result.IsSuccess())
	{
		if (ABaseEnemyCharacter* EnemyChar = Cast<ABaseEnemyCharacter>(ControlledPawn))
		{
			// 1. Tryb strażnika w miejscu:
			if (EnemyChar->PatrolMode == EEnemyPatrolMode::Stationary_Guard)
			{
				ControlledPawn->SetActorRotation(EnemyChar->HomeGuardRotation);
				return;
			}

			// 2. Tryb trasy z precyzyjnym odczytem punktu docelowego:
			if (EnemyChar->PatrolMode == EEnemyPatrolMode::Waypoint_Route && EnemyChar->PatrolWaypoints.Num() > 0)
			{
				if (EnemyChar->PatrolWaypoints.IsValidIndex(ActiveTargetWaypointIndex))
				{
					if (APatrolWaypoint* WaypointActor = Cast<APatrolWaypoint>(EnemyChar->PatrolWaypoints[ActiveTargetWaypointIndex]))
					{
						float WaitTime = WaypointActor->WaitTimeAtWaypoint;

						// PŁYNNY OBRÓT ZGODNIE ZE STRZAŁKĄ TEGO KONKRETNEGO WAYPOINTA:
						if (WaypointActor->bOverrideLookRotationOnWait)
						{
							EnemyChar->DesiredTargetYaw = WaypointActor->GetActorRotation().Yaw;
							EnemyChar->bIsInterpolatingRotation = true;
						}

						if (WaitTime > 0.0f)
						{
							GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &ALightkeeperAIController::ExecutePatrolStep, WaitTime, false);
							return;
						}
						else
						{
							ExecutePatrolStep();
							return;
						}
					}
				}
			}
		}

		PauseAndFindNextPatrolPoint();
	}
}

void ALightkeeperAIController::ExecuteMoveToRandomLocation()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation RandomLocation;
		bool bIsCautious = (CachedSensoryComp.IsValid() && CachedSensoryComp->CurrentState == EAIBehaviorState::Cautious);
		float Radius = bIsCautious ? 600.0f : PatrolRadius;

		if (NavSys->GetRandomReachablePointInRadius(ControlledPawn->GetActorLocation(), Radius, RandomLocation))
		{
			MoveToLocation(RandomLocation.Location, 40.0f, true, true, true);
		}
	}
}



void ALightkeeperAIController::FleeAwayFromThreat(FVector ThreatLocation)
{
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return;

	FVector PawnLoc = ControlledPawn->GetActorLocation();
	FVector AwayDir = (PawnLoc - ThreatLocation).GetSafeNormal();

	FVector DesiredFleePoint = PawnLoc + (AwayDir * 2000.0f);

	FNavLocation ReachableFleeLocation;
	if (NavSys->GetRandomReachablePointInRadius(DesiredFleePoint, 800.0f, ReachableFleeLocation))
	{
		MoveToLocation(ReachableFleeLocation.Location, 40.0f, true, true, true);
	}
	else
	{
		if (NavSys->GetRandomReachablePointInRadius(PawnLoc, 1500.0f, ReachableFleeLocation))
		{
			MoveToLocation(ReachableFleeLocation.Location, 40.0f, true, true, true);
		}
	}
}

void ALightkeeperAIController::ChaseTargetActor(AActor* TargetActor, float AcceptanceRadius)
{
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

	if (!TargetActor) return;

	float CurrentTime = GetWorld()->GetTimeSeconds();

	if ((CurrentTime - LastChaseRepathTime) < 0.25f && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	LastChaseRepathTime = CurrentTime;
	MoveToActor(TargetActor, AcceptanceRadius, true, true, true);
}

void ALightkeeperAIController::MoveToInvestigationPoint(FVector TargetLocation, float AcceptanceRadius)
{
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);
	MoveToLocation(TargetLocation, AcceptanceRadius, true, true, true, true);
}