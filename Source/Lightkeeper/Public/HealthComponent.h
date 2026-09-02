#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "HealthComponent.generated.h"

UENUM(BlueprintType)
enum class EPlayerAction : uint8
{
	Sprint,
	Jump,
	QuickMelee,
	HoldBreath,
	Aim,
	Interact
};

UENUM(BlueprintType)
enum class EAnatomicalLimb : uint8
{
	None,
	Legs,
	LeftArm,
	RightArm,
	Chest,
	Head
};

// ====================================================================
// STRUKTURA DANYCH URAZU DLA DATA TABLE (Excel / UE5 Asset)
// ====================================================================
USTRUCT(BlueprintType)
struct FInjuryDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
	FGameplayTag InjuryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
	EAnatomicalLimb AffectedLimb = EAnatomicalLimb::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
	FText InjuryName = FText::FromString(TEXT("Nowy Uraz"));

	// --- RUCH I SPRINT ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Movement", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float WalkSpeedMultiplier = 1.0f; // np. 0.85 dla skręcenia kostki

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Movement")
	float SprintSpeedCap = 0.0f; // np. 420.0 dla złamanej nogi (0 = brak limitu)

	// --- STAMINA I PŁUCA ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Stamina")
	float MaxStaminaMultiplier = 1.0f; // np. 0.50 dla pękniętych żeber

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Stamina")
	float StaminaDrainMultiplier = 1.0f; // np. 1.20 dla skręconej kostki

	// --- WALKA I RĘCE ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Combat & Hands")
	float MeleeDamageMultiplier = 1.0f; // np. 0.30 dla złamanej prawej ręki

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Combat & Hands")
	float ThrowPowerMultiplier = 1.0f; // np. 0.50 dla złamanej ręki przy rzucie skrzynią

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Combat & Hands", meta = (ToolTip = "Dodatkowy opór myszki przy kręceniu zaworami i wajchami"))
	float MouseResistanceMultiplier = 1.0f; // np. 1.5 dla zwichniętego barku (wajcha stawia większy opór!)

	// --- BÓL I KOSZT HP (HIGH FRICTION) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Pain Cost")
	float SprintPainPerSecond = 0.0f; // np. 2.0 HP/s dla złamanej nogi

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Pain Cost")
	float LandingPainDamage = 0.0f; // np. 5.0 HP przy zeskoku na złamaną kość

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Pain Cost")
	float ActionPainCost = 0.0f; // np. 5.0 HP przy zamachu złamaną ręką

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Pain Cost")
	float ChargedThrowPainCost = 0.0f; // Ból przy ładowaniu rzutu (Hold PPM)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Combat & Hands")
	float GuardAbsorptionMultiplier = 1.0f;

	// --- BLOKADY ZDOLNOŚCI ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Restrictions")
	bool bBlocksQuickMelee = false; // np. true dla zwichniętego barku

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Restrictions")
	bool bBlocksHoldBreath = false; // np. true dla odmy płucnej
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInjuryAdded, FGameplayTag, InjuryTag, EAnatomicalLimb, Limb);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInjuryRemoved, FGameplayTag, InjuryTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPalliativeCrash);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	// ==========================================================
	// 1. DATA TABLE (MISTRZOWSKA BAZA URAZÓW)
	// ==========================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Database")
	TObjectPtr<UDataTable> InjuryDataTable;

	// ==========================================================
	// 2. ZDROWIE I PANCERZ
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health")
	bool bCanBeDestroyed = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health", meta = (EditCondition = "bCanBeDestroyed"))
	float BaseMaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health", meta = (EditCondition = "bCanBeDestroyed"))
	float DamageThreshold = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health", meta = (EditCondition = "bCanBeDestroyed"))
	float DamageSusceptibility = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health", meta = (EditCondition = "bCanBeDestroyed"))
	float CustomDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health", meta = (EditCondition = "bCanBeDestroyed"))
	FGameplayTagContainer AllowedDamageTypes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Health")
	float MaxHealthCapMultiplier = 1.0f;

	// ==========================================================
	// 3. PROFIL OBRONNY (ŚWIĘTA TRÓJCA)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Defense Profile", meta = (EditCondition = "bCanBeDestroyed"))
	FGameplayTagContainer Vulnerabilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Defense Profile", meta = (EditCondition = "bCanBeDestroyed"))
	float VulnerabilityMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Defense Profile", meta = (EditCondition = "bCanBeDestroyed"))
	FGameplayTagContainer Resistances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Defense Profile", meta = (EditCondition = "bCanBeDestroyed"))
	float ResistanceMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Defense Profile", meta = (EditCondition = "bCanBeDestroyed"))
	FGameplayTagContainer Immunities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Defense Profile", meta = (EditCondition = "bCanBeDestroyed"))
	bool bVulnerabilitiesBypassThreshold = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Combat")
	bool bIsGuarding = false;

	// ==========================================================
	// 4. ANATOMIA, PASEK PĘKNIĘCIA I LAUDANUM
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Injuries")
	bool bCanReceiveInjuries = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Injuries", meta = (EditCondition = "bCanReceiveInjuries"))
	FGameplayTagContainer ActiveInjuries;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Injuries", meta = (EditCondition = "bCanReceiveInjuries"))
	float FractureMeter = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Injuries", meta = (EditCondition = "bCanReceiveInjuries"))
	float FractureCap = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Injuries", meta = (EditCondition = "bCanReceiveInjuries"))
	float FractureIncreasePerHit = 0.05f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Injuries", meta = (EditCondition = "bCanReceiveInjuries"))
	float NightFatigueFloor = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Injuries")
	bool bIsPalliativeActive = false;

	// ==========================================================
	// 5. DELEGATY
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Injuries")
	FOnInjuryAdded OnInjuryAdded;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Injuries")
	FOnInjuryRemoved OnInjuryRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Injuries")
	FOnPalliativeCrash OnPalliativeCrash;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Health")
	FOnDeath OnDeath;

	// ==========================================================
	// 6. UNIWERSALNE GETTERY AGREGUJĄCE Z DATA TABLE (PULL API)
	// ==========================================================
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	bool IsActionAllowed(EPlayerAction Action) const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetMovementSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetSprintSpeedCap() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetMeleeDamageMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetMaxStaminaMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetStaminaDrainMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetThrowPowerMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetMouseResistanceMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetActionPainCost(EAnatomicalLimb Limb) const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetChargedThrowPainCost() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	float GetGuardAbsorptionMultiplier() const;

	// ==========================================================
	// 7. GŁÓWNA LOGIKA I MEDYCYNA
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Health")
	void TakeDamage(float DamageAmount, FGameplayTag DamageTypeTag, const FHitResult& HitInfo = FHitResult());

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Health")
	void Heal(float HealAmount);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Health")
	void ApplyFaintCap(float CapMultiplier);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Injuries")
	void AddInjury(FGameplayTag InjuryTag, EAnatomicalLimb Limb = EAnatomicalLimb::None);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Injuries")
	void RemoveInjury(FGameplayTag InjuryTag);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Injuries")
	bool HasActiveInjury(FGameplayTag InjuryTag) const;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Medical")
	void UseBandage(float HealAmount = 35.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Medical")
	void UseLaudanum(float Duration = 120.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Medical")
	void UseLeeches();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Medical")
	void ClearAllMinorInjuries();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Health")
	bool IsDead() const { return CurrentHealth <= 0.0f; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleOwnerLanded(const FHitResult& Hit, float FallSpeed);

	UFUNCTION()
	void HandleOwnerSprintTick(float DeltaTime);

	UFUNCTION()
	void HandleOwnerLowStaminaTick(float DeltaTime);

	UPROPERTY()
	TObjectPtr<class UProgressionComponent> CachedProgComp;

private:
	FTimerHandle PalliativeTimerHandle;
	float BrokenLegRunTimer = 0.0f;
	float LowStaminaChestTimer = 0.0f;

	TMap<EAnatomicalLimb, int32> LimbTraumaHistory;

	float CachedWalkSpeedMultiplier = 1.0f;
	float CachedSprintSpeedCap = 0.0f;
	float CachedMeleeDamageMultiplier = 1.0f;
	float CachedMaxStaminaMultiplier = 1.0f;
	float CachedStaminaDrainMultiplier = 1.0f;
	float CachedThrowPowerMultiplier = 1.0f;
	float CachedMouseResistanceMultiplier = 1.0f;
	float CachedChargedThrowPain = 0.0f;
	float CachedGuardAbsorptionMultiplier = 1.0f;

	void RebuildCachedModifiers();

	void ProcessDamageAndWounds(float DamageAmount, FGameplayTag DamageTypeTag, const FHitResult& HitInfo);
	void TestFractureInjury(FGameplayTag DamageTypeTag, EAnatomicalLimb HitLimb, float DamageAmount);
	void EndPalliativeEffect();

	// Zoptymalizowany odczyt O(1) z Data Table:
	const FInjuryDataRow* FindInjuryDataRow(const FGameplayTag& InjuryTag) const;
};