#include "ProgressionComponent.h"
#include "Engine/Engine.h"

UProgressionComponent::UProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProgressionComponent::BeginPlay()
{
	Super::BeginPlay();
}

int32 UProgressionComponent::GetStatTier(ECharacterStat Stat) const
{
	int32 Tier = 0;

	// 1. Sprawdzamy czy wpisano cyfrę w mapie StatTiers (np. Precision = 2):
	if (const int32* FoundTier = StatTiers.Find(Stat))
	{
		Tier = *FoundTier;
	}

	// 2. Automatyczne wykrycie z dodanych tagów UnlockedPerks:
	if (Stat == ECharacterStat::Precision)
	{
		static const FGameplayTag Prec1A = FGameplayTag::RequestGameplayTag(FName("Perk.Precision.1A"), false);
		static const FGameplayTag Prec2A = FGameplayTag::RequestGameplayTag(FName("Perk.Precision.2A"), false);

		if (HasPerk(Prec2A)) Tier = FMath::Max(Tier, 2);
		else if (HasPerk(Prec1A)) Tier = FMath::Max(Tier, 1);
	}
	else if (Stat == ECharacterStat::Vigor)
	{
		static const FGameplayTag Vigor1A = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.1A"), false);
		static const FGameplayTag Vigor2A = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.2A"), false);

		if (HasPerk(Vigor2A)) Tier = FMath::Max(Tier, 2);
		else if (HasPerk(Vigor1A)) Tier = FMath::Max(Tier, 1);
	}

	return FMath::Clamp(Tier, 0, 3);
}

bool UProgressionComponent::HasPerk(FGameplayTag PerkTag) const
{
	return UnlockedPerks.HasTag(PerkTag);
}

void UProgressionComponent::Debug_UnlockDemoPerks()
{
	static const FGameplayTag Vigor1A = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.1A"), false);
	if (Vigor1A.IsValid()) UnlockedPerks.AddTag(Vigor1A);

	static const FGameplayTag Vigor2A = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.2A"), false);
	if (Vigor2A.IsValid()) UnlockedPerks.AddTag(Vigor2A);

	static const FGameplayTag Prec2A = FGameplayTag::RequestGameplayTag(FName("Perk.Precision.2A"), false);
	if (Prec2A.IsValid()) UnlockedPerks.AddTag(Prec2A);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan, TEXT("🎯 [PROGRESJA] Aktywne Perki: Wigor 1A, 2A & Precyzja 2A!"));
	}
#endif
}

// ====================================================================
// 1. STATYSTYKI PASYWNE WIGORU (4:4)
// ====================================================================
float UProgressionComponent::GetMaxHealthBonus() const
{
	return GetStatTier(ECharacterStat::Vigor) * 25.0f; // +25 HP za każdy Tier
}

float UProgressionComponent::GetMaxStaminaBonus() const
{
	return GetStatTier(ECharacterStat::Vigor) * 25.0f; // +25 Staminy za każdy Tier
}

float UProgressionComponent::GetStaminaCostReductionMultiplier() const
{
	int32 VigorTier = GetStatTier(ECharacterStat::Vigor);
	return FMath::Clamp(1.0f - (VigorTier * 0.15f), 0.55f, 1.0f); // -15% kosztu staminy za Tier
}

float UProgressionComponent::GetPhysicalStrengthMultiplier() const
{
	int32 VigorTier = GetStatTier(ECharacterStat::Vigor);
	return 1.0f + (VigorTier * 0.15f); // +15% siły ciosu i rzutu za Tier!
}

// ====================================================================
// 2. STATYSTYKI PASYWNE PRECYZJI (4:4)
// ====================================================================
float UProgressionComponent::GetCrouchSpeedMultiplier() const
{
	int32 PrecisionTier = GetStatTier(ECharacterStat::Precision);
	return 1.0f + (PrecisionTier * 0.25f); // +25% prędkości w kucaniu za Tier!
}

float UProgressionComponent::GetGeneralMovementSpeedMultiplier() const
{
	int32 PrecisionTier = GetStatTier(ECharacterStat::Precision);
	return 1.0f + (PrecisionTier * 0.05f); // +5% do ogólnego chodu i sprintu za Tier
}

float UProgressionComponent::GetJumpBonusMultiplier() const
{
	int32 PrecisionTier = GetStatTier(ECharacterStat::Precision);
	return 1.0f + (PrecisionTier * 0.15f); // +15% wyższy skok za Tier
}

float UProgressionComponent::GetSafeFallSpeedBonus() const
{
	return HasAcrobaticSoftLanding() ? 450.0f : 0.0f; // +4.5m bezpiecznego zeskoku
}

float UProgressionComponent::GetStealthNoiseMultiplier() const
{
	int32 PrecisionTier = GetStatTier(ECharacterStat::Precision);
	return FMath::Clamp(1.0f - (PrecisionTier * 0.20f), 0.40f, 1.0f); // -20% hałasu za Tier
}

// ====================================================================
// 3. BRAMKI ZDOLNOŚCI (CAPABILITY GATES)
// ====================================================================
bool UProgressionComponent::CanPerformHeavyMelee() const
{
	static const FGameplayTag HeavyAttackPerk = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.1A"), false);
	return HasPerk(HeavyAttackPerk) || (GetStatTier(ECharacterStat::Vigor) >= 1);
}

bool UProgressionComponent::CanLiftHeavyProps() const
{
	static const FGameplayTag HeavyLifterPerk = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.2A"), false);
	return HasPerk(HeavyLifterPerk) || (GetStatTier(ECharacterStat::Vigor) >= 2);
}

bool UProgressionComponent::CanPickLocks() const
{
	static const FGameplayTag LockpickPerk = FGameplayTag::RequestGameplayTag(FName("Perk.Precision.2A"), false);
	return HasPerk(LockpickPerk) || (GetStatTier(ECharacterStat::Precision) >= 2);
}

bool UProgressionComponent::HasAcrobaticSoftLanding() const
{
	static const FGameplayTag AcrobatPerk = FGameplayTag::RequestGameplayTag(FName("Perk.Precision.1A"), false);
	return HasPerk(AcrobatPerk) || (GetStatTier(ECharacterStat::Precision) >= 1);
}