#include "SensoryComponent.h"
#include "ImSimSensorySubsystem.h"
#include "LightkeeperCharacter.h"
#include "SanityComponent.h"
#include "LanternComponent.h"
#include "StatusEffectComponent.h"
#include "HealthComponent.h"
#include "SafeLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

USensoryComponent::USensoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.15f; // Skanujemy zmysły 6 razy na sekundę (optymalizacja CPU)
}

void USensoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USensoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	EvaluateSensoryStimuli();
}

void USensoryComponent::EvaluateSensoryStimuli()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	ALightkeeperCharacter* Player = Cast<ALightkeeperCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player || (Player->HealthComp && Player->HealthComp->IsDead()))
	{
		SetAIState(EAIBehaviorState::Idle_Patrol);
		return;
	}

	FVector MyLocation = OwnerActor->GetActorLocation();
	FVector PlayerLocation = Player->GetActorLocation();
	float DistToPlayer = FVector::Dist(MyLocation, PlayerLocation);

	// ====================================================================
	// 1. ZMYSŁ BLISKOŚCI / DOTYK (Personal Bubble = 2.2 metra):
	// Podejście do potwora na wyciągnięcie ręki = NATYCHMIASTOWY ATAK!
	// ====================================================================
	if (DistToPlayer <= 220.0f)
	{
		FVector Forward = OwnerActor->GetActorForwardVector();
		FVector DirToPlayer = (PlayerLocation - MyLocation).GetSafeNormal();

		// Jeśli gracz jest z przodu lub z boku (Dot > -0.2, czyli kąt mniejszy niż 100 stopni):
		if (FVector::DotProduct(Forward, DirToPlayer) > -0.2f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}
	}

	// ====================================================================
	// 2. STRACH PRZED ŚWIATŁEM (Panika Szabrownika):
	// ====================================================================
	if (SensoryProfile.bFleesFromLight)
	{
		if (Player->LanternComp && Player->LanternComp->bIsLit && DistToPlayer <= 1000.0f)
		{
			SetAIState(EAIBehaviorState::Flee_Panic, PlayerLocation);
			return;
		}
	}

	// ====================================================================
	// 3. FOTOTROPISM (Łaknienie Światła - Cień-Złodziej):
	// ====================================================================
	if (SensoryProfile.bIsPhototropic)
	{
		if (Player->LanternComp && Player->LanternComp->bIsLit && DistToPlayer <= 2200.0f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}
	}

	// ====================================================================
	// 4. TELEPATIA OBŁĘDU (Wyczuwanie Sanity - Skażone Widmo):
	// ====================================================================
	if (SensoryProfile.bHuntsLowSanity && Player->SanityComp)
	{
		float SanityPct = Player->SanityComp->CurrentSanity / FMath::Max(1.0f, Player->SanityComp->GetMaxSanity());
		if (SanityPct <= SensoryProfile.SanityHuntThreshold && DistToPlayer <= 2500.0f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}
	}

	// ====================================================================
	// 5. WZROK: THIEF LIGHT GEM (W Mroku gracz jest ukryty, w świetle widoczny z daleka!):
	// ====================================================================
	if (SensoryProfile.SightRadius > 0.0f)
	{
		float TargetVisibility = 0.05f; // Bazowo w głębokim cieniu: 5% widoczności

		// A. Zapalona latarnia na klacie [F] = 100% widoczności:
		if (Player->LanternComp && Player->LanternComp->bIsLit)
		{
			TargetVisibility = 1.0f;
		}
		// B. PŁYNNE SKALOWANIE ODLEGŁOŚCIĄ OD LAMPY W ŚWIECIE:
		else if (Player->SanityComp && Player->SanityComp->GetOverlappingLightSources().Num() > 0)
		{
			float HighestLightAlpha = 0.0f;

			for (USafeLightComponent* Light : Player->SanityComp->GetOverlappingLightSources())
			{
				if (Light && Light->bIsLightActive)
				{
					float DistToLight = FVector::Dist(PlayerLocation, Light->GetComponentLocation());

					// Używamy publicznego gettera GetScaledSphereRadius():
					float Radius = Light->GetScaledSphereRadius();

					float Alpha = 1.0f - FMath::Clamp(DistToLight / FMath::Max(1.0f, Radius), 0.0f, 1.0f);
					HighestLightAlpha = FMath::Max(HighestLightAlpha, Alpha);
				}
			}

			TargetVisibility = FMath::Lerp(0.05f, 0.75f, HighestLightAlpha);
		}

		// Rzeczywisty zasięg wzroku potwora w tej klatce:
		float EffectiveSightRadius = SensoryProfile.SightRadius * TargetVisibility;

#if !UE_BUILD_SHIPPING
		if (GEngine && DistToPlayer <= 2000.0f)
		{
			FString LightStateStr = (TargetVisibility >= 0.9f) ? TEXT("Zapalona Latarnia [F]") : (TargetVisibility > 0.1f ? TEXT("Płynne Światło Miejskie") : TEXT("Głęboki Cień"));
			FColor GemColor = (TargetVisibility >= 0.8f) ? FColor::Red : (TargetVisibility > 0.2f ? FColor::Yellow : FColor::Green);

			GEngine->AddOnScreenDebugMessage((uint64)GetOwner()->GetUniqueID() + 200, 0.2f, GemColor,
				FString::Printf(TEXT("👁️ [LIGHT GEM] Widoczność: %.0f%% (%s) | Zasięg Wzroku AI: %.1f m"), TargetVisibility * 100.0f, *LightStateStr, EffectiveSightRadius / 100.0f));
		}
#endif

		if (DistToPlayer <= EffectiveSightRadius)
		{
			FVector Forward = OwnerActor->GetActorForwardVector();
			FVector DirToPlayer = (PlayerLocation - MyLocation).GetSafeNormal();

			float Dot = FVector::DotProduct(Forward, DirToPlayer);
			float ActiveConeAngle = (TargetVisibility >= 0.8f) ? 75.0f : SensoryProfile.PeripheralVisionAngle;
			float ConeLimit = FMath::Cos(FMath::DegreesToRadians(ActiveConeAngle));

			if ((Dot >= ConeLimit || DistToPlayer <= 350.0f) && CheckLineOfSightToPlayer(Player))
			{
				SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
				return;
			}
		}
	}

	// ====================================================================
	// 6. SŁUCH I WĘCH (Z DYNAMICZNYM PRZEKIEROWYWANIEM NA NOWE HAŁASY):
	// ====================================================================
	if (UImSimSensorySubsystem* SensorySubsystem = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
	{
		for (const FImSimStimulusEvent& Stimulus : SensorySubsystem->GetActiveStimuli())
		{
			float DistToStimulus = FVector::Dist(MyLocation, Stimulus.Location);

			// A. SŁUCH:
			static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
			if (Stimulus.StimulusTag.MatchesTag(NoiseTag) && SensoryProfile.HearingSensitivity > 0.0f)
			{
				// ====================================================================
				// POPRAWKA: Ignorujemy hałas uderzenia na własnej klatce piersiowej (< 150 cm)!
				// Potwór nie bada własnych stóp, tylko idzie szukać sprawcy!
				// ====================================================================
				if (DistToStimulus < 150.0f) continue;

				float Multiplier = (CurrentState == EAIBehaviorState::Suspicious) ? 1.5f : 1.0f;
				float EffectiveHearingRadius = Stimulus.IntensityRadius * SensoryProfile.HearingSensitivity * Multiplier;

				if (!CheckAcousticOcclusion(Stimulus.Location))
				{
					EffectiveHearingRadius *= 0.5f;
				}

				if (DistToStimulus <= EffectiveHearingRadius)
				{
					// Cichy szmer z daleka -> SUSPICIOUS:
					if (Stimulus.IntensityRadius < 300.0f && DistToStimulus > 600.0f && CurrentState == EAIBehaviorState::Idle_Patrol)
					{
						SetAIState(EAIBehaviorState::Suspicious, Stimulus.Location);
						return;
					}

					// Wyraźny hałas -> INVESTIGATE:
					SetAIState(EAIBehaviorState::Investigate, Stimulus.Location);
					return;
				}
			}

			// B. WĘCH KRWI:
			if (SensoryProfile.TrackedScents.HasTag(Stimulus.StimulusTag))
			{
				static const FGameplayTag ScentMaskBuff = FGameplayTag::RequestGameplayTag(FName("Status.Buff.ScentMask"), false);
				if (Player->StatusComp && Player->StatusComp->HasStatusEffect(ScentMaskBuff)) continue;

				if (DistToStimulus <= SensoryProfile.ScentTrackingRadius)
				{
					SetAIState(EAIBehaviorState::Investigate, Stimulus.Location);
					return;
				}
			}
		}
	}
}

bool USensoryComponent::CheckLineOfSightToPlayer(ALightkeeperCharacter* Player) const
{
	if (!Player) return false;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	FVector Start = GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f); // Wysokość oczu potwora
	FVector End = Player->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);       // Klatka piersiowa gracza

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) return false;

		// Promień trafiający w ciało, latarnię lub wyekwipowaną broń gracza = WIDZĘ GO!
		if (HitActor == Player || HitActor->GetAttachParentActor() == Player || HitActor->GetOwner() == Player)
		{
			return true;
		}

		return false; // Ściana/szafa zasłania gracza
	}

	return true;
}

bool USensoryComponent::CheckAcousticOcclusion(FVector SoundOrigin) const
{
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	FVector Start = GetOwner()->GetActorLocation();

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, SoundOrigin, ECC_Visibility, Params))
	{
		return (FVector::DistSquared(Hit.ImpactPoint, SoundOrigin) < (35.0f * 35.0f));
	}
	return true;
}

void USensoryComponent::SetAIState(EAIBehaviorState NewState, FVector TargetLocation)
{
	bool bIsNewLocation = FVector::DistSquared(TargetLocation, FVector::ZeroVector) > 10.0f;

	if (CurrentState != NewState || (NewState == EAIBehaviorState::Investigate && bIsNewLocation))
	{
		CurrentState = NewState;
		OnAIStateChanged.Broadcast(CurrentState, TargetLocation);

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			FString StateStr;
			FColor StateColor = FColor::White;

			switch (NewState)
			{
			case EAIBehaviorState::Idle_Patrol: StateStr = TEXT("PATROL"); StateColor = FColor::Green; break;
			case EAIBehaviorState::Suspicious: StateStr = TEXT("SUSPICIOUS (Nasłuchiwanie)"); StateColor = FColor::Orange; break;
			case EAIBehaviorState::Investigate: StateStr = TEXT("INVESTIGATE (Marsz do hałasu)"); StateColor = FColor::Yellow; break;
			case EAIBehaviorState::Chase_Attack: StateStr = TEXT("CHASE (Wykrycie!)"); StateColor = FColor::Red; break;
			case EAIBehaviorState::Flee_Panic: StateStr = TEXT("FLEE (Ucieczka!)"); StateColor = FColor::Purple; break;
			}

			GEngine->AddOnScreenDebugMessage((uint64)GetOwner()->GetUniqueID() + 100, 1.5f, StateColor,
				FString::Printf(TEXT("👾 [%s] Stan AI: %s"), *GetOwner()->GetName(), *StateStr));
		}
#endif
	}
}