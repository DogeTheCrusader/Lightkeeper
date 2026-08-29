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

// 2. DELEGATY
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

	// ==========================================================
	// 1. DATA TABLE STATUSÓW
	// ==========================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Database")
	TObjectPtr<UDataTable> StatusEffectDataTable;

	// ==========================================================
	// 2. DANE STANU I IMMUNITETY
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Status Effects")
	FGameplayTagContainer ActiveStatusTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Status Immunities")
	bool bImmuneToStun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Status Immunities")
	FGameplayTagContainer ImmuneStatusTags;

	// ==========================================================
	// 3. METODY GŁÓWNE
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplyStatusEffectFromTable(FGameplayTag StatusTag, float CustomDuration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void AddStatusEffect(FGameplayTag StatusTag, float Duration = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void RemoveStatusEffect(FGameplayTag StatusTag);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool HasStatusEffect(FGameplayTag StatusTag) const { return ActiveStatusTags.HasTag(StatusTag); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ClearAllStatusEffects();

	// ==========================================================
	// 4. KONTROLA CIAŁA (STUN, BLEED, SLOW)
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplyStun(float Duration);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplySlow(float SpeedMultiplier, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ApplyBleed(float Duration = 15.0f, float DamagePerTick = 1.0f, float TickInterval = 3.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void StopBleed();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool IsStunned() const { return bIsStunned; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool IsBleeding() const { return bIsBleeding; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	float GetCurrentSpeedMultiplier() const { return CurrentSpeedMultiplier; }

	// ==========================================================
	// 5. DELEGATY ASSIGNABLE
	// ==========================================================
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
	bool bIsBleeding = false;

	UPROPERTY(VisibleAnywhere, Category = "Lightkeeper|Status Effects")
	float CurrentSpeedMultiplier = 1.0f;

	FTimerHandle StunTimerHandle;
	FTimerHandle SlowTimerHandle;
	FTimerHandle BleedTimerHandle;
	FTimerHandle BleedDurationTimerHandle;

	float BleedDamageTick = 1.0f;

	void EndStun();
	void EndSlow();
	void ProcessBleedTick();

	const FStatusEffectDataRow* FindStatusEffectRow(const FGameplayTag& StatusTag) const;
};