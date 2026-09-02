#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SanityComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSanityChanged, float, CurrentSanity, float, MaxSanity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMinorMadnessTriggered, FGameplayTag, MadnessTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoutOfMadnessTriggered, FGameplayTag, BoutTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPsychologicalCollapse, int32, CollapseCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTotalMentalBreakdown);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API USanityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USanityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================================
	// 1. STATYSTYKI POCZYTALNOŚCI I REGENERACJA
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float BaseMaxSanity = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float CurrentSanity = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float MaxSanityCapMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float BaseLightRecoveryRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float AdrenalineRecoveryRate = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float AdrenalineDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float PassiveRecoveryWindowPercent = 0.30f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float LowestSanityPercentInDarkness = 1.0f;

	// ==========================================================
	// 2. MROK, SANKTUARIA I SPOJRZENIE NA POTWORY
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	int32 ActiveLightSourcesCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	int32 SanctuaryZonesCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	bool bIsInDarkness = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float TimeInDarkness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float BaseDarknessDrainRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float DarknessAccelerationFactor = 0.08f;

	// Spojrzenie na potwora (Gaze Dread):
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	bool bIsLookingAtMonster = false;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void RegisterPotentialLight(class USafeLightComponent* LightComp);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void UnregisterPotentialLight(class USafeLightComponent* LightComp);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sanity")
	const TArray<class USafeLightComponent*>& GetOverlappingLightSources() const { return OverlappingLightSources; }

	// ==========================================================
	// 3. SZALEŃSTWO I FAIL FORWARD
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float GracePeriodDuration = 10.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	bool bHasMinorMadness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float MinorMadnessDrainMultiplier = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	int32 MentalCollapseCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	FGameplayTagContainer ActiveMadnessTags;

	// Delegaty:
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnSanityChanged OnSanityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnMinorMadnessTriggered OnMinorMadnessTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnBoutOfMadnessTriggered OnBoutOfMadnessTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnPsychologicalCollapse OnPsychologicalCollapse;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Sanity")
	FOnTotalMentalBreakdown OnTotalMentalBreakdown;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void TakeSanityDamage(float DamageAmount, FGameplayTag ShockTag = FGameplayTag());

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void RestoreSanity(float RestoreAmount);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void AddLightSource();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void RemoveLightSource();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void EnterSanctuary() { SanctuaryZonesCount++; }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void LeaveSanctuary() { SanctuaryZonesCount = FMath::Max(0, SanctuaryZonesCount - 1); }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sanity")
	bool IsInSanctuary() const { return SanctuaryZonesCount > 0; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sanity")
	float GetMaxSanity() const { return BaseMaxSanity * MaxSanityCapMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sanity")
	float GetCurrentDynamicComfortCap() const;

protected:
	virtual void BeginPlay() override;

	bool bIsGracePeriodActive = false;
	bool bIsAdrenalineActive = false;

	FTimerHandle GracePeriodTimerHandle;
	FTimerHandle AdrenalineTimerHandle;

	virtual void EndGracePeriod();
	virtual void EndAdrenalineSurge();

	UPROPERTY()
	TArray<class USafeLightComponent*> OverlappingLightSources;

	bool CheckLightLineOfSight();
	void EvaluateMonsterGazeDread(float DeltaTime);

private:
	void HandleSanityDepleted();
	void TriggerRandomMinorMadness();
	void TriggerMajorBoutOfMadness();
};