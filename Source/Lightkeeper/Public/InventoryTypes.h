#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InventoryTypes.generated.h" // Zmieñ na nazwê swojego pliku

// Struktura opisuj¹ca czym jest przedmiot w ekwipunku
USTRUCT(BlueprintType)
struct FInventoryItemData
{
	GENERATED_BODY()

	// 1. TO¯SAMOŒÆ PRZEDMIOTU
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	FText ItemName = FText::FromString(TEXT("Nowy Przedmiot"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (MultiLine = true))
	FText ItemDescription = FText::FromString(TEXT("Opis przedmiotu..."));

	// 2. INTERFEJS I SIATKA (RESIDENT EVIL GRID)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	class UTexture2D* ItemIcon = nullptr;

	// 3. FIZYCZNOŒÆ W ŒWIECIE 3D
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	TSubclassOf<class ABaseInteractable> DropClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Item Data")
	float SavedHealth = 100.0f;

	// 4. UNIWERSALNA WARTOŒÆ U¯YTKOWA (Paliwo, Leczenie HP, Amunicja, Z³oto itp.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	float PrimaryValue = 0.0f;
};