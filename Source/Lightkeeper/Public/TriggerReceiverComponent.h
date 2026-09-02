#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TriggerReceiverComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTriggerReceived, bool, bIsActivated);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UTriggerReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTriggerReceiverComponent();

	// ==========================================================
	// AKUSTYKA DLA AI (Konfigurowalna w Details dla każdej maszyny!)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Acoustics")
	bool bEmitsActivationNoise = true;

	// Zasięg hałasu przy włączeniu maszyny (np. 600cm dla windy, 2500cm dla syreny!):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Acoustics", meta = (EditCondition = "bEmitsActivationNoise"))
	float ActivationNoiseRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Acoustics", meta = (EditCondition = "bEmitsActivationNoise"))
	FGameplayTag MaterialTag;

	// Delegat wywoływany przy odebraniu sygnału (dla Blueprinta do animacji/FMOD):
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Trigger")
	FOnTriggerReceived OnTriggerReceived;

	// Główna funkcja wywoływana przez wajchę/zawór/przycisk:
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Trigger")
	void ReceiveTriggerSignal(bool bNewActivated);
};