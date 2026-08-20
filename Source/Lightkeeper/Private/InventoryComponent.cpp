#include "InventoryComponent.h"
#include "BaseInteractable.h"
#include "HealthComponent.h"
#include "Engine/World.h"

// Konstruktor
UInventoryComponent::UInventoryComponent()
{
	// Wy³¹czamy Tick - optymalizacja! Ekwipunek nie musi siê aktualizowaæ co klatkê.
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// G³ówna pêtla szukaj¹ca miejsca (jak w Tetrisie)
bool UInventoryComponent::TryAddItem(FInventoryItemData ItemToAdd)
{
	// Przeszukujemy siatkê rz¹d po rzêdzie, kolumna po kolumnie (od lewej do prawej, od góry do do³u)
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Columns; ++Col)
		{
			// Czy ten konkretny "prostok¹t" przestrzeni jest wolny?
			if (IsSpaceAvailable(ItemToAdd.GridSize, Col, Row))
			{
				// ZnaleŸliœmy miejsce! Tworzymy nowy wpis do plecaka.
				FInventorySlot NewSlot;
				NewSlot.TopLeftIndex = FIntPoint(Col, Row);
				NewSlot.ItemData = ItemToAdd;

				// Dodajemy do bazy i informujemy UI
				StoredItems.Add(NewSlot);
				OnInventoryUpdated.Broadcast();

				return true; // Sukces
			}
		}
	}

	// Przeszukaliœmy ca³y plecak i nie ma miejsca
	return false;
}

// Wewnêtrzna matematyka Siatki (Sprawdzanie kolizji 2D)
bool UInventoryComponent::IsSpaceAvailable(FIntPoint ItemSize, int32 StartCol, int32 StartRow) const
{
	// 1. Sprawdzenie granic (Czy przedmiot nie wystaje poza krawêdzie plecaka?)
	if (StartCol < 0 || StartRow < 0) return false;
	if (StartCol + ItemSize.X > Columns) return false;
	if (StartRow + ItemSize.Y > Rows) return false;

	// 2. Sprawdzenie kolizji z istniej¹cymi przedmiotami
	for (const FInventorySlot& Slot : StoredItems)
	{
		// Wymiary sprawdzanego prostok¹ta (naszego nowego przedmiotu)
		int32 R1_Left = StartCol;
		int32 R1_Right = StartCol + ItemSize.X;
		int32 R1_Top = StartRow;
		int32 R1_Bottom = StartRow + ItemSize.Y;

		// Wymiary prostok¹ta przedmiotu, który ju¿ le¿y w plecaku
		int32 R2_Left = Slot.TopLeftIndex.X;
		int32 R2_Right = Slot.TopLeftIndex.X + Slot.ItemData.GridSize.X;
		int32 R2_Top = Slot.TopLeftIndex.Y;
		int32 R2_Bottom = Slot.TopLeftIndex.Y + Slot.ItemData.GridSize.Y;

		// Test AABB (Axis-Aligned Bounding Box) dla 2D. 
		// Sprawdza, czy te dwa prostok¹ty siê nak³adaj¹.
		if (R1_Left < R2_Right && R1_Right > R2_Left &&
			R1_Top < R2_Bottom && R1_Bottom > R2_Top)
		{
			// Znaleziono kolizjê - to miejsce jest zajête!
			return false;
		}
	}

	// Prostok¹t nie wyszed³ poza granice i na nic nie wpad³ - miejsce jest wolne!
	return true;
}

ABaseInteractable* UInventoryComponent::DropItem(int32 ItemIndex, FVector DropLocation, FRotator DropRotation)
{
	// 1. Sprawdzamy czy taki slot istnieje w plecaku:
	if (!StoredItems.IsValidIndex(ItemIndex))
	{
		return nullptr;
	}

	FInventorySlot SlotToDrop = StoredItems[ItemIndex];

	// 2. Sprawdzamy czy przedmiot wie, jaki Blueprint ma zrespawnowaæ:
	if (!SlotToDrop.ItemData.DropClass)
	{
		return nullptr;
	}

	// 3. Usuwamy przedmiot z pamiêci plecaka:
	StoredItems.RemoveAt(ItemIndex);
	OnInventoryUpdated.Broadcast(); // Sygna³ do odœwie¿enia UI

	// 4. Spawnujemy fizyczny model 3D w œwiecie:
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABaseInteractable* SpawnedProp = GetWorld()->SpawnActor<ABaseInteractable>(
		SlotToDrop.ItemData.DropClass,
		DropLocation,
		DropRotation,
		SpawnParams
	);

	// 5. Przywracamy mu zapisane punkty ¿ycia (HP):
	if (SpawnedProp && SpawnedProp->HealthComp)
	{
		SpawnedProp->HealthComp->CurrentHealth = SlotToDrop.ItemData.SavedHealth;
	}

	return SpawnedProp;
}

bool UInventoryComponent::HasItemWithTag(FGameplayTag ItemTag) const
{
	if (!ItemTag.IsValid()) return false;

	for (const FInventorySlot& Slot : StoredItems)
	{
		if (Slot.ItemData.ItemTag.MatchesTag(ItemTag))
		{
			return true; // Znaleziono pasuj¹cy przedmiot!
		}
	}

	return false;
}