#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UtilityManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveUtilityChanged, FGameplayTag, NewUtilityTag);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UUtilityManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUtilityManagerComponent();

	// ==========================================================
	// 1. SLOTY NA PASIE I ROZWÓJ POSTACI (Metroidvania)
	// ==========================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Utility")
	int32 MaxUnlockedSlots = 1; // Zaczynamy z 1 slotem na pasie (ulepszane w Hubie!)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Utility")
	TArray<FGameplayTag> EquippedUtilityTags; // Co wisi na pasie: [0] = Item.Utility.BeltLantern

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Utility")
	int32 ActiveSlotIndex = 0; // Który przedmiot z pasa jest aktualnie wybrany pod [F]

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Utility")
	FOnActiveUtilityChanged OnActiveUtilityChanged;

	// ==========================================================
	// 2. STEROWANIE KLAWISZEM [F] I PRZEŁĄCZANIE
	// ==========================================================
	// Klawisz [F] -> Włącza / Wyłącza aktywne narzędzie (np. Latarnię)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Utility")
	void ToggleActiveUtility();

	// Przełącza na kolejne narzędzie na pasie (np. z Latarni na Monokl)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Utility")
	void CycleNextUtility();

	// Dolewa paliwa do aktywnego narzędzia z plecaka
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Utility")
	bool RefillActiveUtility();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Utility")
	FGameplayTag GetActiveUtilityTag() const;

protected:
	virtual void BeginPlay() override;
};