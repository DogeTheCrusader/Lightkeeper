#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeightPlateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeightThresholdChanged, bool, bThresholdReached);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeightUpdated, float, CurrentWeightKg, float, WeightRatio);

class UPrimitiveComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UWeightPlateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeightPlateComponent();

	// Wymagana masa w kg do aktywacji mechanizmu:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Weight Plate")
	float TargetWeightThresholdKg = 150.0f;

	// Waga gracza w kg:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Weight Plate")
	float PlayerMassKg = 80.0f;

	// Aktualna masa obiektów w czasie rzeczywistym:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Weight Plate")
	float CurrentWeightKg = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Weight Plate")
	bool bIsThresholdReached = false;

	// Opcjonalny wskaźnik na konkretną strefę kolizji (jeśli puste, bierze główną kolizję mebla):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Weight Plate")
	TObjectPtr<UPrimitiveComponent> CustomDetectionVolume;

	// Delegaty zdarzeń:
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Weight Plate")
	FOnWeightThresholdChanged OnWeightThresholdChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Weight Plate")
	FOnWeightUpdated OnWeightUpdated;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	void RecalculateTotalWeight();
	UPrimitiveComponent* GetActivePrimitive() const;
};