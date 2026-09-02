#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AISensoryTypes.h"
#include "SensoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAIStateChanged, EAIBehaviorState, NewState, FVector, TargetLocation);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API USensoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USensoryComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================================
	// 1. PROFIL ZMYSŁÓW TEGO KONKRETNEGO POTWORA:
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sensory Profile")
	FAISensoryProfile SensoryProfile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI State")
	EAIBehaviorState CurrentState = EAIBehaviorState::Idle_Patrol;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Events")
	FOnAIStateChanged OnAIStateChanged;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|AI")
	void SetAIState(EAIBehaviorState NewState, FVector TargetLocation = FVector::ZeroVector);

protected:
	virtual void BeginPlay() override;

private:
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	float ChaseLostTime = 0.0f;
	float DetectionBuildUpTimer = 0.0f;

	int32 RecentFlashCount = 0;
	float LastFlashTimestamp = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lightkeeper|Sensory Profile")
	float ChaseMemoryDuration = 3.5f;

	float DetectionTelegraphTimer = 0.0f;
	bool bIsTelegraphingAggro = false;

	int32 RecentNoiseCount = 0;
	float LastNoiseTimestamp = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lightkeeper|Sensory Profile", meta = (ToolTip = "Liczba hałasów z bliska (np. 3), która wymusza natychmiastowe dostrojenie i szarżę na gracza."))
	int32 NoiseSaturationThreshold = 3;

	void EvaluateSensoryStimuli();
	bool CheckLineOfSightToTarget(AActor* TargetActor) const;
	bool CheckAcousticOcclusion(FVector SoundOrigin) const;
	bool IsTargetLookingAtMe(APawn* TargetPawn) const;
};