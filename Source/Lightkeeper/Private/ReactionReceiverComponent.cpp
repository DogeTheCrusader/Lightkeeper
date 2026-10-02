#include "ReactionReceiverComponent.h"
#include "HealthComponent.h"
#include "SafeLightComponent.h"
#include "StatusEffectComponent.h"
#include "BaseInteractable.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UReactionReceiverComponent::UReactionReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UDataTable> StatusTableFinder(TEXT("/Game/Lightkeeper/Technical/DT_StatusEffects"));
	if (StatusTableFinder.Succeeded())
	{
		StatusEffectsDataTable = StatusTableFinder.Object;
	}
}

void UReactionReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeReactionRules();
	StateExposureTimers.Empty();

	if (AActor* Owner = GetOwner())
	{
		CachedHealthComp = Owner->FindComponentByClass<UHealthComponent>();
		CachedStatusComp = Owner->FindComponentByClass<UStatusEffectComponent>();
	}
}

void UReactionReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DoTTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

const FStatusEffectDataRow* UReactionReceiverComponent::FindStatusEffectRow(const FGameplayTag& StatusTag) const
{
	if (!StatusEffectsDataTable || !StatusTag.IsValid()) return nullptr;

	const FName RowName = StatusTag.GetTagName();
	static const FString ContextString(TEXT("ReactionReceiverStatusLookup"));

	if (const FStatusEffectDataRow* FastRow = StatusEffectsDataTable->FindRow<FStatusEffectDataRow>(RowName, ContextString))
	{
		return FastRow;
	}

	TArray<FStatusEffectDataRow*> AllRows;
	StatusEffectsDataTable->GetAllRows<FStatusEffectDataRow>(ContextString, AllRows);

	for (const FStatusEffectDataRow* Row : AllRows)
	{
		if (Row && StatusTag.MatchesTag(Row->StatusTag))
		{
			return Row;
		}
	}

	return nullptr;
}

void UReactionReceiverComponent::InitializeReactionRules()
{
	ReactionRules.Empty();

	FGameplayTag TagWater = FGameplayTag::RequestGameplayTag(FName("State.Element.Moisture.Water"));
	FGameplayTag TagFire = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"));
	FGameplayTag TagElectricity = FGameplayTag::RequestGameplayTag(FName("State.Element.Electricity.Current"));
	FGameplayTag TagAcid = FGameplayTag::RequestGameplayTag(FName("State.Element.Moisture.Acid"));

	FGameplayTag TagBurning = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"));
	FGameplayTag TagWet = FGameplayTag::RequestGameplayTag(FName("Status.State.Neutral.Wet"));
	FGameplayTag TagOiled = FGameplayTag::RequestGameplayTag(FName("Status.State.Neutral.Oiled"));
	FGameplayTag TagElectrocuted = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"));
	FGameplayTag TagCorroding = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Corroding"));

	FGameplayTag TagMetal = FGameplayTag::RequestGameplayTag(FName("Material.Metal"));
	FGameplayTag TagFlesh = FGameplayTag::RequestGameplayTag(FName("Material.Flesh"));

	// 1. Woda gasi ogień:
	ReactionRules.Add({ TagWater, TagBurning, TagBurning, TagWet, FGameplayTag(), false });

	// 2. Ogień suszy wodę:
	ReactionRules.Add({ TagFire, TagWet, TagWet, FGameplayTag(), FGameplayTag(), false });

	// 3. Ogień + Olej = Detonacja Synergiczna!
	ReactionRules.Add({ TagFire, TagOiled, TagOiled, TagBurning, FGameplayTag(), true });

	// 4. Prąd ZAWSZE poraża Ciało (Flesh) i postacie:
	ReactionRules.Add({ TagElectricity, FGameplayTag(), FGameplayTag(), TagElectrocuted, TagFlesh, false });

	// 5. Prąd naelektryzowuje mokry obiekt:
	ReactionRules.Add({ TagElectricity, TagWet, FGameplayTag(), TagElectrocuted, FGameplayTag(), false });

	// 6. Prąd przewodzi przez metal:
	ReactionRules.Add({ TagElectricity, FGameplayTag(), FGameplayTag(), TagElectrocuted, TagMetal, false });

	// 7. Kwas trawi metal i ciało:
	ReactionRules.Add({ TagAcid, FGameplayTag(), FGameplayTag(), TagCorroding, TagMetal, false });
	ReactionRules.Add({ TagAcid, FGameplayTag(), FGameplayTag(), TagCorroding, TagFlesh, false });
}

bool UReactionReceiverComponent::EvaluateReactionMatrix(const FGameplayTag& IncomingState, float Intensity, const FGameplayTag& OwnerMaterial)
{
	ABaseInteractable* OwnerInteractable = Cast<ABaseInteractable>(GetOwner());

	static const FGameplayTag TagElectrocuted = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
	static const FGameplayTag TagElectricity = FGameplayTag::RequestGameplayTag(FName("State.Element.Electricity"), false);

	// ====================================================================
	// 1. GWARANTOWANE PORAŻENIE PRĄDEM DLA POSTACI:
	// Przekazujemy do StatusEffectComponent (Zczytuje Stun i DoT z DT_StatusEffects!)
	// ====================================================================
	if (IncomingState.MatchesTag(TagElectricity) && CachedStatusComp)
	{
		ActiveStates.AddTag(TagElectrocuted);
		OnStateApplied.Broadcast(TagElectrocuted, Intensity);

		CachedStatusComp->ApplyStatusEffectFromTable(TagElectrocuted);

		if (CachedHealthComp)
		{
			static const FGameplayTag ArrhythmiaTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Arrhythmia"), false);
			CachedHealthComp->AddInjury(ArrhythmiaTag, EAnatomicalLimb::Chest);
		}

		CheckAndManageDoTTimer();
		return true;
	}

	// ====================================================================
	// 2. REGUŁY REAKCJI CHEMICZNYCH:
	// ====================================================================
	for (const FChemicalReactionRule& Rule : ReactionRules)
	{
		if (IncomingState.MatchesTag(Rule.IncomingElement))
		{
			bool bStateMatch = !Rule.RequiredActiveState.IsValid() || HasState(Rule.RequiredActiveState);
			bool bMaterialMatch = !Rule.RequiredMaterial.IsValid() || (OwnerMaterial.IsValid() && OwnerMaterial.MatchesTag(Rule.RequiredMaterial));

			if (bStateMatch && bMaterialMatch)
			{
				if (Rule.StateToRemove.IsValid())
				{
					RemoveState(Rule.StateToRemove);
				}

				if (Rule.StateToAdd.IsValid())
				{
					ActiveStates.AddTag(Rule.StateToAdd);
					OnStateApplied.Broadcast(Rule.StateToAdd, Intensity);

					// Przekazujemy nowy stan do StatusEffectComponent (jeśli obiekt to postać/potwór):
					if (CachedStatusComp)
					{
						CachedStatusComp->ApplyStatusEffectFromTable(Rule.StateToAdd);
					}
				}

				if (Rule.bTriggerEmission && OwnerInteractable)
				{
					OwnerInteractable->TriggerStateEmission(); // Detonacja oleju!
				}

				CheckAndManageDoTTimer();
				return true;
			}
		}
	}

	return false;
}

float UReactionReceiverComponent::GetCalculatedStateDuration(const FGameplayTag& StateTag, float Intensity) const
{
	// 1. Priorytet: Ręczne nadpisanie z mapy Details tego konkretnego obiektu:
	for (const TPair<FGameplayTag, float>& OverridePair : CustomStateDurationOverrides)
	{
		if (StateTag.MatchesTag(OverridePair.Key) && OverridePair.Value > 0.0f)
		{
			return OverridePair.Value;
		}
	}

	// 2. Pobieramy bazę z tabeli DT_StatusEffects:
	float BaseDuration = 8.0f;
	if (const FStatusEffectDataRow* Row = FindStatusEffectRow(StateTag))
	{
		BaseDuration = Row->DefaultDuration;
	}

	// 3. Mnożnik siły (Intensity):
	float ClampedIntensity = FMath::Clamp(Intensity, 0.5f, 2.0f);

	// 4. Bonus fizyczny za masę (TYLKO dla ognia na drewnie):
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	float MassBonus = 0.0f;
	if (StateTag.MatchesTag(BurningStatus))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnerActor->GetComponentByClass(UPrimitiveComponent::StaticClass())))
			{
				if (PrimComp->IsSimulatingPhysics())
				{
					MassBonus = PrimComp->GetMass() * 0.15f;
				}
			}
		}
	}

	return FMath::Clamp((BaseDuration * ClampedIntensity) + MassBonus, 3.0f, 60.0f);
}

void UReactionReceiverComponent::ApplyStateImpact(FGameplayTag IncomingState, float Intensity)
{
	if (!IncomingState.IsValid()) return;

	ABaseInteractable* OwnerInteractable = Cast<ABaseInteractable>(GetOwner());
	FGameplayTag OwnerMaterial = OwnerInteractable ? OwnerInteractable->MaterialTag : FGameplayTag();

	static const FGameplayTag WaterTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Moisture.Water"), false);
	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"), false);
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	static const FGameplayTag WetStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Neutral.Wet"), false);
	static const FGameplayTag WoodTag = FGameplayTag::RequestGameplayTag(FName("Material.Wood"), false);
	static const FGameplayTag TagFlesh = FGameplayTag::RequestGameplayTag(FName("Material.Flesh"), false);

	// ====================================================================
	// SPECJALNA OBSŁUGA WODY (Gaszenie z poszanowaniem latarni w szkle!):
	// ====================================================================
	if (IncomingState.MatchesTag(WaterTag))
	{
		bool bExtinguishedAnyLight = false;

		// Szukamy źródła światła i sprawdzamy, czy woda może je zgasić (świeca tak, latarnia gazowa nie):
		if (USafeLightComponent* SafeLight = GetOwner()->FindComponentByClass<USafeLightComponent>())
		{
			if (SafeLight->bCanBeExtinguishedByWater)
			{
				SafeLight->SetLightActive(false); // Gasi żarówkę i odłącza chemię
				bExtinguishedAnyLight = true;
			}
		}

		// Zdejmujemy ogień ze zwykłych skrzynek LUB ugaszonych świeczek:
		if (HasState(BurningStatus) && (!GetOwner()->FindComponentByClass<USafeLightComponent>() || bExtinguishedAnyLight))
		{
			RemoveState(BurningStatus);
		}

		ActiveStates.AddTag(WetStatus);
		OnStateApplied.Broadcast(WetStatus, Intensity);
		CheckAndManageDoTTimer();
		return; // Woda całkowicie obsłużona!
	}

	// 1. Sprawdzamy reguły macierzy reakcji:
	if (EvaluateReactionMatrix(IncomingState, Intensity, OwnerMaterial))
	{
		return;
	}

	// 2. Podpalenie Drewna, Ciała i Postaci:
	if (IncomingState.MatchesTag(FireTag))
	{
		if (HasState(WetStatus))
		{
			RemoveState(WetStatus);
			return; // Woda gasi/chroni przed pierwszym ogniem
		}

		// Sprawdzamy, czy obiekt już ma własne wbudowane źródło światła:
		bool bIsLightSource = GetOwner() && GetOwner()->FindComponentByClass<USafeLightComponent>() != nullptr;

		if (CachedStatusComp || OwnerMaterial.MatchesTag(WoodTag) || OwnerMaterial.MatchesTag(TagFlesh) || bIsLightSource || IsVulnerableTo(FireTag))
		{
			ActiveStates.AddTag(BurningStatus);
			OnStateApplied.Broadcast(BurningStatus, Intensity);

			if (CachedStatusComp)
			{
				CachedStatusComp->ApplyStatusEffectFromTable(BurningStatus);
			}

			// SPAWNOWANIE BLUEPRINTOWEGO POŻARU:
			if (!bIsLightSource && !CachedStatusComp && !ActiveFireHazardActor)
			{
				if (AActor* OwnerActor = GetOwner())
				{
					UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnerActor->GetComponentByClass(UPrimitiveComponent::StaticClass()));

					FBoxSphereBounds WorldBounds = PrimComp ? PrimComp->Bounds : OwnerActor->GetRootComponent()->Bounds;
					FVector BoxExtent = WorldBounds.BoxExtent;
					FVector CenterOrigin = WorldBounds.Origin;
					float SphereRadius = WorldBounds.SphereRadius;

					// 1. UNIWERSALNE WYLICZENIE CZASU:
					float ScaledBurnDuration = GetCalculatedStateDuration(BurningStatus, Intensity);

					// 2. TWOJE SPRAWDZONE PARAMETRY ŚWIATŁA (Bez zmian):
					float ClampedIntensity = FMath::Clamp(Intensity, 0.5f, 2.0f);
					float ScaledRadius = FMath::Clamp(SphereRadius * 3.5f, 380.0f, 550.0f);
					float ScaledLightIntensity = FMath::Clamp(4000.0f + (SphereRadius * 15.0f * ClampedIntensity), 4500.0f, 6500.0f);

					// 3. INTELIGENTNE POZYCJONOWANIE (Drzwi/Okna vs Skrzynki):
					float MinHorizontalExtent = FMath::Min(BoxExtent.X, BoxExtent.Y);
					bool bIsVerticalSurface = (BoxExtent.Z > 75.0f) && (BoxExtent.Z > MinHorizontalExtent * 1.5f);

					FVector StartLocation;

					if (bIsVerticalSurface)
					{
						FVector DoorCenter = CenterOrigin;
						APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
						FVector ToPlayerDir = PlayerPawn ? (PlayerPawn->GetActorLocation() - CenterOrigin).GetSafeNormal() : OwnerActor->GetActorForwardVector();

						FVector SurfaceNormal = OwnerActor->GetActorForwardVector();
						if (FMath::Abs(FVector::DotProduct(SurfaceNormal, ToPlayerDir)) < 0.3f)
						{
							SurfaceNormal = OwnerActor->GetActorRightVector();
						}

						if (FVector::DotProduct(SurfaceNormal, ToPlayerDir) < 0.0f)
						{
							SurfaceNormal = -SurfaceNormal;
						}

						StartLocation = DoorCenter + (SurfaceNormal * (MinHorizontalExtent + 15.0f));
					}
					else
					{
						StartLocation = CenterOrigin + FVector(0.0f, 0.0f, BoxExtent.Z + 35.0f);
					}

					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = OwnerActor;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					TSubclassOf<AFireHazardActor> ClassToSpawn = FireHazardClass ? FireHazardClass : TSubclassOf<AFireHazardActor>(AFireHazardActor::StaticClass());
					ActiveFireHazardActor = GetWorld()->SpawnActor<AFireHazardActor>(ClassToSpawn, StartLocation, FRotator::ZeroRotator, SpawnParams);

					if (ActiveFireHazardActor)
					{
						ActiveFireHazardActor->InitializeHazard(OwnerActor, ScaledRadius, ScaledLightIntensity, ScaledBurnDuration, bIsVerticalSurface);
					}

					AutoExtinguishDuration = ScaledBurnDuration;

#if !UE_BUILD_SHIPPING
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Orange,
							FString::Printf(TEXT("🔥 [POŻAR] Czas wyliczony dla obiektu: %.1fs (Intensity: %.1fx) | Pionowy: %s"),
								ScaledBurnDuration, ClampedIntensity, bIsVerticalSurface ? TEXT("TAK (Drzwi/Okno)") : TEXT("NIE (Skrzynia)")));
					}
#endif
				}
			}

			CheckAndManageDoTTimer();
			return;
		}
	}

	// 3. Obsługa pozostałych stanów podatności (Fallback):
	if (IsVulnerableTo(IncomingState))
	{
		ActiveStates.AddTag(IncomingState);
		OnStateApplied.Broadcast(IncomingState, Intensity);
		CheckAndManageDoTTimer();
	}
}

void UReactionReceiverComponent::RemoveState(FGameplayTag StateTag)
{
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);

	if (ActiveStates.HasTag(StateTag))
	{
		ActiveStates.RemoveTag(StateTag);
		StateExposureTimers.Remove(StateTag);
		OnStateRemoved.Broadcast(StateTag);

		if (StateTag.MatchesTag(BurningStatus))
		{
			// Jeśli ogień został ugaszony przed czasem (np. woda), niszczymy aktora natychmiast:
			if (ActiveFireHazardActor)
			{
				ActiveFireHazardActor->Destroy();
				ActiveFireHazardActor = nullptr;

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Silver, TEXT("💨 [OGIEŃ ZGASŁ] Usunięto Blueprint FireHazard!"));
#endif
			}
		}

		CheckAndManageDoTTimer();
	}
}

bool UReactionReceiverComponent::IsVulnerableTo(const FGameplayTag& StateTag) const
{
	for (const FGameplayTag& VulnTag : VulnerableStates)
	{
		if (StateTag.MatchesTag(VulnTag)) return true;
	}
	return false;
}

void UReactionReceiverComponent::CheckAndManageDoTTimer()
{
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	static const FGameplayTag CorrodingStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Corroding"), false);
	static const FGameplayTag ElectrocutedStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);

	bool bNeedsTimer = HasState(BurningStatus) || HasState(CorrodingStatus) || HasState(ElectrocutedStatus);

	// Timer ma działać dla obiektów bez StatusEffectComponent (dla ścian, skrzynek, podłóg!):
	if (bNeedsTimer && !CachedStatusComp)
	{
		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(DoTTimerHandle))
			{
				World->GetTimerManager().SetTimer(DoTTimerHandle, this, &UReactionReceiverComponent::ProcessDoTTick, 1.0f, true);
			}
		}
	}
	else
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DoTTimerHandle);
		}
	}
}

void UReactionReceiverComponent::ProcessDoTTick()
{
	// Jeśli obiekt miał HP i umarł, zatrzymujemy timer:
	if (CachedHealthComp && CachedHealthComp->IsDead())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DoTTimerHandle);
		}
		return;
	}

	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	TArray<FGameplayTag> StatesToAutoRemove;

	for (const FGameplayTag& ActiveTag : ActiveStates)
	{
		float& TimeExposed = StateExposureTimers.FindOrAdd(ActiveTag);
		TimeExposed += 1.0f;

		// POBIERAMY CZAS TRWANIA DLA KAŻDEGO STANU (Ogień, Woda, Olej, Kwas itp.):
		float MaxAllowedDuration = GetCalculatedStateDuration(ActiveTag, 1.0f);

		// Samoczynne wygaszenie danego stanu po jego własnym czasie:
		if (MaxAllowedDuration > 0.0f && TimeExposed >= MaxAllowedDuration)
		{
			StatesToAutoRemove.Add(ActiveTag);
		}

		// Obrażenia zadawane tylko obiektom z komponentem zdrowia:
		if (CachedHealthComp)
		{
			if (const FStatusEffectDataRow* Row = FindStatusEffectRow(ActiveTag))
			{
				if (Row->DamagePerTick > 0.0f)
				{
					CachedHealthComp->TakeDamage(Row->DamagePerTick, ActiveTag);
				}
			}
		}
	}

	// Usuwamy wygaszone stany ze świata:
	for (const FGameplayTag& ExpiredTag : StatesToAutoRemove)
	{
		RemoveState(ExpiredTag);
	}

	// Propagacja ognia przez emiter wybuchu/strefy mebla:
	if (HasState(BurningStatus))
	{
		if (ABaseInteractable* InterProp = Cast<ABaseInteractable>(GetOwner()))
		{
			InterProp->TriggerStateEmission();
		}
	}
}

bool UReactionReceiverComponent::GetFirstActiveHazard(FGameplayTag& OutHazardState) const
{
	static const FGameplayTag HazardParentTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard"), false);

	for (const FGameplayTag& State : ActiveStates)
	{
		// MatchesTag sprawdza całą gałąź: Burning, Electrocuted, Corroding, Scalding itp.!
		if (State.MatchesTag(HazardParentTag))
		{
			OutHazardState = State;
			return true;
		}
	}
	return false;
}