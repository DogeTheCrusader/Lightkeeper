#include "ProgressionComponent.h"

UProgressionComponent::UProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Inicjalizacja domyślnego stanu (Tier 0)
	Stats.Add(ECharacterStat::Vigor, FStatData());
	Stats.Add(ECharacterStat::Precision, FStatData());
	Stats.Add(ECharacterStat::Engineering, FStatData());
	Stats.Add(ECharacterStat::Deduction, FStatData());
	Stats.Add(ECharacterStat::Occultism, FStatData());
}

void UProgressionComponent::BeginPlay()
{
	Super::BeginPlay();
	// (Tymczasowo) Od razu ładujemy perki na start dema:
	Debug_UnlockDemoPerks();
}

int32 UProgressionComponent::GetStatTier(ECharacterStat Stat) const
{
	if (const FStatData* StatData = Stats.Find(Stat))
	{
		return StatData->CurrentTier;
	}
	return 0;
}

bool UProgressionComponent::HasPerk(FGameplayTag PerkTag) const
{
	return UnlockedPerks.HasTag(PerkTag);
}

void UProgressionComponent::Debug_UnlockDemoPerks()
{
	// Dźwigar (HeavyLifter)
	UnlockedPerks.AddTag(FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.HeavyLifter")));
	if (FStatData* VigorData = Stats.Find(ECharacterStat::Vigor)) VigorData->CurrentTier = 1;

	// Mistrz Włamań (MasterLocksmith)
	UnlockedPerks.AddTag(FGameplayTag::RequestGameplayTag(FName("Perk.Precision.MasterLocksmith")));
	if (FStatData* PrecData = Stats.Find(ECharacterStat::Precision)) PrecData->CurrentTier = 2;

	UE_LOG(LogTemp, Warning, TEXT("[Progression] Demo Perks Unlocked: HeavyLifter & MasterLocksmith"));
}