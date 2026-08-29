#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InventoryTypes.h"
#include "BaseTool.generated.h"

UENUM(BlueprintType)
enum class EToolState : uint8
{
	Holstered		UMETA(DisplayName = "Schowane na plecach"),
	LowReady		UMETA(DisplayName = "Opuszczone (Tryb Eksploracji)"),
	Aiming			UMETA(DisplayName = "Celowanie (PPM Wciśnięty)"),
	Action			UMETA(DisplayName = "W Trakcie Akcji (Cooldown)")
};

UCLASS(Blueprintable)
class LIGHTKEEPER_API ABaseTool : public AActor
{
	GENERATED_BODY()

public:
	ABaseTool();

	virtual void Tick(float DeltaTime) override;

	// ====================================================================
	// 1. KOMPONENTY
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* SkeletalMeshComp;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Tool")
	class UMeshComponent* GetActiveMeshComponent() const;

	// ====================================================================
	// 2. GŁÓWNA STRUKTURA DANYCH
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Tool Data")
	FInventoryItemData ToolItemData;

	// ====================================================================
	// 3. ZMIENNE KOMPATYBILNOŚCI (Dla ToolManager i InventoryComponent)
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Tool Runtime")
	FGameplayTag ToolTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Tool Runtime")
	bool bUsesSharedFuel = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Tool Runtime")
	EItemEquipType EquipType = EItemEquipType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Tool Runtime")
	TSubclassOf<class ABaseInteractable> DropClass;

	// ====================================================================
	// 4. STAN I AMUNICJA W LOCIE
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Tool State")
	EToolState CurrentState = EToolState::LowReady;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Tool State")
	float CurrentAmmo = 0.0f;

	// ====================================================================
	// 5. METODY BRONI
	// ====================================================================
	virtual void EquipTool(AActor* NewOwner);
	virtual void UnequipTool();

	virtual void EnterAimState();
	virtual void ExitAimState();

	virtual void StartPrimaryAction();
	virtual void StopPrimaryAction();
	virtual void EndPrimaryAction();

	virtual void QuickMelee();
	virtual bool ReloadTool();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Tool")
	virtual void InitializeFromItemData(const FInventoryItemData& Data, UStaticMesh* SourceMesh);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Tool")
	bool IsAiming() const { return CurrentState == EToolState::Aiming; }

protected:
	virtual void BeginPlay() override;

	virtual void PerformLightAttack();
	virtual void PerformHeavyAttack();
	virtual void PerformThrowAction(float ChargeMultiplier = 1.0f);
	virtual void PerformUniversalAttackTrace(bool bIsHeavy);

	virtual void ApplyMeleeHit(AActor* TargetActor, float PhysicalDamage, FGameplayTag PhysicalDamageTag, FGameplayTag StateTag, float Intensity);
	virtual bool TryConsumeResources(bool bIsHeavy);

	UFUNCTION(BlueprintNativeEvent, Category = "Lightkeeper|Tool")
	void ExecuteQuickMeleeEffect();

	void TriggerAutomaticHeavyAttack();
	void ResetActionState();
	void ResetQuickMeleeCooldown();

	FTimerHandle HeavyChargeTimerHandle;
	FTimerHandle ActionCooldownTimer;
	FTimerHandle QuickMeleeTimerHandle;

	float PrimaryActionStartTime = 0.0f;
	bool bCanQuickMelee = true;
};