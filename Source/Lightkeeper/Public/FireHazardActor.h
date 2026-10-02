#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FireHazardActor.generated.h"

class UPointLightComponent;
class USafeLightComponent;

UCLASS()
class LIGHTKEEPER_API AFireHazardActor : public AActor
{
	GENERATED_BODY()

public:
	AFireHazardActor();

	virtual void Tick(float DeltaTime) override;

	void InitializeHazard(AActor* InTargetActor, float Radius, float Intensity, float Duration, bool bIsVerticalObject = false);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Hazard")
	TObjectPtr<USceneComponent> RootScene;

	// Główne światło (nad skrzynią, rzuca miękkie cienie na świat):
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Hazard")
	TObjectPtr<UPointLightComponent> FireLight;

	// Światło wypełniające (w środku skrzynki, rozświetla boki drewna, BEZ CIENI):
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Hazard")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Hazard")
	TObjectPtr<USafeLightComponent> SafeLight;

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> TargetPhysMesh;

	bool bFollowPhysicsTop = false;

	bool bIsVerticalSurface = false;
};