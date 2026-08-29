#include "ImSimSensorySubsystem.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void UImSimSensorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CleanupTimerHandle, this, &UImSimSensorySubsystem::CleanupExpiredStimuli, 1.0f, true);
	}
}

void UImSimSensorySubsystem::RegisterNoise(FVector Location, float Radius, FGameplayTag NoiseTag)
{
	if (Radius <= 0.0f) return;

	float CurrentTime = GetWorld()->GetTimeSeconds();

	// 1. FILTR KONKURENCJI (Odrzuca cichsze duplikaty w promieniu 4.5m):
	for (const FImSimStimulusEvent& ExistingEvent : ActiveStimuli)
	{
		static const FGameplayTag AcousticNoise = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
		if (ExistingEvent.StimulusTag.MatchesTag(AcousticNoise))
		{
			if (FVector::DistSquared(ExistingEvent.Location, Location) < (450.0f * 450.0f))
			{
				if (ExistingEvent.IntensityRadius >= Radius)
				{
					return; // ODRZUCAMY DUBLIKAT! (Zero spamu!)
				}
			}
		}
	}

	// 2. REJESTRUJEMY TYLKO DOMINUJĄCY HAŁAS:
	FImSimStimulusEvent NewNoise;
	NewNoise.Location = Location;
	NewNoise.IntensityRadius = Radius;
	NewNoise.StimulusTag = NoiseTag;
	NewNoise.ExpirationTime = CurrentTime + 0.35f;

	ActiveStimuli.Add(NewNoise);

#if !UE_BUILD_SHIPPING
	// Rysujemy okrąg na podłodze:
	DrawDebugCircle(GetWorld(), Location + FVector(0.0f, 0.0f, 5.0f), Radius, 24, FColor::Yellow, false, 0.35f, 0, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);

	// Logujemy na ekran TYLKO ten zaakceptowany dźwięk, który faktycznie słyszy AI:
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow,
			FString::Printf(TEXT("🔊 [AI SŁYSZY] Fala dźwiękowa: %.0f cm (%.1f m)"), Radius, Radius / 100.0f));
	}
#endif
}

void UImSimSensorySubsystem::RegisterScent(FVector Location, float Radius, FGameplayTag ScentTag, float Duration)
{
	if (Radius <= 0.0f) return;

	FImSimStimulusEvent NewScent;
	NewScent.Location = Location;
	NewScent.IntensityRadius = Radius;
	NewScent.StimulusTag = ScentTag;
	NewScent.ExpirationTime = GetWorld()->GetTimeSeconds() + Duration;

	ActiveStimuli.Add(NewScent);

#if !UE_BUILD_SHIPPING
	DrawDebugSphere(GetWorld(), Location, Radius, 12, FColor::Red, false, 2.0f, 0, 1.0f);
#endif
}

void UImSimSensorySubsystem::CleanupExpiredStimuli()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	ActiveStimuli.RemoveAll([CurrentTime](const FImSimStimulusEvent& Event) {
		return CurrentTime >= Event.ExpirationTime;
		});
}