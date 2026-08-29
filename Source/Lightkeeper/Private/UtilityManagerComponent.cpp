#include "UtilityManagerComponent.h"
#include "LanternComponent.h"
#include "InteractionComponent.h"
#include "Engine/Engine.h"

UUtilityManagerComponent::UUtilityManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxUnlockedSlots = 1;
}

void UUtilityManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Na start gry pierwszym domyślnym narzędziem na pasie jest Mosiężna Latarnia:
	if (EquippedUtilityTags.Num() == 0)
	{
		EquippedUtilityTags.Add(FGameplayTag::RequestGameplayTag(FName("Item.Utility.BeltLantern"), false));
	}
	ActiveSlotIndex = 0;
}

void UUtilityManagerComponent::ToggleActiveUtility()
{
	FGameplayTag ActiveTag = GetActiveUtilityTag();
	if (!ActiveTag.IsValid()) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// 1. Jeśli aktywnym narzędziem na pasie jest LATARNIA:
	static const FGameplayTag LanternTag = FGameplayTag::RequestGameplayTag(FName("Item.Utility.BeltLantern"), false);
	if (ActiveTag.MatchesTag(LanternTag))
	{
		if (ULanternComponent* Lantern = Owner->FindComponentByClass<ULanternComponent>())
		{
			Lantern->ToggleLantern();
		}
	}
	// 2. W PRZYSZŁOŚCI: Monokl Kobaltowy, Miernik Gazu itp.
}

void UUtilityManagerComponent::CycleNextUtility()
{
	if (EquippedUtilityTags.Num() <= 1) return;

	ActiveSlotIndex = (ActiveSlotIndex + 1) % EquippedUtilityTags.Num();
	FGameplayTag NewTag = GetActiveUtilityTag();
	OnActiveUtilityChanged.Broadcast(NewTag);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan,
			FString::Printf(TEXT("[PAS] Aktywne narzędzie: %s"), *NewTag.ToString()));
	}
}

bool UUtilityManagerComponent::RefillActiveUtility()
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	// ====================================================================
	// ZABEZPIECZENIE: Jeśli gracz bada przedmiot w dłoniach -> BLOKUJEMY DOLEWANIE!
	// ====================================================================
	if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
	{
		if (InterComp->IsInspecting() || InterComp->GetGrabbedComponent() != nullptr)
		{
			return false; // Gracz obraca przedmiot, ignorujemy klawisz R dla nafty!
		}
	}

	FGameplayTag ActiveTag = GetActiveUtilityTag();
	if (!ActiveTag.IsValid()) return false;

	static const FGameplayTag LanternTag = FGameplayTag::RequestGameplayTag(FName("Item.Utility.BeltLantern"), false);
	if (ActiveTag.MatchesTag(LanternTag))
	{
		if (ULanternComponent* Lantern = Owner->FindComponentByClass<ULanternComponent>())
		{
			return Lantern->RefillFuelFromInventory();
		}
	}

	return false;
}

FGameplayTag UUtilityManagerComponent::GetActiveUtilityTag() const
{
	if (EquippedUtilityTags.IsValidIndex(ActiveSlotIndex))
	{
		return EquippedUtilityTags[ActiveSlotIndex];
	}
	return FGameplayTag();
}