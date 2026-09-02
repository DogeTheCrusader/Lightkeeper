#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "ReactionReceiverComponent.generated.h"

// ====================================================================
// STRUKTURA REGUŁ CHEMICZNYCH
// ====================================================================
USTRUCT(BlueprintType)
struct FChemicalReactionRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag IncomingElement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag RequiredActiveState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag StateToRemove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag StateToAdd;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag RequiredMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	bool bTriggerEmission = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateApplied, FGameplayTag, StateTag, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateRemoved, FGameplayTag, StateTag);

class UHealthComponent;
class UStatusEffectComponent;
struct FStatusEffectDataRow;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UReactionReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReactionReceiverComponent();

	// ==========================================================
	// BAZA DANYCH STATUSÓW (Jedno Źródło Prawdy dla Całej Gry)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Database")
	UDataTable* StatusEffectsDataTable;

	// Tagi stanów, na które ten obiekt jest wrażliwy:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim States")
	FGameplayTagContainer VulnerableStates;

	// Tagi stanów, które OBECNIE działają na ten obiekt:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|ImSim States")
	FGameplayTagContainer ActiveStates;

	// Czas samoczynnego wygaszenia ognia dla niezniszczalnych ścian (np. 8s):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim States|Timing")
	float AutoExtinguishDuration = 8.0f;

	// ==========================================================
	// DELEGATY
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|ImSim States")
	FOnStateApplied OnStateApplied;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|ImSim States")
	FOnStateRemoved OnStateRemoved;

	// ==========================================================
	// FUNKCJE
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim States")
	void ApplyStateImpact(FGameplayTag IncomingState, float Intensity = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim States")
	void RemoveState(FGameplayTag StateTag);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|ImSim States")
	bool HasState(FGameplayTag StateTag) const { return ActiveStates.HasTag(StateTag); }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|ImSim States")
	bool IsVulnerableTo(const FGameplayTag& StateTag) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle DoTTimerHandle;

	// UNIWERSALNA MAPA CZASU TRWANIA STANÓW:
	TMap<FGameplayTag, float> StateExposureTimers;

	TArray<FChemicalReactionRule> ReactionRules;

	// ZBUFOROWANE WSKAŹNIKI PAMIĘCI:
	UPROPERTY()
	TObjectPtr<UHealthComponent> CachedHealthComp;

	UPROPERTY()
	TObjectPtr<UStatusEffectComponent> CachedStatusComp;

	void InitializeReactionRules();
	bool EvaluateReactionMatrix(const FGameplayTag& IncomingState, float Intensity, const FGameplayTag& OwnerMaterial);
	void ProcessDoTTick();
	void CheckAndManageDoTTimer();

	const FStatusEffectDataRow* FindStatusEffectRow(const FGameplayTag& StatusTag) const;
};