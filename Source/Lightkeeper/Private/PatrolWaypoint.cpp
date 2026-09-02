#include "PatrolWaypoint.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

APatrolWaypoint::APatrolWaypoint()
{
	PrimaryActorTick.bCanEverTick = false; // Zero zużycia CPU!

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	if (DirectionArrow)
	{
		DirectionArrow->SetupAttachment(RootComponent);
		DirectionArrow->ArrowColor = FColor::Yellow;
		DirectionArrow->ArrowSize = 1.2f;
	}
#endif
}