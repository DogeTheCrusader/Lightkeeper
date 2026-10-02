#include "InventoryComponent.h"
#include "BaseInteractable.h"
#include "ToolManagerComponent.h"
#include "Engine/World.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UInventoryComponent::TryAddItem(FInventoryItemData ItemToAdd)
{
	// ====================================================================
	// 1. PRÓBA DOŁOŻENIA DO ISTNIEJĄCEGO STOSU (Stacking):
	// ====================================================================
	if (ItemToAdd.MaxStackSize > 1)
	{
		for (FInventorySlot& ExistingSlot : StoredItems)
		{
			// Jeśli to ten sam przedmiot i stos nie jest pełny:
			if (ExistingSlot.ItemData.ItemTag.MatchesTagExact(ItemToAdd.ItemTag) &&
				ExistingSlot.ItemData.CurrentStack < ExistingSlot.ItemData.MaxStackSize)
			{
				int32 SpaceInStack = ExistingSlot.ItemData.MaxStackSize - ExistingSlot.ItemData.CurrentStack;
				int32 AmountToAdd = FMath::Min(ItemToAdd.CurrentStack, SpaceInStack);

				ExistingSlot.ItemData.CurrentStack += AmountToAdd;
				ItemToAdd.CurrentStack -= AmountToAdd;

				OnInventoryUpdated.Broadcast();

				// Jeśli cały zebrany stos zmieścił się w starym slocie -> SUKCES!
				if (ItemToAdd.CurrentStack <= 0)
				{
					return true;
				}
			}
		}
	}

	// ====================================================================
	// 2. JEŚLI ZOSTAŁY RESZTKI LUB PRZEDMIOT NIESTAKOWALNY -> SZUKAMY MIEJSCA NA SIATCE:
	// ====================================================================
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Columns; ++Col)
		{
			if (IsSpaceAvailable(ItemToAdd.GridSize, Col, Row))
			{
				FInventorySlot NewSlot;
				NewSlot.TopLeftIndex = FIntPoint(Col, Row);
				NewSlot.ItemData = ItemToAdd;

				StoredItems.Add(NewSlot);
				OnInventoryUpdated.Broadcast();

				return true;
			}
		}
	}

	return false; // Brak miejsca w plecaku!
}

bool UInventoryComponent::IsSpaceAvailable(FIntPoint ItemSize, int32 StartCol, int32 StartRow) const
{
	if (StartCol < 0 || StartRow < 0) return false;
	if (StartCol + ItemSize.X > Columns) return false;
	if (StartRow + ItemSize.Y > Rows) return false;

	for (const FInventorySlot& Slot : StoredItems)
	{
		int32 R1_Left = StartCol;
		int32 R1_Right = StartCol + ItemSize.X;
		int32 R1_Top = StartRow;
		int32 R1_Bottom = StartRow + ItemSize.Y;

		int32 R2_Left = Slot.TopLeftIndex.X;
		int32 R2_Right = Slot.TopLeftIndex.X + Slot.ItemData.GridSize.X;
		int32 R2_Top = Slot.TopLeftIndex.Y;
		int32 R2_Bottom = Slot.TopLeftIndex.Y + Slot.ItemData.GridSize.Y;

		if (R1_Left < R2_Right && R1_Right > R2_Left &&
			R1_Top < R2_Bottom && R1_Bottom > R2_Top)
		{
			return false; // Kolizja z innym przedmiotem w siatce
		}
	}

	return true;
}

ABaseInteractable* UInventoryComponent::DropItem(int32 ItemIndex, FVector DropLocation, FRotator DropRotation)
{
	if (!StoredItems.IsValidIndex(ItemIndex))
	{
		return nullptr;
	}

	FInventorySlot SlotToDrop = StoredItems[ItemIndex];

	if (!SlotToDrop.ItemData.DropClass)
	{
		return nullptr;
	}

	// 1. Usuwamy przedmiot z pamięci plecaka:
	StoredItems.RemoveAt(ItemIndex);
	OnInventoryUpdated.Broadcast();

	// 2. Chowamy broń z rąk TYLKO JEŚLI wyrzucana broń to DOKŁADNIE TA, którą trzymamy w dłoni:
	if (AActor* Owner = GetOwner())
	{
		if (UToolManagerComponent* ToolMgr = Owner->FindComponentByClass<UToolManagerComponent>())
		{
			if (ToolMgr->CurrentEquippedTool)
			{
				if (ToolMgr->CurrentEquippedTool->ToolTag.MatchesTag(SlotToDrop.ItemData.ItemTag))
				{
					ToolMgr->HolsterCurrentTool();
				}
			}
		}
	}

	// 3. Spawnujemy fizyczny obiekt w świecie 3D:
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABaseInteractable* SpawnedProp = GetWorld()->SpawnActor<ABaseInteractable>(
		SlotToDrop.ItemData.DropClass,
		DropLocation,
		DropRotation,
		SpawnParams
	);

	if (SpawnedProp)
	{
		SpawnedProp->ApplyItemData(SlotToDrop.ItemData);

		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(SpawnedProp->GetRootComponent()))
		{
			// PRZEDMIOT STARTUJE ZE SPOKOJNEJ POZYCJI BEZ FAŁSZYWYCH IMPULSÓW:
			Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	return SpawnedProp;
}

bool UInventoryComponent::ConsumeItemByTag(FGameplayTag ItemTag, int32 AmountToConsume)
{
	if (!ItemTag.IsValid() || AmountToConsume <= 0) return false;

	// Przeszukujemy plecak od końca:
	for (int32 i = StoredItems.Num() - 1; i >= 0; --i)
	{
		FInventorySlot& Slot = StoredItems[i];

		if (Slot.ItemData.ItemTag.MatchesTag(ItemTag))
		{
			// 1. Jeśli w stosie jest więcej sztuk niż zużywamy:
			if (Slot.ItemData.CurrentStack > AmountToConsume)
			{
				Slot.ItemData.CurrentStack -= AmountToConsume;
				OnInventoryUpdated.Broadcast(); // Odświeżamy UI (np. cyferkę na ikonie)
				return true;
			}
			// 2. Jeśli zużyliśmy ostatnią sztukę ze stosu (lub dokładnie tyle, ile było):
			else
			{
				int32 RemainingToConsume = AmountToConsume - Slot.ItemData.CurrentStack;
				StoredItems.RemoveAt(i);
				OnInventoryUpdated.Broadcast(); // Usuwamy pusty slot z siatki!

				// Jeśli gracz chciał zużyć więcej niż było w tym jednym slocie, szukamy w kolejnym:
				if (RemainingToConsume > 0)
				{
					return ConsumeItemByTag(ItemTag, RemainingToConsume);
				}

				return true;
			}
		}
	}

	return false; // Brak przedmiotu w plecaku!
}

bool UInventoryComponent::HasItemWithTag(FGameplayTag ItemTag) const
{
	if (!ItemTag.IsValid()) return false;

	for (const FInventorySlot& Slot : StoredItems)
	{
		if (Slot.ItemData.ItemTag.MatchesTag(ItemTag))
		{
			return true;
		}
	}

	return false;
}