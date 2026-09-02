#include "SensoryComponent.h"
#include "BaseEnemyCharacter.h" 
#include "ImSimSensorySubsystem.h"
#include "SanityComponent.h"
#include "StaminaComponent.h"
#include "LanternComponent.h"
#include "StatusEffectComponent.h"
#include "HealthComponent.h"
#include "InteractionComponent.h"
#include "SafeLightComponent.h"
#include "ReactionReceiverComponent.h"
#include "ProgressionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USensoryComponent::USensoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.10f; // Skanujemy zmysły 10 razy na sekundę
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

	// ====================================================================
	// 1. POBIERANIE REFERENCJI I KOMPONENTÓW
	// ====================================================================
	UStatusEffectComponent* OwnerStatus = OwnerActor->FindComponentByClass<UStatusEffectComponent>();
	UReactionReceiverComponent* OwnerReaction = OwnerActor->FindComponentByClass<UReactionReceiverComponent>();
	ABaseEnemyCharacter* EnemyChar = Cast<ABaseEnemyCharacter>(OwnerActor);

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		SetAIState(EAIBehaviorState::Idle_Patrol);
		return;
	}

	UHealthComponent* PlayerHealth = PlayerPawn->FindComponentByClass<UHealthComponent>();
	if (PlayerHealth && PlayerHealth->IsDead())
	{
		SetAIState(EAIBehaviorState::Idle_Patrol);
		return;
	}

	FVector MyLocation = OwnerActor->GetActorLocation();
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	float DistToPlayer = FVector::Dist(MyLocation, PlayerLocation);

	ULanternComponent* PlayerLantern = PlayerPawn->FindComponentByClass<ULanternComponent>();
	USanityComponent* PlayerSanity = PlayerPawn->FindComponentByClass<USanityComponent>();
	UStaminaComponent* PlayerStamina = PlayerPawn->FindComponentByClass<UStaminaComponent>();
	UStatusEffectComponent* PlayerStatus = PlayerPawn->FindComponentByClass<UStatusEffectComponent>();

	bool bIsIlluminatedByLantern = (PlayerLantern && PlayerLantern->bIsLit);
	bool bIsInStreetLight = (PlayerSanity && PlayerSanity->GetOverlappingLightSources().Num() > 0);

	// ====================================================================
	// 2. KALKULACJA WIDOCZNOŚCI I ZASIĘGU WZROKU
	// ====================================================================
	float TargetVisibility = 0.05f;
	if (SensoryProfile.bHasNightVision || bIsIlluminatedByLantern)
	{
		TargetVisibility = 1.0f;
	}
	else if (bIsInStreetLight && PlayerSanity)
	{
		float HighestLightAlpha = 0.0f;
		for (USafeLightComponent* Light : PlayerSanity->GetOverlappingLightSources())
		{
			if (Light && Light->bIsLightActive)
			{
				float DistToLight = FVector::Dist(PlayerLocation, Light->GetComponentLocation());
				float Radius = Light->GetScaledSphereRadius();
				float Alpha = 1.0f - FMath::Clamp(DistToLight / FMath::Max(1.0f, Radius), 0.0f, 1.0f);
				HighestLightAlpha = FMath::Max(HighestLightAlpha, Alpha);
			}
		}
		TargetVisibility = FMath::Lerp(0.05f, 0.90f, HighestLightAlpha);
	}

	// ====================================================================
	// MODYFIKATOR KUCANIA I PRECYZJI RPG (Stealth Stance Modifier):
	// ====================================================================
	if (PlayerPawn)
	{
		UCharacterMovementComponent* MoveComp = PlayerPawn->FindComponentByClass<UCharacterMovementComponent>();
		UCapsuleComponent* Capsule = PlayerPawn->FindComponentByClass<UCapsuleComponent>();

		bool bCrouched = (MoveComp && MoveComp->IsCrouching()) ||
			(Capsule && Capsule->GetScaledCapsuleHalfHeight() < 70.0f);

		if (bCrouched)
		{
			// 1. W pełnym świetle (1.0) kucanie daje mały bonus (0.88x), w głębokim mroku (0.05) potężny (0.50x!):
			float DynamicCrouchMod = FMath::Lerp(0.50f, 0.88f, TargetVisibility);

			// 2. Bonus z poziomu Precyzji (używa istniejącego gettera z ProgressionComponent!):
			if (UProgressionComponent* ProgComp = PlayerPawn->FindComponentByClass<UProgressionComponent>())
			{
				DynamicCrouchMod *= ProgComp->GetGeneralMovementSpeedMultiplier();
			}

			// Aplikujemy dynamiczny modyfikator:
			TargetVisibility = FMath::Clamp(TargetVisibility * DynamicCrouchMod, 0.02f, 1.0f);
		}
	}

	// Zasięg wzroku potwora wyliczony z dynamicznej widoczności:
	float BaseSight = (SensoryProfile.SightRadius > 0.0f) ? SensoryProfile.SightRadius : 1500.0f;
	float CalculatedSight = BaseSight * TargetVisibility;

	// W zwarciu (< 4.5m) w pościgu zasięg wzroku w mroku nie spada poniżej 4.5m:
	float EffectiveSightRadius = (CurrentState == EAIBehaviorState::Chase_Attack) ? FMath::Max(450.0f, CalculatedSight) : CalculatedSight;

	bool bHasClearLoS = CheckLineOfSightToTarget(PlayerPawn);
	bool bIsStunned = (OwnerStatus && OwnerStatus->IsStunned());
	bool bIsAgitated = (EnemyChar && EnemyChar->bIsAgitated);

#if !UE_BUILD_SHIPPING
	// STAŁA TELEMETRIA NA EKRANIE:
	if (GEngine)
	{
		FString StateStr;
		FColor StateColor = FColor::White;
		switch (CurrentState)
		{
		case EAIBehaviorState::Idle_Patrol: StateStr = TEXT("PATROL"); StateColor = FColor::Green; break;
		case EAIBehaviorState::Suspicious: StateStr = TEXT("SUSPICIOUS (Nasłuch / Ryk)"); StateColor = FColor::Orange; break;
		case EAIBehaviorState::Cautious: StateStr = TEXT("CAUTIOUS (Czujny marsz)"); StateColor = FColor::Yellow; break;
		case EAIBehaviorState::Investigate: StateStr = TEXT("INVESTIGATE (Badanie)"); StateColor = FColor::Yellow; break;
		case EAIBehaviorState::Chase_Attack: StateStr = TEXT("CHASE (Pościg!)"); StateColor = FColor::Red; break;
		case EAIBehaviorState::Flee_Panic: StateStr = TEXT("FLEE (Ucieczka!)"); StateColor = FColor::Purple; break;
		}

		FString LoSString = bHasClearLoS ? TEXT("WIDZI (CLEAR)") : TEXT("ZASŁONIĘTY (BLOCKED)");
		FString StunStr = bIsStunned ? TEXT(" | ⚡ [STUNNED]") : TEXT("");
		FString AgitStr = bIsAgitated ? TEXT(" | 💢 [AGITATED]") : TEXT("");

		GEngine->AddOnScreenDebugMessage((uint64)OwnerActor->GetUniqueID() + 100, 0.15f, StateColor,
			FString::Printf(TEXT("👾 [%s] Stan: %s%s%s | Dyst: %.1f m | Wzrok: %.1f m | Widoczność: %.0f%% | LoS: %s"),
				*OwnerActor->GetName(), *StateStr, *StunStr, *AgitStr, DistToPlayer / 100.0f, EffectiveSightRadius / 100.0f, TargetVisibility * 100.0f, *LoSString));
	}
#endif

	if (bIsStunned) return;

	// ====================================================================
	// PRIORYTET 0: UCIECZKA I PANIKA (Pirofobia / Strach przed światłem)
	// ====================================================================
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	
	// Weryfikujemy ogień wyłącznie z żywego układu nerwowego (StatusEffectComponent):
	bool bIsOnFire = (OwnerStatus && OwnerStatus->HasStatusEffect(BurningStatus));

	if (SensoryProfile.bFleesFromFire && bIsOnFire)
	{
		SetAIState(EAIBehaviorState::Flee_Panic, PlayerLocation);
		return;
	}

	if (SensoryProfile.bFleesFromLight && bIsIlluminatedByLantern && DistToPlayer <= 1000.0f)
	{
		SetAIState(EAIBehaviorState::Flee_Panic, PlayerLocation);
		return;
	}

	// Gdy ogień zgaśnie -> natychmiast wychodzi z paniki w stan CAUTIOUS:
	if (CurrentState == EAIBehaviorState::Flee_Panic && !bIsOnFire)
	{
		SetAIState(EAIBehaviorState::Cautious, MyLocation);
		return;
	}

	// ====================================================================
	// PRIORYTET 1: DOTYK I BLISKOŚĆ (Personal Bubble)
	// ====================================================================
	if (SensoryProfile.ProximitySenseRadius > 0.0f && DistToPlayer <= SensoryProfile.ProximitySenseRadius)
	{
		FVector Forward = OwnerActor->GetActorForwardVector();
		FVector DirToPlayer = (PlayerLocation - MyLocation).GetSafeNormal();

		if (FVector::DotProduct(Forward, DirToPlayer) > -0.2f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}
	}

	// ====================================================================
	// PRIORYTET 2: ZMYSŁY KOSMICZNE
	// ====================================================================
	if (SensoryProfile.bHuntsLowSanity && PlayerSanity)
	{
		float MaxSanity = PlayerSanity->GetMaxSanity();
		float SanityPercent = (MaxSanity > 0.0f) ? (PlayerSanity->CurrentSanity / MaxSanity) : 1.0f;

		if (SanityPercent <= SensoryProfile.SanityHuntThreshold && DistToPlayer <= 2500.0f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}
	}

	if (SensoryProfile.bHearsExhaustedBreath && PlayerStamina)
	{
		if (PlayerStamina->bIsFatigued && DistToPlayer <= 1400.0f)
		{
			SetAIState(EAIBehaviorState::Investigate, PlayerLocation);
			return;
		}
	}

	if (SensoryProfile.bTriggeredByDirectGaze)
	{
		if (IsTargetLookingAtMe(PlayerPawn) && DistToPlayer <= 1800.0f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}
	}

	if (SensoryProfile.bFreezesWhenObserved)
	{
		bool bPlayerIsWatching = IsTargetLookingAtMe(PlayerPawn);
		bool bIsLookingInMirror = false;
		FHitResult MirrorHit;
		FCollisionQueryParams MirrorParams;
		MirrorParams.AddIgnoredActor(OwnerActor);

		FVector EyeStart = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f);
		FVector EyeForward = OwnerActor->GetActorForwardVector();
		FVector TraceEnd = EyeStart + (EyeForward * 400.0f);

		if (GetWorld()->LineTraceSingleByChannel(MirrorHit, EyeStart, TraceEnd, ECC_Visibility, MirrorParams))
		{
			if (AActor* HitProp = MirrorHit.GetActor())
			{
				static const FGameplayTag MirrorTag = FGameplayTag::RequestGameplayTag(FName("Surface.Reflective"), false);
				if (UReactionReceiverComponent* Receiver = HitProp->FindComponentByClass<UReactionReceiverComponent>())
				{
					if (Receiver->ActiveStates.HasTag(MirrorTag) || Receiver->VulnerableStates.HasTag(MirrorTag))
					{
						bIsLookingInMirror = true;
					}
				}
			}
		}

		if (bIsLookingInMirror)
		{
			SetAIState(EAIBehaviorState::Suspicious, MirrorHit.ImpactPoint);
			return;
		}

		if (bPlayerIsWatching)
		{
			SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
			return;
		}
	}

	// ====================================================================
	// PRIORYTET 3: WZROK (Z Faza Zauważenia i Oknem Ryku / Telegraph 0.8s)
	// ====================================================================
	
	if (bIsTelegraphingAggro)
	{
		float Elapsed = GetWorld()->GetTimeSeconds() - DetectionTelegraphTimer;
		float RequiredDelay = (SensoryProfile.AggroTelegraphDuration > 0.0f) ? SensoryProfile.AggroTelegraphDuration : 0.8f;

		if (Elapsed >= RequiredDelay)
		{
			bIsTelegraphingAggro = false;
			DetectionBuildUpTimer = 0.0f;
			SetAIState(EAIBehaviorState::Chase_Attack, LastKnownTargetLocation);
			return;
		}
		else
		{
			SetAIState(EAIBehaviorState::Suspicious, LastKnownTargetLocation);
			return;
		}
	}

	if (SensoryProfile.bIsPhototropic && bIsIlluminatedByLantern && DistToPlayer <= 2200.0f)
	{
		SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
		return;
	}

	FVector Forward = OwnerActor->GetActorForwardVector();
	FVector DirToPlayer = (PlayerLocation - MyLocation).GetSafeNormal();
	float Dot = FVector::DotProduct(Forward, DirToPlayer);

	float ActiveConeAngle = (TargetVisibility >= 0.75f || CurrentState == EAIBehaviorState::Cautious) ? 75.0f : SensoryProfile.PeripheralVisionAngle;
	float ConeLimit = FMath::Cos(FMath::DegreesToRadians(ActiveConeAngle));
	bool bInsideCone = (Dot >= ConeLimit || DistToPlayer <= 250.0f);

	bool bInCloseCombat = (CurrentState == EAIBehaviorState::Chase_Attack && DistToPlayer <= 500.0f && bHasClearLoS);

	if ((bHasClearLoS && bInsideCone && DistToPlayer <= EffectiveSightRadius) || bInCloseCombat)
	{
		if (SensoryProfile.bStopsAtLight && bIsInStreetLight)
		{
			SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
			return;
		}

		// A. POINT-BLANK PROVOKE (Włączona latarnia z bliska <= 3.5m -> natychmiastowa szarża):
		if (bIsIlluminatedByLantern && DistToPlayer <= 350.0f)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
			return;
		}

		// B. POŚCIG Z BUFOREM REAKCJI:
		if (CurrentState == EAIBehaviorState::Cautious || CurrentState == EAIBehaviorState::Investigate || TargetVisibility >= 0.70f || DistToPlayer <= 300.0f || bInCloseCombat)
		{
			if (CurrentState != EAIBehaviorState::Chase_Attack)
			{
				float CurrentTime = GetWorld()->GetTimeSeconds();

				// Inicjalizacja bufora zauważenia przy pierwszym ułamku sekundy:
				if (DetectionBuildUpTimer == 0.0f)
				{
					DetectionBuildUpTimer = CurrentTime;
					SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
					return;
				}

				// Wymaga ciągłego świecenia przez co najmniej 0.35 sekundy:
				float TimeIlluminated = CurrentTime - DetectionBuildUpTimer;
				if (TimeIlluminated >= 0.35f)
				{
					// PRZEKROCZONO BUFOR -> ODPALENIE NIEODWOŁALNEGO RYKU I SZARŻY:
					bIsTelegraphingAggro = true;
					DetectionTelegraphTimer = CurrentTime;
					LastKnownTargetLocation = PlayerLocation;

					SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
					return;
				}
				else
				{
					// Poniżej 0.35s: Potwór tylko podejrzliwie nasłuchuje (Suspicious):
					SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
					return;
				}
			}
			else
			{
				SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
				return;
			}
		}
		else if (CurrentState == EAIBehaviorState::Idle_Patrol)
		{
			SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
			return;
		}
	}
	else
	{
		// GRACZ ZGASIŁ LATARNIĘ LUB SCHOWAŁ SIĘ W CIENIU:
		if (DetectionBuildUpTimer > 0.0f && !bIsTelegraphingAggro)
		{
			DetectionBuildUpTimer = 0.0f;
			float CurrentTime = GetWorld()->GetTimeSeconds();

			// 1. Zliczanie krótkich błysków w oknie 6 sekund:
			if ((CurrentTime - LastFlashTimestamp) < 6.0f)
			{
				RecentFlashCount++;
			}
			else
			{
				RecentFlashCount = 1;
			}
			LastFlashTimestamp = CurrentTime;

			// 2. TRZECI BŁYSK W KRÓTKIM CZASIE -> NATYCHMIASTOWA SZARŻA (KONIEC CHEESOWANIA!):
			if (RecentFlashCount >= 3)
			{
				RecentFlashCount = 0;
				bIsTelegraphingAggro = true;
				DetectionTelegraphTimer = CurrentTime;
				LastKnownTargetLocation = PlayerLocation;

#if !UE_BUILD_SHIPPING
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage((uint64)OwnerActor->GetUniqueID() + 950, 2.5f, FColor::Red,
						TEXT("🚨 [AI WZROK] Trzeci rozbłysk latarki! Potwór rozszyfrował gracza i szarżuje!"));
				}
#endif
				SetAIState(EAIBehaviorState::Suspicious, PlayerLocation);
				return;
			}

			// 1 lub 2 błysk -> potwór podejrzliwie idzie zbadać punkt rozbłysku (Investigate):
			if (CurrentState == EAIBehaviorState::Suspicious)
			{
				SetAIState(EAIBehaviorState::Investigate, PlayerLocation);
			}
		}
	}

	// ====================================================================
	// PRIORYTET 4: WERYFIKACJA UTRATY POŚCIGU (Pamięć 3.5s -> Suspicious -> Cautious)
	// ====================================================================
	if (CurrentState == EAIBehaviorState::Chase_Attack)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();

		if (ChaseLostTime == 0.0f)
		{
			ChaseLostTime = CurrentTime;
			LastKnownTargetLocation = PlayerLocation;
		}

		if ((CurrentTime - ChaseLostTime) < ChaseMemoryDuration)
		{
			SetAIState(EAIBehaviorState::Chase_Attack, LastKnownTargetLocation);
			return;
		}

		ChaseLostTime = 0.0f;
		// Po dobiegnięciu do miejsca zniknięcia staje i nasłuchuje (Suspicious na 2.5s):
		SetAIState(EAIBehaviorState::Suspicious, LastKnownTargetLocation);
		return;
	}
	else
	{
		ChaseLostTime = 0.0f;
	}

	// ====================================================================
	// PRIORYTET 5: WĘCH (Ślady Krwi)
	// ====================================================================
	if (CurrentState != EAIBehaviorState::Chase_Attack && CurrentState != EAIBehaviorState::Flee_Panic)
	{
		if (UImSimSensorySubsystem* SensorySubsystem = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			float FreshestScentTime = 0.0f;
			FVector BestScentLocation = FVector::ZeroVector;
			bool bFoundFreshScent = false;

			for (const FImSimStimulusEvent& Stimulus : SensorySubsystem->GetActiveStimuli())
			{
				if (SensoryProfile.TrackedScents.HasTag(Stimulus.StimulusTag))
				{
					static const FGameplayTag ScentMaskBuff = FGameplayTag::RequestGameplayTag(FName("Status.Buff.ScentMask"), false);
					if (PlayerStatus && PlayerStatus->HasStatusEffect(ScentMaskBuff)) continue;

					float DistToStimulus = FVector::Dist(MyLocation, Stimulus.Location);

					if (DistToStimulus > 120.0f && DistToStimulus <= SensoryProfile.ScentTrackingRadius)
					{
						if (Stimulus.ExpirationTime > FreshestScentTime)
						{
							FreshestScentTime = Stimulus.ExpirationTime;
							BestScentLocation = Stimulus.Location;
							bFoundFreshScent = true;
						}
					}
				}
			}

			if (bFoundFreshScent)
			{
				SetAIState(EAIBehaviorState::Investigate, BestScentLocation);
				return;
			}
		}
	}

	// ====================================================================
	// PRIORYTET 6: SŁUCH (Akustyka z Trajectory Back-Tracing)
	// ====================================================================
	if (CurrentState != EAIBehaviorState::Chase_Attack && CurrentState != EAIBehaviorState::Flee_Panic)
	{
		if (EnemyChar && EnemyChar->bIsAgitated)
		{
			return; // Jeśli dostał cios -> idzie do źródła ataku
		}

		if (UImSimSensorySubsystem* SensorySubsystem = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			float CurrentTime = GetWorld()->GetTimeSeconds();

			static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
			bool bIsPlayerSprinting = PlayerPawn && (PlayerPawn->GetVelocity().Size2D() > 400.0f);

			for (const FImSimStimulusEvent& Stimulus : SensorySubsystem->GetActiveStimuli())
			{
				if (Stimulus.StimulusTag.MatchesTag(NoiseTag) && SensoryProfile.HearingSensitivity > 0.0f)
				{
					float DistToStimulus = FVector::Dist(MyLocation, Stimulus.Location);

					float Multiplier = (CurrentState == EAIBehaviorState::Suspicious || CurrentState == EAIBehaviorState::Cautious) ? 1.5f : 1.0f;
					float EffectiveHearingRadius = Stimulus.IntensityRadius * SensoryProfile.HearingSensitivity * Multiplier;
					if (!CheckAcousticOcclusion(Stimulus.Location)) EffectiveHearingRadius *= 0.5f;

					if (DistToStimulus <= EffectiveHearingRadius)
					{
						float DistFromSoundToPlayer = PlayerPawn ? FVector::Dist(PlayerPawn->GetActorLocation(), Stimulus.Location) : 9999.0f;

						// ==============================================================
						// A. ANTY-BAITING (TYLKO DLA KROKÓW SPRINTU GRACZA < 2.0m od stóp):
						// ==============================================================
						if (bIsPlayerSprinting && DistFromSoundToPlayer <= 200.0f)
						{
							if (EnemyChar)
							{
								if ((CurrentTime - EnemyChar->LastDistractionTimestamp) < 8.0f)
								{
									EnemyChar->ConsecutiveNoiseDistractions++;
								}
								else
								{
									EnemyChar->ConsecutiveNoiseDistractions = 1;
								}
								EnemyChar->LastDistractionTimestamp = CurrentTime;

								// WYMAGA AŻ 4 SPRINTÓW Z RZĘDU W ODSTĘPIE 8 SEKUND:
								if (EnemyChar->ConsecutiveNoiseDistractions >= 4)
								{
									EnemyChar->ConsecutiveNoiseDistractions = 0;
#if !UE_BUILD_SHIPPING
									if (GEngine)
									{
										GEngine->AddOnScreenDebugMessage((uint64)OwnerActor->GetUniqueID() + 900, 2.5f, FColor::Red,
											TEXT("🎯 [AKUSTYKA] Wykryto ciągły sprint! Potwór szarżuje!"));
									}
#endif
									SetAIState(EAIBehaviorState::Chase_Attack, PlayerLocation);
									return;
								}
							}
						}

						// ==============================================================
						// B. RZUCANE PRZEDMIOTY (PUSZKI / BUTELKI):
						// ==============================================================
						FVector TargetInvestigatePoint = Stimulus.Location;

						// Jeśli przedmiot rozbił się tuż obok potwora (< 2.5m) -> bada skąd przyleciał:
						if (DistToStimulus < 250.0f && PlayerPawn)
						{
							FVector DirFromPlayer = (Stimulus.Location - PlayerLocation).GetSafeNormal();
							TargetInvestigatePoint = Stimulus.Location - (DirFromPlayer * 500.0f);
						}

						// Cichy hałas z daleka -> Suspicious, głośny -> Investigate:
						if (Stimulus.IntensityRadius < 300.0f && DistToStimulus > 600.0f && CurrentState == EAIBehaviorState::Idle_Patrol)
						{
							SetAIState(EAIBehaviorState::Suspicious, TargetInvestigatePoint);
							return;
						}

						// POTWÓR SPOKOJNIE IDZIE ZBADAĆ HAŁAS PUSZKI:
						SetAIState(EAIBehaviorState::Investigate, TargetInvestigatePoint);
						return;
					}
				}
			}
		}
	}
}

bool USensoryComponent::CheckLineOfSightToTarget(AActor* TargetActor) const
{
	if (!TargetActor) return false;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);

	// 1. Ignorujemy broń podpiętą pod potwora:
	TArray<AActor*> AttachedActors;
	OwnerActor->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		Params.AddIgnoredActor(Attached);
	}

	// 2. Ignorujemy fizyczny rekwizyt trzymany w dłoniach gracza:
	if (UInteractionComponent* InterComp = TargetActor->FindComponentByClass<UInteractionComponent>())
	{
		if (InterComp->GrabbedActor)
		{
			Params.AddIgnoredActor(InterComp->GrabbedActor);
		}
	}

	FVector Start = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f); // Oczy potwora
	FVector End = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);   // Klatka piersiowa gracza

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) return false;

		// Jeśli promień bezpośrednio trafił w gracza, jego latarnię lub ubranie:
		if (HitActor == TargetActor || HitActor->GetAttachParentActor() == TargetActor || HitActor->GetOwner() == TargetActor)
		{
			return true;
		}

		// ====================================================================
		// DYNAMICZNA OPTYKA SZKŁA DLA AI (Skalowana parametrem MaterialPurity):
		// ====================================================================
		static const FGameplayTag GlassTag = FGameplayTag::RequestGameplayTag(FName("Material.Glass"), false);
		bool bIsGlass = HitActor->ActorHasTag(FName("Material.Glass"));

		if (!bIsGlass && HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			bIsGlass = IPhysicalInteract::Execute_GetMaterialTag(HitActor).MatchesTag(GlassTag);
		}

		if (bIsGlass)
		{
			// Odczytujemy czystość szkła (1.0 = czyste, 0.1 = zaparowane/brudne):
			float GlassPurity = 1.0f;
			if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
			{
				GlassPurity = IPhysicalInteract::Execute_GetMaterialPurity(HitActor);
			}

			// Rzucamy drugi promień ignorujący szybę:
			Params.AddIgnoredActor(HitActor);
			FHitResult HitBehindGlass;

			if (GetWorld()->LineTraceSingleByChannel(HitBehindGlass, Start, End, ECC_Visibility, Params))
			{
				AActor* TargetBehindGlass = HitBehindGlass.GetActor();
				if (TargetBehindGlass == TargetActor || TargetBehindGlass->GetAttachParentActor() == TargetActor)
				{
					// BRUDNE / ZAMGLONE SZKŁO: Potwór widzi gracza tylko przy zapalonej latarce LUB z bliska:
					ULanternComponent* Lantern = TargetActor->FindComponentByClass<ULanternComponent>();
					bool bIsLit = Lantern && Lantern->bIsLit;
					float DistToPlayer = FVector::Dist(Start, End);

					float MaxSightThroughDirtyGlass = 1500.0f * GlassPurity;

					if (!bIsLit && DistToPlayer > MaxSightThroughDirtyGlass)
					{
						return false; // Brudne szkło chroni gracza w cieniu!
					}

					return true; // Potwór widzi gracza przez okno!
				}
			}
			else
			{
				return true;
			}
		}

#if !UE_BUILD_SHIPPING
		if (GEngine && FVector::Dist(Start, End) < 600.0f)
		{
			GEngine->AddOnScreenDebugMessage((uint64)OwnerActor->GetUniqueID() + 800, 0.1f, FColor::Orange,
				FString::Printf(TEXT("🚫 [WZROK ZABLOKOWANY PRZEZ]: %s"), *HitActor->GetName()));
		}
#endif
		return false; // Ściana/szafa zasłania cel
	}

	return true; // Czysta przestrzeń bez przeszkód
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

bool USensoryComponent::IsTargetLookingAtMe(APawn* TargetPawn) const
{
	if (!TargetPawn) return false;

	FVector CameraLocation;
	FRotator CameraRotation;
	TargetPawn->GetActorEyesViewPoint(CameraLocation, CameraRotation);

	FVector DirToMe = (GetOwner()->GetActorLocation() - CameraLocation).GetSafeNormal();
	float Dot = FVector::DotProduct(CameraRotation.Vector(), DirToMe);

	return (Dot > 0.75f && CheckLineOfSightToTarget(TargetPawn));
}

void USensoryComponent::SetAIState(EAIBehaviorState NewState, FVector TargetLocation)
{
	bool bIsNewLocation = FVector::DistSquared(TargetLocation, FVector::ZeroVector) > 10.0f;

	if (CurrentState != NewState || (NewState == EAIBehaviorState::Investigate && bIsNewLocation))
	{
		CurrentState = NewState;

		if (NewState == EAIBehaviorState::Idle_Patrol)
		{
			RecentFlashCount = 0;
			DetectionBuildUpTimer = 0.0f;
			bIsTelegraphingAggro = false;
			ChaseLostTime = 0.0f;
			LastKnownTargetLocation = FVector::ZeroVector;
		}

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
			case EAIBehaviorState::Cautious: StateStr = TEXT("CAUTIOUS (Czujny marsz)"); StateColor = FColor::Yellow; break;
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