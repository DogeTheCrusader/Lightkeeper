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
	void EvaluateSensoryStimuli();
	bool CheckLineOfSightToPlayer(class ALightkeeperCharacter* Player) const;
	bool CheckAcousticOcclusion(FVector SoundOrigin) const;
};