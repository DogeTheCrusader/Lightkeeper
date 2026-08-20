#include "LanternComponent.h"
#include "SanityComponent.h"
#include "InventoryComponent.h"

ULanternComponent::ULanternComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULanternComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentFuel = MaxFuel;
	bIsLit = false;
}

void ULanternComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ==========================================================
	// 1. ZU¯YWANIE PALIWA GDY ŒWIECI
	// ==========================================================
	if (bIsLit)
	{
		CurrentFuel = FMath::Clamp(CurrentFuel - (FuelDrainRate * DeltaTime), 0.0f, MaxFuel);
		OnFuelChanged.Broadcast(CurrentFuel, MaxFuel);

		// Jeœli nafta siê skoñczy³a -> GASIMY LATARNIÊ!
		if (CurrentFuel <= 0.0f)
		{
			bIsLit = false;
			OnLanternToggled.Broadcast(false);
			OnFuelDepleted.Broadcast();

			// POWIADAMIAMY SANITY: Zgas³o jedno Ÿród³o œwiat³a!
			if (AActor* Owner = GetOwner())
			{
				if (USanityComponent* SanityComp = Owner->FindComponentByClass<USanityComponent>())
				{
					SanityComp->RemoveLightSource(); // <--- POPRAWIONA LINIJKA!
				}
			}

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[LATARNIA] Nafta wypalona! Latarnia zgasla."));
			}
		}
	}
}

void ULanternComponent::ToggleLantern()
{
	if (!bIsLit && !HasFuel())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[LATARNIA] Brak nafty!"));
		return;
	}

	bIsLit = !bIsLit;
	OnLanternToggled.Broadcast(bIsLit);

	// POWIADAMIAMY SANITY COMPONENT (Licznik Ÿróde³ œwiat³a):
	if (AActor* Owner = GetOwner())
	{
		if (USanityComponent* SanityComp = Owner->FindComponentByClass<USanityComponent>())
		{
			if (bIsLit)
			{
				SanityComp->AddLightSource(); // +1 ród³o Œwiat³a
			}
			else
			{
				SanityComp->RemoveLightSource(); // -1 ród³o Œwiat³a
			}
		}
	}

	if (GEngine)
	{
		if (bIsLit)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[LATARNIA] Swiatlo wlaczone. Sanity bezpieczne."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[LATARNIA] Zgaszono swiatlo."));
		}
	}
}

void ULanternComponent::RefillFuel(float Amount)
{
	if (Amount <= 0.0f) return;

	CurrentFuel = FMath::Clamp(CurrentFuel + Amount, 0.0f, MaxFuel);
	OnFuelChanged.Broadcast(CurrentFuel, MaxFuel);
}

bool ULanternComponent::RefillFuelFromInventory(float AmountPerBottle)
{
	if (CurrentFuel >= MaxFuel) return false;

	if (AActor* Owner = GetOwner())
	{
		if (UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>())
		{
			FGameplayTag OilTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Oil"), false);

			if (InvComp->HasItemWithTag(OilTag))
			{
				float ActualFuelToRestore = AmountPerBottle;

				for (int32 i = 0; i < InvComp->StoredItems.Num(); ++i)
				{
					if (InvComp->StoredItems[i].ItemData.ItemTag.MatchesTag(OilTag))
					{
						// Odczytujemy uniwersaln¹ wartoœæ PrimaryValue z butelki w plecaku:
						ActualFuelToRestore = InvComp->StoredItems[i].ItemData.PrimaryValue;

						InvComp->StoredItems.RemoveAt(i);
						InvComp->OnInventoryUpdated.Broadcast();
						break;
					}
				}

				// Dolewamy dok³adnie tyle paliwa, ile mia³a butelka:
				RefillFuel(ActualFuelToRestore);

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
						FString::Printf(TEXT("[LATARNIA] Dolano +%.1f nafty! Paliwo: %.1f / %.1f"), ActualFuelToRestore, CurrentFuel, MaxFuel));
				}

				return true;
			}
		}
	}

	return false;
}