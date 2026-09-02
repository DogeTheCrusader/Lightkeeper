#include "ReactionReceiverComponent.h"
#include "HealthComponent.h"
#include "StatusEffectComponent.h"
#include "BaseInteractable.h"
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

void UReactionReceiverComponent::ApplyStateImpact(FGameplayTag IncomingState, float Intensity)
{
	if (!IncomingState.IsValid()) return;

	ABaseInteractable* OwnerInteractable = Cast<ABaseInteractable>(GetOwner());
	FGameplayTag OwnerMaterial = OwnerInteractable ? OwnerInteractable->MaterialTag : FGameplayTag();

	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"), false);
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	static const FGameplayTag WetStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Neutral.Wet"), false);
	static const FGameplayTag WoodTag = FGameplayTag::RequestGameplayTag(FName("Material.Wood"), false);
	static const FGameplayTag TagFlesh = FGameplayTag::RequestGameplayTag(FName("Material.Flesh"), false);

	// 1. Sprawdzamy reguły reakcji:
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
			return;
		}

		if (CachedStatusComp || OwnerMaterial.MatchesTag(WoodTag) || OwnerMaterial.MatchesTag(TagFlesh) || IsVulnerableTo(FireTag))
		{
			ActiveStates.AddTag(BurningStatus);
			OnStateApplied.Broadcast(BurningStatus, Intensity);

			if (CachedStatusComp)
			{
				CachedStatusComp->ApplyStatusEffectFromTable(BurningStatus);
			}

			CheckAndManageDoTTimer();
			return;
		}
	}

	// 3. Fallback:
	if (IsVulnerableTo(IncomingState))
	{
		ActiveStates.AddTag(IncomingState);
		OnStateApplied.Broadcast(IncomingState, Intensity);
		CheckAndManageDoTTimer();
	}
}

void UReactionReceiverComponent::RemoveState(FGameplayTag StateTag)
{
	if (ActiveStates.HasTag(StateTag))
	{
		ActiveStates.RemoveTag(StateTag);
		StateExposureTimers.Remove(StateTag);
		OnStateRemoved.Broadcast(StateTag);
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

	// Timer w ReactionReceiver działa TYLKO dla obiektów bez StatusEffectComponent (np. płonąca drewniana skrzynia w świecie):
	if (bNeedsTimer && !CachedStatusComp && CachedHealthComp)
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
	if (!CachedHealthComp || CachedHealthComp->IsDead())
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

		// Samoczynne wygaszenie ognia dla niezniszczalnych ścian (np. po 8s):
		if (ActiveTag.MatchesTag(BurningStatus) && AutoExtinguishDuration > 0.0f && TimeExposed >= AutoExtinguishDuration)
		{
			StatesToAutoRemove.Add(ActiveTag);
		}

		// Obrażenia zadawane obiektom ze wspólnej tabeli DT_StatusEffects:
		if (const FStatusEffectDataRow* Row = FindStatusEffectRow(ActiveTag))
		{
			if (Row->DamagePerTick > 0.0f)
			{
				CachedHealthComp->TakeDamage(Row->DamagePerTick, ActiveTag);
			}
		}
	}

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