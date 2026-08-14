#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================================
	// ZMIENNE STAMINY
	// ==========================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float Stamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float DrainRate = 15.0f; // Zużycie staminy na sekundę przy sprincie

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float RegenRate = 10.0f; // Regeneracja staminy na sekundę

	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Stamina")
	bool bIsSprinting = false;

	// ==========================================================
	// FUNKCJE STAMINY
	// ==========================================================

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	void StopSprint();
};