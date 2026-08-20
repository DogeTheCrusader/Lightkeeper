#include "SanityComponent.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

USanityComponent::USanityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USanityComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentSanity = GetMaxSanity();
	LowestSanityPercentInDarkness = 1.0f;
	MentalCollapseCount = 0;
	ActiveLightSourcesCount = 0;
	bIsInDarkness = true; // Domyślnie w nocy startujemy w ciemności!
}

void USanityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bIsProtected = (ActiveLightSourcesCount > 0) || IsInSanctuary();
	bIsInDarkness = !bIsProtected;

	bIsInDarkness = (ActiveLightSourcesCount <= 0);

	// ====================================================================
	// 1. W MROKU
	// ====================================================================
	if (bIsInDarkness)
	{
		TimeInDarkness += DeltaTime;

		// Jeśli nie jesteśmy na 0 -> spadek nieliniowy:
		if (CurrentSanity > 0.0f)
		{
			float CurrentDrain = BaseDarknessDrainRate * (1.0f + (TimeInDarkness * DarknessAccelerationFactor));
			if (bHasMinorMadness) CurrentDrain *= MinorMadnessDrainMultiplier;

			CurrentSanity = FMath::Clamp(CurrentSanity - (CurrentDrain * DeltaTime), 0.0f, GetMaxSanity());
			OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());

			float CurrentSanityPercent = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
			LowestSanityPercentInDarkness = FMath::Min(LowestSanityPercentInDarkness, CurrentSanityPercent);

			if (!bHasMinorMadness && CurrentSanityPercent < 0.40f && FMath::FRandRange(0.0f, 100.0f) < 2.0f)
			{
				TriggerRandomMinorMadness();
			}

			// NATYCHMIASTOWY BOUT GDY OSIĄGAMY ZERO!
			if (CurrentSanity <= 0.0f)
			{
				HandleSanityDepleted();
			}
		}
		// JEŚLI STOIMY NA 0 W CIEMNOŚCI -> ESKALACJA KOLEJNEGO BOUTA!
		else if (!bIsInMadnessSpike && MentalCollapseCount < 3)
		{
			TimeAtZeroInDarkness += DeltaTime;
			if (TimeAtZeroInDarkness >= GracePeriodBeforeNextBout)
			{
				TimeAtZeroInDarkness = 0.0f;
				HandleSanityDepleted(); // Kolejny Bout za brak ucieczki do światła!
			}
		}
	}
	// ====================================================================
	// 2. W ŚWIETLE (Leczenie tylko gdy minął szok szaleństwa!)
	// ====================================================================
	else
	{
		TimeInDarkness = 0.0f;
		TimeAtZeroInDarkness = 0.0f;

		// Leczymy tylko jeśli nie trwa ostry atak szoku:
		if (!bIsInMadnessSpike)
		{
			float DynamicCap = GetCurrentDynamicComfortCap();
			if (CurrentSanity < DynamicCap)
			{
				float SanityRatio = FMath::Clamp(CurrentSanity / FMath::Max(1.0f, GetMaxSanity()), 0.20f, 1.0f);
				float CurrentRecoverySpeed = BaseLightRecoveryRate * SanityRatio;

				CurrentSanity = FMath::FInterpConstantTo(CurrentSanity, DynamicCap, DeltaTime, CurrentRecoverySpeed);
				OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
			}
		}
	}
}

void USanityComponent::AddLightSource()
{
	ActiveLightSourcesCount++;
}

void USanityComponent::RemoveLightSource()
{
	ActiveLightSourcesCount = FMath::Max(0, ActiveLightSourcesCount - 1);
}

void USanityComponent::HandleSanityDepleted()
{
	if (MentalCollapseCount >= 3) return;

	MentalCollapseCount++;
	bIsInMadnessSpike = true; // ZACZYNA SIĘ OSTRE SZALEŃSTWO!

	// Uruchamiamy zegar trwania szoku (2 sekundy do testów):
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SpikeTimerHandle, this, &USanityComponent::EndMadnessSpike, MadnessSpikeDuration, false);
	}

	if (MentalCollapseCount == 1)
	{
		MaxSanityCapMultiplier = 0.75f;
		LowestSanityPercentInDarkness = 0.40f;
		TriggerMajorBoutOfMadness();
		OnPsychologicalCollapse.Broadcast(1);
	}
	else if (MentalCollapseCount == 2)
	{
		MaxSanityCapMultiplier = 0.50f;
		LowestSanityPercentInDarkness = 0.35f;
		TriggerMajorBoutOfMadness();
		OnPsychologicalCollapse.Broadcast(2);
	}
	else if (MentalCollapseCount >= 3)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[KONIEC NOCY] Ostateczne Załamanie Psychiczne! Przebudzenie w Hubie rano."));
		}
		OnTotalMentalBreakdown.Broadcast();
	}

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
}

void USanityComponent::SetInDarkness(bool bNewInDarkness)
{
	bIsInDarkness = bNewInDarkness;
	if (!bIsInDarkness)
	{
		TimeInDarkness = 0.0f;
	}
}

void USanityComponent::EndMadnessSpike()
{
	bIsInMadnessSpike = false;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[SZALEŃSTWO] Szok minął. Jeśli nie znajdziesz światła, nastąpi kolejny atak!"));
	}
}

float USanityComponent::GetCurrentDynamicComfortCap() const
{
	float CapPercent = FMath::Clamp(LowestSanityPercentInDarkness + PassiveRecoveryWindowPercent, 0.35f, 1.0f);
	return GetMaxSanity() * CapPercent;
}

void USanityComponent::TakeSanityDamage(float DamageAmount, FGameplayTag ShockTag)
{
	if (DamageAmount <= 0.0f) return;

	float FinalDamage = bHasMinorMadness ? (DamageAmount * MinorMadnessDrainMultiplier) : DamageAmount;
	CurrentSanity = FMath::Clamp(CurrentSanity - FinalDamage, 0.0f, GetMaxSanity());

	float CurrentSanityPercent = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
	LowestSanityPercentInDarkness = FMath::Min(LowestSanityPercentInDarkness, CurrentSanityPercent);

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());

	if (CurrentSanity <= 0.0f)
	{
		HandleSanityDepleted();
	}
}

void USanityComponent::RestoreSanity(float RestoreAmount)
{
	if (RestoreAmount <= 0.0f) return;

	CurrentSanity = FMath::Clamp(CurrentSanity + RestoreAmount, 0.0f, GetMaxSanity());
	LowestSanityPercentInDarkness = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
	bHasMinorMadness = false;

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
}

void USanityComponent::TriggerRandomMinorMadness()
{
	bHasMinorMadness = true;
	FGameplayTag MinorTag = FGameplayTag::RequestGameplayTag(FName("Status.Madness.Minor.Whispers"), false);
	OnMinorMadnessTriggered.Broadcast(MinorTag);
}

void USanityComponent::TriggerMajorBoutOfMadness()
{
	FGameplayTag BoutTag = FGameplayTag::RequestGameplayTag(FName("Status.Madness.Major.Scotophobia"), false);
	ActiveMadnessTags.AddTag(BoutTag);
	OnBoutOfMadnessTriggered.Broadcast(BoutTag);
}