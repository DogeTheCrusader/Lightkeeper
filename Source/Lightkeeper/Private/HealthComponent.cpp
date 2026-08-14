#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = GetMaxHealth();
	FractureMeter = 0.0f;
}

void UHealthComponent::TakeDamage(float DamageAmount, FGameplayTag DamageTypeTag)
{
	if (DamageAmount <= 0.0f || IsDead()) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, GetMaxHealth());
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());

	// 1. ZWIĘKSZAMY PASEK PĘKNIĘCIA O +5% (0.05)
	FractureMeter = FMath::Clamp(FractureMeter + FractureIncreasePerHit, 0.0f, 1.0f);

	// 2. TEST RNG NA URAZ (Losowanie 0.0 - 1.0)
	TestFractureInjury(DamageTypeTag);

	// 3. ŚMIERĆ / OMDLENIE
	if (CurrentHealth <= 0.0f)
	{
		OnDeath.Broadcast();
	}
}

void UHealthComponent::TestFractureInjury(FGameplayTag DamageTypeTag)
{
	float RandomRoll = FMath::FRandRange(0.0f, 1.0f);

	// Jeśli wylosowana liczba jest mniejsza niż nasz Pasek Pęknięcia -> URAZ!
	if (RandomRoll < FractureMeter)
	{
		// Wyznaczamy jaki uraz wywołać na podstawie typu obrażeń (np. Nogi / Ręce)
		FGameplayTag TriggeredInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Sprain"));
		OnInjuryTriggered.Broadcast(TriggeredInjury);
	}
}

void UHealthComponent::ApplyFaintCap(float CapMultiplier)
{
	MaxHealthCapMultiplier = FMath::Clamp(CapMultiplier, 0.1f, 1.0f);
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, GetMaxHealth());
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
}

void UHealthComponent::Heal(float HealAmount)
{
	if (HealAmount <= 0.0f || IsDead()) return;

	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, GetMaxHealth());
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
}