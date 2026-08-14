#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ReactionReceiverComponent.generated.h"

// Event powiadamiający Blueprint obiektu, że dany Stan na niego wpłynął (np. Ogień zaczął go palić)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateApplied, FGameplayTag, StateTag, float, Intensity);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UReactionReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReactionReceiverComponent();

	// Tagi stanów, na które ten obiekt JEST WRAŻLIWY (np. Drewno -> State.Thermal.Fire)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim States")
	FGameplayTagContainer VulnerableStates;

	// Tagi stanów, które OBECNIE działają na ten obiekt
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|ImSim States")
	FGameplayTagContainer ActiveStates;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|ImSim States")
	FOnStateApplied OnStateApplied;

	// Wywoływane gdy Ogień / Woda / Para dotknie tego obiektu
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim States")
	void ApplyStateImpact(FGameplayTag StateTag, float Intensity = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim States")
	void RemoveState(FGameplayTag StateTag);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|ImSim States")
	bool HasState(FGameplayTag StateTag) const { return ActiveStates.HasTag(StateTag); }
};