#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SanityComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSanityChanged, float, CurrentSanity, float, MaxSanity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoutOfMadnessTriggered, int32, MadnessTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPsychicFaint);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API USanityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USanityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float BaseMaxSanity = 100.0f;

	// Limit Sanity po Obłędach (1.0 = 100%, 0.75 = 75%, 0.50 = 50%)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float MaxSanityCapMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float CurrentSanity = 100.0f;

	// Szybkość spadku Sanity w ciemności (punkty na sekundę)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float DarknessDrainRate = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	bool bIsInDarkness = false;

	// ==========================================================
	// DELEGATY (EVENTY)
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnSanityChanged OnSanityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnBoutOfMadnessTriggered OnBoutOfMadnessTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnPsychicFaint OnPsychicFaint;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void ModifySanity(float DeltaAmount);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void SetInDarkness(bool bInDarkness) { bIsInDarkness = bInDarkness; }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void ApplySanityCap(float CapMultiplier);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sanity")
	float GetMaxSanity() const { return BaseMaxSanity * MaxSanityCapMultiplier; }

protected:
	virtual void BeginPlay() override;

private:
	int32 CurrentMadnessTier = 0; // 0 = Normal, 1 = 75% Cap, 2 = 50% Cap
	void CheckMadnessThresholds();
};