#include "SanityComponent.h"
#include "SafeLightComponent.h"
#include "LightkeeperCharacter.h"
#include "LanternComponent.h"
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
	SanctuaryZonesCount = 0;
	bIsGracePeriodActive = false;
	bIsAdrenalineActive = false;
	bIsInDarkness = true;
}

void USanityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bIsPhysicallyIlluminated = false;

	// 1. NAJPIERW SPRAWDZAMY CZY JESTEŚMY W BEZPIECZNYM POKOJU (Sanctuary):
	if (IsInSanctuary())
	{
		bIsPhysicallyIlluminated = true;
	}

	// 2. NASTĘPNIE SPRAWDZAMY CZY LATARNIA GRACZA JEST WŁĄCZONA:
	if (!bIsPhysicallyIlluminated)
	{
		if (ALightkeeperCharacter* Player = Cast<ALightkeeperCharacter>(GetOwner()))
		{
			if (Player->LanternComp && Player->LanternComp->bIsLit)
			{
				bIsPhysicallyIlluminated = true;
			}
		}
	}

	// 3. JEŚLI LATARNIA JEST ZGASZONA -> SPRAWDZAMY ZEWNĘTRZNE ŚWIATŁA ZE ŚWIATA (RAYCAST):
	if (!bIsPhysicallyIlluminated)
	{
		bIsPhysicallyIlluminated = CheckLightLineOfSight();
	}

	// 4. DOPIERO TERAZ PRZYPISUJEMY WYNIK DO GŁÓWNEJ ZMIENNEJ:
	bIsInDarkness = !bIsPhysicallyIlluminated;


	// ====================================================================
	// 1. W MROKU
	// ====================================================================
	if (bIsInDarkness)
	{
		TimeInDarkness += DeltaTime;

		// Płynny drenaż: im bliżej krawędzi światła stoisz, tym drenaż jest wolniejszy (nawet o 85%!):
		float LightProximityPenalty = 1.0f;
		if (OverlappingLightSources.Num() > 0)
		{
			float HighestAlpha = 0.0f;
			for (USafeLightComponent* Light : OverlappingLightSources)
			{
				if (Light && Light->bIsLightActive)
				{
					float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Light->GetComponentLocation());
					float Alpha = 1.0f - FMath::Clamp(Dist / FMath::Max(1.0f, Light->GetScaledSphereRadius()), 0.0f, 1.0f);
					HighestAlpha = FMath::Max(HighestAlpha, Alpha);
				}
			}
			LightProximityPenalty = FMath::Clamp(1.0f - HighestAlpha, 0.15f, 1.0f); // Do 85% wolniejszy drenaż przy krawędzi!
		}

		float CurrentDrain = (BaseDarknessDrainRate * LightProximityPenalty) * (1.0f + (TimeInDarkness * DarknessAccelerationFactor));
		if (bHasMinorMadness) CurrentDrain *= MinorMadnessDrainMultiplier;

		float MinAllowedSanity = bIsGracePeriodActive ? (BaseMaxSanity * 0.25f) : 0.0f;

		CurrentSanity = FMath::Clamp(CurrentSanity - (CurrentDrain * DeltaTime), MinAllowedSanity, GetMaxSanity());
		OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());

		// Zapisujemy traumę TYLKO WTEDY, gdy nie trwa Grace Period:
		if (!bIsGracePeriodActive)
		{
			float CurrentSanityPercent = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
			LowestSanityPercentInDarkness = FMath::Min(LowestSanityPercentInDarkness, CurrentSanityPercent);
		}

		if (!bHasMinorMadness && (CurrentSanity / FMath::Max(1.0f, GetMaxSanity())) < 0.40f && FMath::FRandRange(0.0f, 100.0f) < 2.0f)
		{
			TriggerRandomMinorMadness();
		}

		if (CurrentSanity <= 0.0f && !bIsGracePeriodActive)
		{
			HandleSanityDepleted();
		}
	}
	// ====================================================================
	// 2. W ŚWIETLE / BEZPIECZNYM POKOJU
	// ====================================================================
	else
	{
		TimeInDarkness = 0.0f;

		float DynamicCap = GetCurrentDynamicComfortCap();
		if (CurrentSanity < DynamicCap)
		{
			float SanityRatio = FMath::Clamp(CurrentSanity / FMath::Max(1.0f, GetMaxSanity()), 0.20f, 1.0f);
			float BaseRate = bIsAdrenalineActive ? AdrenalineRecoveryRate : BaseLightRecoveryRate;
			float EffectiveRecoverySpeed = BaseRate * SanityRatio;

			CurrentSanity = FMath::FInterpConstantTo(CurrentSanity, DynamicCap, DeltaTime, EffectiveRecoverySpeed);
			OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
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
	bIsGracePeriodActive = true; // Włączamy 10s ochrony
	bIsAdrenalineActive = true;   // Gotowy na szybki zryw w świetle

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GracePeriodTimerHandle, this, &USanityComponent::EndGracePeriod, GracePeriodDuration, false);
		World->GetTimerManager().SetTimer(AdrenalineTimerHandle, this, &USanityComponent::EndAdrenalineSurge, AdrenalineDuration, false);
	}

	// ====================================================================
	// RESET TRAUMY: Pozwala na wyleczenie się w świetle do PEŁNEGO nowego limitu!
	// ====================================================================
	if (MentalCollapseCount == 1)
	{
		MaxSanityCapMultiplier = 0.75f; // Nowy limit: 75.0
		LowestSanityPercentInDarkness = 1.0f;
		CurrentSanity = BaseMaxSanity * 0.25f; // <--- ZAWSZE RÓWNE 25.0 PKT!
		TriggerMajorBoutOfMadness();
		OnPsychologicalCollapse.Broadcast(1);
	}
	else if (MentalCollapseCount == 2)
	{
		MaxSanityCapMultiplier = 0.50f; // Nowy limit: 50.0
		LowestSanityPercentInDarkness = 1.0f;
		CurrentSanity = BaseMaxSanity * 0.25f; // <--- ZAWSZE RÓWNE 25.0 PKT!
		TriggerMajorBoutOfMadness();
		OnPsychologicalCollapse.Broadcast(2);
	}
	else if (MentalCollapseCount >= 3)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[KONIEC NOCY] Ostateczne Załamanie Psychiczne! Przebudzenie w Hubie rano."));
		OnTotalMentalBreakdown.Broadcast();
	}

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
}

void USanityComponent::EndGracePeriod()
{
	bIsGracePeriodActive = false; // Immunitet 10 sekund wygasł!

	// Jeśli po 10 sekundach wciąż stoisz w mroku na zerze -> KOLEJNA ZAPAŚĆ!
	if (bIsInDarkness && CurrentSanity <= 0.0f)
	{
		HandleSanityDepleted();
	}
}

void USanityComponent::EndAdrenalineSurge()
{
	bIsAdrenalineActive = false; // Powrót do normalnego tempa 4.0 pkt/s
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

	if (CurrentSanity <= 0.0f && !bIsGracePeriodActive)
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

void USanityComponent::RegisterPotentialLight(USafeLightComponent* LightComp)
{
	if (LightComp && !OverlappingLightSources.Contains(LightComp))
	{
		OverlappingLightSources.Add(LightComp);
	}
}

void USanityComponent::UnregisterPotentialLight(USafeLightComponent* LightComp)
{
	if (LightComp)
	{
		OverlappingLightSources.Remove(LightComp);
	}
}

bool USanityComponent::CheckLightLineOfSight()
{
	if (OverlappingLightSources.Num() == 0) return false;

	AActor* Player = GetOwner();
	if (!Player) return false;

	FVector PlayerLocation = Player->GetActorLocation();
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Player); // Promień ignoruje gracza

	for (USafeLightComponent* Light : OverlappingLightSources)
	{
		if (!Light || !Light->bIsLightActive) continue;

		FVector LightLocation = Light->GetComponentLocation();
		FHitResult Hit;

		// Rzucamy promień z gracza do żarówki
		bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			PlayerLocation,
			LightLocation,
			ECC_Visibility, // Kanał kolizji. Opcjonalnie zmienisz na własny, by ignorował szkło
			TraceParams
		);

		// Jeśli promień nie trafił w ścianę (lub trafił prosto w mebel ze światłem)
		if (!bHit || Hit.GetActor() == Light->GetOwner())
		{
			return true; // Sukces! Przynajmniej jedno światło nas oświetla
		}
	}

	return false; // Wszystkie światła są za ścianą!
}