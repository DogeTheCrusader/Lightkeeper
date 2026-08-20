#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h" // Twój plik ze struktur¹ FInventoryItemData
#include "InventoryComponent.generated.h"

// Delegat powiadamiaj¹cy UI, ¿e zawartoœæ plecaka siê zmieni³a (np. dodano przedmiot)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

// Struktura opisuj¹ca KONKRETNY przedmiot le¿¹cy w konkretnym miejscu w plecaku
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	// Pozycja lewego górnego rogu przedmiotu w siatce (X = Kolumna, Y = Rz¹d)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory Slot")
	FIntPoint TopLeftIndex = FIntPoint(0, 0);

	// Same dane przedmiotu (Tag, ikona, rozmiar, HP)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory Slot")
	FInventoryItemData ItemData;
};


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// ==========================================================
	// PARAMETRY SIATKI (GRID)
	// ==========================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Inventory")
	int32 Columns = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Inventory")
	int32 Rows = 5;

	// G³ówna tablica przechowuj¹ca wszystko, co aktualnie mamy w plecaku
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Inventory")
	TArray<FInventorySlot> StoredItems;

	// ==========================================================
	// EVENTY I FUNKCJE
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

	// G³ówna funkcja dodaj¹ca - przeszukuje siatkê i upycha przedmiot, jeœli jest miejsce
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Inventory")
	bool TryAddItem(FInventoryItemData ItemToAdd);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Inventory")
	class ABaseInteractable* DropItem(int32 ItemIndex, FVector DropLocation, FRotator DropRotation);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Inventory")
	bool HasItemWithTag(FGameplayTag ItemTag) const;

protected:
	virtual void BeginPlay() override;

private:
	// Wewnêtrzna matematyka - sprawdza, czy dany prostok¹t na siatce jest wolny
	bool IsSpaceAvailable(FIntPoint ItemSize, int32 StartCol, int32 StartRow) const;

};