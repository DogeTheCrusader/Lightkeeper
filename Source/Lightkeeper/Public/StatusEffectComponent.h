#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "StatusEffectComponent.generated.h"

// 1. STRUKTURA DANYCH DLA TABELI (DT_StatusEffects)
USTRUCT(BlueprintType)
struct FStatusEffectDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
	FGameplayTag StatusTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
	FText EffectName = FText::FromString(TEXT("Nowy Status"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Timing")
	float DefaultDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Control")
	bool bIsStun = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Control")
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Damage Over Time")
	float DamagePerTick = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Damage Over Time")
	float TickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. AI Scent")
	bool bSpawnsBloodScent = false;
};

// Struktura instancji w pamięci RAM:
USTRUCT()
struct FActiveStatusInstance
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag StatusTag;

	UPROPERTY()
	float RemainingDuration = 0.0f;

	UPROPERTY()
	bool bIsTimed = false;

	UPROPERTY()
	float TimeUntilNextTick = 0.0f;

	UPROPERTY()
	float DamagePerTick = 0.0f;

	UPROPERTY()
	float TickInterval = 1.0f;

	UPROPERTY()
	bool bIsStun = false;

	UPROPERTY()
	float SpeedMultiplier = 1.0f;

	UPROPERTY()
	bool bSpawnsBloodScent = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusEffectAdded, FGameplayTag, StatusTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusEffectRemoved, FGameplayTag, StatusTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStunStateChanged, bool, bIsStunned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBloodDropSpawned, FVector, Location);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatusEffectComponent();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Database")
	TObjectPtr<UDataTable> StatusEffectDataTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Status Effects")
	FGameplayTagContainer ActiveStatusTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Status Immunities")
	bool bImmuneToStun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Status Immunities")
	FGameplayTagContainer ImmuneStatusTags;

	// ==========================================================
	// GŁÓWNE PUBLICZNE METODY DLA CAŁEJ GRY:
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplyStatusEffectFromTable(FGameplayTag StatusTag, float CustomDuration = -1.0f, float CustomDamagePerTick = -1.0f, float CustomTickInterval = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void AddStatusEffect(FGameplayTag StatusTag, float Duration = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void RemoveStatusEffect(FGameplayTag StatusTag);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool HasStatusEffect(FGameplayTag StatusTag) const { return ActiveStatusTags.HasTag(StatusTag); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ClearAllStatusEffects();

	// ==========================================================
	// HELPERY DLA WALKI I MEDYCYNY:
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplyStun(float CustomDuration = 0.0f);

	// Helper krwawienia z opcjonalnymi parametrami:
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplyBleed(float CustomDuration = 0.0f, float CustomDamagePerTick = 0.0f, float CustomTickInterval = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplySlow(float CustomSpeedMultiplier = 0.0f, float CustomDuration = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void StopBleed();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool IsStunned() const { return bIsStunned; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool IsBleeding() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	float GetCurrentSpeedMultiplier() const { return CurrentSpeedMultiplier; }

	// Delegaty
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Status Effects")
	FOnStatusEffectAdded OnStatusEffectAdded;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Status Effects")
	FOnStatusEffectRemoved OnStatusEffectRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Status Effects")
	FOnStunStateChanged OnStunStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Status Effects")
	FOnBloodDropSpawned OnBloodDropSpawned;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Lightkeeper|Status Effects")
	bool bIsStunned = false;

	UPROPERTY(VisibleAnywhere, Category = "Lightkeeper|Status Effects")
	float CurrentSpeedMultiplier = 1.0f;

	UPROPERTY()
	TArray<FActiveStatusInstance> ActiveStatusInstances;

	FTimerHandle MasterHeartbeatTimerHandle;

	void ProcessMasterHeartbeat();
	void UpdateAggregatedStates();

	const FStatusEffectDataRow* FindStatusEffectRow(const FGameplayTag& StatusTag) const;
};