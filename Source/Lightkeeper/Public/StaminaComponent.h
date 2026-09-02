#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExhaustionTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSecondWindActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForcedDropDueToFatigue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSprintTickEvent, float, DeltaTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLowStaminaTickEvent, float, DeltaTime);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Stamina")
	float Stamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float JumpStaminaCost = 18.0f; // Koszt staminy za pojedynczy skok

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float DrainRate = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina")
	float BaseRegenRate = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Carrying")
	float HeavyCarryMassThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Carrying")
	float DrainPerKgOverThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Carrying")
	float MaxCarryingMassForSprint = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Breathing")
	float FatigueThresholdPercent = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Breathing", meta = (ClampMin = "0.05", ClampMax = "0.50"))
	float LowStaminaThresholdPercent = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Breathing")
	float FatigueRegenMultiplier = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Breathing")
	float RestTimeToTriggerRush = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Breathing")
	float SecondWindMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Stamina|Breathing")
	float SecondWindExitThreshold = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Stamina")
	bool bIsSprinting = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Stamina")
	bool bWantsToSprint = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Stamina")
	bool bIsFatigued = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Stamina")
	bool bSecondWindActive = false;

	// Delegaty zdarzeniowe:
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Stamina")
	FOnStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Stamina")
	FOnExhaustionTriggered OnExhaustionTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Stamina")
	FOnSecondWindActivated OnSecondWindActivated;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Stamina")
	FOnForcedDropDueToFatigue OnForcedDropDueToFatigue;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Stamina")
	FOnSprintTickEvent OnSprintTick;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Stamina")
	FOnLowStaminaTickEvent OnLowStaminaTick;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	bool TryConsumeStamina(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	void StopSprint();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	void HandleLanded();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Stamina")
	float GetCurrentStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Stamina")
	float GetEffectiveMaxStamina() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Stamina")
	float GetEncumbranceSpeedMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Stamina")
	bool TryConsumeJumpStamina();

protected:
	virtual void BeginPlay() override;

private:
	float RestTimer = 0.0f;

	float FootstepNoiseTimer = 0.0f;

	float CalculateCurrentRegenRate() const;
	float GetCarryingStaminaDrain() const;
	bool CanSprint() const;
};