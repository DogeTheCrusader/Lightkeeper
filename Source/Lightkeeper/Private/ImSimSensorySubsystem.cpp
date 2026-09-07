#include "ImSimSensorySubsystem.h"
#include "PhysicalInteract.h"
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

float UImSimSensorySubsystem::GetMaterialNoiseMultiplier(FGameplayTag MaterialTag)
{
	if (!MaterialTag.IsValid()) return 1.0f;

	static const FGameplayTag GlassTag = FGameplayTag::RequestGameplayTag(FName("Material.Glass"), false);
	static const FGameplayTag MetalTag = FGameplayTag::RequestGameplayTag(FName("Material.Metal"), false);
	static const FGameplayTag WaterTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Moisture.Water"), false);
	static const FGameplayTag FleshTag = FGameplayTag::RequestGameplayTag(FName("Material.Flesh"), false);

	if (MaterialTag.MatchesTag(GlassTag)) return 1.8f; // Szkło = +80%
	if (MaterialTag.MatchesTag(MetalTag)) return 1.5f; // Metal = +50%
	if (MaterialTag.MatchesTag(WaterTag)) return 1.4f; // Woda = +40%
	if (MaterialTag.MatchesTag(FleshTag)) return 0.5f; // Ciało = -50%

	return 1.0f; // Drewno/Kamień = 1.0x
}

float UImSimSensorySubsystem::ExtractNoiseMultiplierFromActor(AActor* TargetActor)
{
	if (!TargetActor) return 1.0f;

	if (TargetActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		return IPhysicalInteract::Execute_GetAcousticNoiseMultiplier(TargetActor);
	}

	return 1.0f;
}

FGameplayTag UImSimSensorySubsystem::ExtractMaterialTagFromActor(AActor* TargetActor)
{
	if (!TargetActor) return FGameplayTag::EmptyTag;

	// 1. Sprawdzamy interfejs IPhysicalInteract (Mebel / Kłódka / Prop):
	if (TargetActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		FGameplayTag MatTag = IPhysicalInteract::Execute_GetMaterialTag(TargetActor);
		if (MatTag.IsValid()) return MatTag;
	}

	// 2. NAJPIERW SPRAWDZAMY PRZYPISANY TAG MATERIAŁU (Dla kamiennych posągów, mosiężnych automatów i szkła!):
	for (const FName& TagName : TargetActor->Tags)
	{
		FString TagStr = TagName.ToString();
		if (TagStr.StartsWith(TEXT("Material.")) || TagStr.StartsWith(TEXT("State.Element.Moisture.Water")))
		{
			return FGameplayTag::RequestGameplayTag(TagName, false);
		}
	}

	// 3. DOPIERO JEŚLI BRAK SPECYFICZNEGO TAGU: Zwykły człowiek/potwór biologiczny = Ciało (Flesh):
	if (TargetActor->IsA<APawn>())
	{
		static const FGameplayTag FleshTag = FGameplayTag::RequestGameplayTag(FName("Material.Flesh"), false);
		return FleshTag;
	}

	return FGameplayTag::EmptyTag; // Domyślne drewno/kamień -> mnożnik 1.0x
}

void UImSimSensorySubsystem::RegisterNoise(FVector Location, float Radius, FGameplayTag NoiseTag, FGameplayTag MaterialTag, float CustomMultiplier)
{
	if (Radius <= 0.0f) return;

	// 1. Wyliczamy ostateczny promień: Baza * Mnożnik Materiału * Mnożnik Sytuacyjny:
	float MaterialMod = GetMaterialNoiseMultiplier(MaterialTag);
	float SafeCustomMod = FMath::Max(0.0f, CustomMultiplier);
	float FinalRadius = Radius * MaterialMod * SafeCustomMod;

	if (FinalRadius <= 0.0f) return; // 100% cisza

	float CurrentTime = GetWorld()->GetTimeSeconds();

	// 2. Filtr konkurencji (Acoustic Cluster Filter):
	for (const FImSimStimulusEvent& ExistingEvent : ActiveStimuli)
	{
		static const FGameplayTag AcousticNoise = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
		if (ExistingEvent.StimulusTag.MatchesTag(AcousticNoise))
		{
			if (FVector::DistSquared(ExistingEvent.Location, Location) < (450.0f * 450.0f))
			{
				if (ExistingEvent.IntensityRadius >= FinalRadius)
				{
					return; // Odrzucamy cichszy duplikat z tej samej klatki!
				}
			}
		}
	}

	// 3. Rejestrujemy czysty bodziec:
	FImSimStimulusEvent NewNoise;
	NewNoise.Location = Location;
	NewNoise.IntensityRadius = FinalRadius;
	NewNoise.StimulusTag = NoiseTag;
	NewNoise.ExpirationTime = CurrentTime + 0.35f;

	ActiveStimuli.Add(NewNoise);

#if !UE_BUILD_SHIPPING
	DrawDebugCircle(GetWorld(), Location + FVector(0.0f, 0.0f, 5.0f), FinalRadius, 24, FColor::Yellow, false, 0.35f, 0, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);

	if (GEngine)
	{
		FString MatStr = MaterialTag.IsValid() ? FString::Printf(TEXT(" [Materiał: %s x%.1f]"), *MaterialTag.ToString(), MaterialMod) : TEXT("");
		FString CustomStr = (CustomMultiplier != 1.0f) ? FString::Printf(TEXT(" [Mnożnik: x%.2f]"), CustomMultiplier) : TEXT("");

		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
			FString::Printf(TEXT("🔊 [AI SŁYSZY] Fala dźwiękowa: %.0f cm (%.1f m)%s%s"), FinalRadius, FinalRadius / 100.0f, *MatStr, *CustomStr));
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
	// Rysujemy czerwony okrąg zapachu na ziemi, widoczny przez 15 sekund!
	DrawDebugCircle(GetWorld(), Location + FVector(0.0f, 0.0f, 5.0f), Radius, 32, FColor::Red, false, Duration, 0, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
			FString::Printf(TEXT("🩸 [ZAPACH] Upuszczono ślad krwi! Trwa %.0fs, Węch: %.0f cm"), Duration, Radius));
	}
#endif
}

void UImSimSensorySubsystem::CleanupExpiredStimuli()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	ActiveStimuli.RemoveAll([CurrentTime](const FImSimStimulusEvent& Event) {
		return CurrentTime >= Event.ExpirationTime;
		});
}