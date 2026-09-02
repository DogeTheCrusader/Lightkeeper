#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PatrolWaypoint.generated.h"

UCLASS()
class LIGHTKEEPER_API APatrolWaypoint : public AActor
{
	GENERATED_BODY()

public:
	APatrolWaypoint();

	// Czas postoju w tym konkretnym punkcie (0.0 = brak postoju, płynny marsz dalej):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Waypoint")
	float WaitTimeAtWaypoint = 0.0f;

	// Czy potwór po dotarciu ma obrócić się w kierunku, w który skierowany jest ten Waypoint w edytorze?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Waypoint")
	bool bOverrideLookRotationOnWait = true;

#if WITH_EDITORONLY_DATA
	// Wizualizacja strzałki kierunku w edytorze (niewidoczna w grze):
	UPROPERTY()
	class UArrowComponent* DirectionArrow;
#endif
};