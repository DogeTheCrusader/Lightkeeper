#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AISensoryTypes.h"
#include "ImSimSensorySubsystem.generated.h"

UCLASS()
class LIGHTKEEPER_API UImSimSensorySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Rejestruje hałas w świecie (Kroki, zeskoki, wybuchy, rzuty):
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sensory")
	void RegisterNoise(FVector Location, float Radius, FGameplayTag NoiseTag, FGameplayTag MaterialTag = FGameplayTag(), float CustomMultiplier = 1.0f);

	static float GetMaterialNoiseMultiplier(FGameplayTag MaterialTag);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sensory")
	static float ExtractNoiseMultiplierFromActor(AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Sensory")
	static FGameplayTag ExtractMaterialTagFromActor(AActor* TargetActor);

	// Rejestruje zapach (Kropla krwi, przynęta, rozlany śluz):
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Sensory")
	void RegisterScent(FVector Location, float Radius, FGameplayTag ScentTag, float Duration = 15.0f);

	// Pobiera aktywne bodźce w danym promieniu:
	const TArray<FImSimStimulusEvent>& GetActiveStimuli() const { return ActiveStimuli; }

	// Usuwa wygasłe zapachy i hałasy:
	void CleanupExpiredStimuli();

private:
	TArray<FImSimStimulusEvent> ActiveStimuli;
	FTimerHandle CleanupTimerHandle;
};