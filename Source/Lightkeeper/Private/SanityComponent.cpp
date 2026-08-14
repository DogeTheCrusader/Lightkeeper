#include "SanityComponent.h"

USanityComponent::USanityComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // Włączamy Tick do odliczania spadu w mroku
}

void USanityComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentSanity = GetMaxSanity();
	CurrentMadnessTier = 0;
}

void USanityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Jeśli gracz stoi w mroku -> Odejmujemy Sanity co sekundę
	if (bIsInDarkness && CurrentSanity > 0.0f)
	{
		ModifySanity(-DarknessDrainRate * DeltaTime);
	}
}

void USanityComponent::ModifySanity(float DeltaAmount)
{
	if (DeltaAmount == 0.0f) return;

	CurrentSanity = FMath::Clamp(CurrentSanity + DeltaAmount, 0.0f, GetMaxSanity());
	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());

	CheckMadnessThresholds();
}

void USanityComponent::CheckMadnessThresholds()
{
	if (CurrentSanity <= 0.0f)
	{
		if (CurrentMadnessTier == 0)
		{
			CurrentMadnessTier = 1;
			ApplySanityCap(0.75f); // Próg 1: Max Sanity spada do 75%
			OnBoutOfMadnessTriggered.Broadcast(1);
		}
		else if (CurrentMadnessTier == 1)
		{
			CurrentMadnessTier = 2;
			ApplySanityCap(0.50f); // Próg 2: Max Sanity spada do 50%
			OnBoutOfMadnessTriggered.Broadcast(2);
		}
		else if (CurrentMadnessTier >= 2)
		{
			// Próg 3: Omdlenie Psychiczne (Fail Forward - Koniec Nocy)
			OnPsychicFaint.Broadcast();
		}
	}
}

void USanityComponent::ApplySanityCap(float CapMultiplier)
{
	MaxSanityCapMultiplier = FMath::Clamp(CapMultiplier, 0.1f, 1.0f);
	CurrentSanity = GetMaxSanity(); // Odnawia do obecnego limitu
	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
}