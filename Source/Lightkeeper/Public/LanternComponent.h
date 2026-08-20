#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "LanternComponent.generated.h"

// Delegaty dla UI, dŸwiêków w³¹czania/wy³¹czania i efektów œwietlnych
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLanternToggled, bool, bIsLit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFuelChanged, float, CurrentFuel, float, MaxFuel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFuelDepleted);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API ULanternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULanternComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================================
	// 1. IDENTYFIKACJA NARZÊDZIA (Pod przysz³y system slotów!)
	// ==========================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Lantern")
	FGameplayTag UtilityTag; // Domyœlnie: Item.Utility.BeltLantern

	// ==========================================================
	// 2. ZASOBY I PALIWO (Nafta)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Lantern")
	float MaxFuel = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Lantern")
	float CurrentFuel = 100.0f;

	// Ile jednostek nafty zjada na sekundê (1.0 = 100 sekund ci¹g³ego œwiecenia)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Lantern")
	float FuelDrainRate = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Lantern")
	bool bIsLit = false;

	// ==========================================================
	// 3. DELEGATY (EVENTY DLA UI / DWIÊKÓW)
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Lantern")
	FOnLanternToggled OnLanternToggled;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Lantern")
	FOnFuelChanged OnFuelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Lantern")
	FOnFuelDepleted OnFuelDepleted;

	// ==========================================================
	// 4. FUNKCJE STEROWANIA
	// ==========================================================
	// W³¹cza / Wy³¹cza œwiat³o pod klawiszem [F]
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Lantern")
	void ToggleLantern();

	// Bezpoœrednie dolanie paliwa
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Lantern")
	void RefillFuel(float Amount);

	// Szuka butelki nafty w plecaku (Item.Consumable.Oil) i uzupe³nia bak
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Lantern")
	bool RefillFuelFromInventory(float AmountPerBottle = 50.0f);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Lantern")
	bool HasFuel() const { return CurrentFuel > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Lantern")
	float GetFuelPercent() const { return MaxFuel > 0.0f ? (CurrentFuel / MaxFuel) : 0.0f; }

protected:
	virtual void BeginPlay() override;
};