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

	// ====================================================================
	// TELEMETRIA HP (Pomarańczowy napis):
	// ====================================================================
	if (GEngine)
	{
		FString TypeStr = DamageTypeTag.IsValid() ? DamageTypeTag.ToString() : TEXT("Fizyczne");
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
			FString::Printf(TEXT("[HP] %s | OBRAŻENIA: -%.1f HP | POZOSTAŁO: %.1f / %.1f HP | TYP: %s"),
				*GetOwner()->GetName(), DamageAmount, CurrentHealth, GetMaxHealth(), *TypeStr));
	}

	// Test Paska Pęknięcia
	FractureMeter = FMath::Clamp(FractureMeter + FractureIncreasePerHit, 0.0f, 1.0f);
	TestFractureInjury(DamageTypeTag);

	// ====================================================================
	// TELEMETRIA ŚMIERCI (Czerwony napis):
	// ====================================================================
	if (CurrentHealth <= 0.0f)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red,
				FString::Printf(TEXT("[ZNISZCZENIE] %s ZOSTAŁ CAŁKOWICIE ZNISZCZONY!"), *GetOwner()->GetName()));
		}
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