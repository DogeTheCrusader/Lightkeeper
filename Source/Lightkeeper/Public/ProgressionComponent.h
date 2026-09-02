#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProgressionTypes.h"
#include "GameplayTagContainer.h"
#include "ProgressionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressionComponent();

	// Tagi odblokowanych Perków (np. Perk.Vigor.1A, Perk.Precision.2A):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Progression")
	FGameplayTagContainer UnlockedPerks;

	// Poziomy statystyk (Tiery 0 - 3):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Progression")
	TMap<ECharacterStat, int32> StatTiers;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression")
	int32 GetStatTier(ECharacterStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression")
	bool HasPerk(FGameplayTag PerkTag) const;

	// Szybkie odblokowanie perków pod testy:
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Debug")
	void Debug_UnlockDemoPerks();

	// ==========================================================
	// 1. STATYSTYKI PASYWNE 4:4 (PULL API)
	// ==========================================================
	// --- WIGOR (4 Modyfikatory) ---
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Vigor")
	float GetMaxHealthBonus() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Vigor")
	float GetMaxStaminaBonus() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Vigor")
	float GetStaminaCostReductionMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Vigor")
	float GetPhysicalStrengthMultiplier() const;

	// --- PRECYZJA (4 Modyfikatory) ---
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Precision")
	float GetCrouchSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Precision")
	float GetGeneralMovementSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Precision")
	float GetJumpBonusMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Precision")
	float GetSafeFallSpeedBonus() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Precision")
	float GetStealthNoiseMultiplier() const;

	// ==========================================================
	// 2. BRAMKI ZDOLNOŚCI (CAPABILITY GATES)
	// ==========================================================
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Abilities")
	bool CanPerformHeavyMelee() const; // Wigor 1A (Ataki ciężkie)

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Abilities")
	bool CanLiftHeavyProps() const; // Wigor 2A (Barykady 250kg)

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Abilities")
	bool CanPickLocks() const; // Precyzja 2A (Włamywacz pod [E])

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression|Abilities")
	bool HasAcrobaticSoftLanding() const; // Precyzja 1A (Kocie lądowanie)

protected:
	virtual void BeginPlay() override;
};