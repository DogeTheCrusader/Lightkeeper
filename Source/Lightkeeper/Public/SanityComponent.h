#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SanityComponent.generated.h"

// Delegaty dla UI, dźwięków, drżenia kamery i efektów szaleństwa
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSanityChanged, float, CurrentSanity, float, MaxSanity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMinorMadnessTriggered, FGameplayTag, MadnessTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoutOfMadnessTriggered, FGameplayTag, BoutTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPsychologicalCollapse, int32, CollapseCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTotalMentalBreakdown); // 3. zapaść - Koniec Nocy!

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

	// Mnożnik limitu Sanity po zapaściach (1.0 -> 0.75 -> 0.50)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float MaxSanityCapMultiplier = 1.0f;

	// Spokojna prędkość regeneracji w świetle podczas normalnej gry (4.0 pkt/s):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float BaseLightRecoveryRate = 3.0f;

	// Błyskawiczny zryw adrenaliny po wejściu w światło po zapaści (14.0 pkt/s):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float AdrenalineRecoveryRate = 14.0f;

	// Czas trwania zrywu adrenaliny (6 sekund):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float AdrenalineDuration = 6.0f;

	// Dynamiczny sufit – ile % ponad najgłębszą panikę leczy światło (+30%):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float PassiveRecoveryWindowPercent = 0.30f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float LowestSanityPercentInDarkness = 1.0f;

	// ==========================================================
	// 2. MROK I SANKTUARIA (Wykrywanie Światła)
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	int32 ActiveLightSourcesCount = 0; // 0 = Mrok, >0 = Bezpieczne światło

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	int32 SanctuaryZonesCount = 0; // >0 = Bezpieczny Pokój (Safe Room)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	bool bIsInDarkness = true; // Startujemy w mroku

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	float TimeInDarkness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float BaseDarknessDrainRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float DarknessAccelerationFactor = 0.08f;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void RegisterPotentialLight(class USafeLightComponent* LightComp);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void UnregisterPotentialLight(class USafeLightComponent* LightComp);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sanity")
	const TArray<class USafeLightComponent*>& GetOverlappingLightSources() const { return OverlappingLightSources; }

	// ==========================================================
	// 3. SZALEŃSTWO, GRACE PERIOD I FAIL FORWARD
	// ==========================================================
	// Pancerz ochronny po zapaści (10 sekund absolutnego immunitetu na kolejny Bout w mroku!):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float GracePeriodDuration = 10.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	bool bHasMinorMadness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Sanity")
	float MinorMadnessDrainMultiplier = 1.5f;

	// Licznik psychicznych zapaści w danej nocy (1, 2, 3 -> Koniec Nocy)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	int32 MentalCollapseCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Sanity")
	FGameplayTagContainer ActiveMadnessTags;

	// ==========================================================
	// 4. EVENTY I FUNKCJE OBSŁUGI
	// ==========================================================
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

	// Szok psychiczny (spojrzenie na potwora)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void TakeSanityDamage(float DamageAmount, FGameplayTag ShockTag);

	// Leki (Wino Mariani / Sole) - leczą ponad limit i resetują traumę
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void RestoreSanity(float RestoreAmount);

	// Źródła światła (Latarnia na pasie, latarnie miejskie):
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void AddLightSource();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sanity")
	void RemoveLightSource();

	// Bezpieczne pokoje (Sanctuary):
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

	// Główna funkcja weryfikująca czy światło jest zasłonięte
	bool CheckLightLineOfSight();

private:
	void HandleSanityDepleted();
	void TriggerRandomMinorMadness();
	void TriggerMajorBoutOfMadness();
};