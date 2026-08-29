#include "ReactionReceiverComponent.h"
#include "HealthComponent.h"
#include "StatusEffectComponent.h"
#include "BaseInteractable.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UReactionReceiverComponent::UReactionReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UReactionReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeReactionRules();
	StateExposureTimers.Empty();

	// ZNAJDUJEMY KOMPONENTY DOKŁADNIE RAZ PRZY STARCIE:
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

	// ====================================================================
	// AUTONOMICZNA TABLICA REGUŁ CHEMICZNYCH:
	// ====================================================================
	// 1. Woda gasi ogień:
	ReactionRules.Add({ TagWater, TagBurning, TagBurning, TagWet, FGameplayTag(), false });

	// 2. Ogień suszy wodę:
	ReactionRules.Add({ TagFire, TagWet, TagWet, FGameplayTag(), FGameplayTag(), false });

	// 3. Ogień + Olej = Detonacja Synergiczna!
	ReactionRules.Add({ TagFire, TagOiled, TagOiled, TagBurning, FGameplayTag(), true });

	// 4. Prąd ZAWSZE poraża Ciało (Flesh) i postacie:
	ReactionRules.Add({ TagElectricity, FGameplayTag(), FGameplayTag(), TagElectrocuted, TagFlesh, false });

	// 5. Prąd naelektryzowuje mokry obiekt (dowolny materiał):
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
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

	static const FGameplayTag TagElectrocuted = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
	static const FGameplayTag TagElectricity = FGameplayTag::RequestGameplayTag(FName("State.Element.Electricity"), false);

	// GWARANTOWANE PORAŻENIE DLA DOWOLNEJ POSTACI (GRACZ / POTWORY):
	if (IncomingState.MatchesTag(TagElectricity) && OwnerChar)
	{
		ActiveStates.AddTag(TagElectrocuted);
		OnStateApplied.Broadcast(TagElectrocuted, Intensity);

		if (UStatusEffectComponent* StatusComp = OwnerChar->FindComponentByClass<UStatusEffectComponent>())
		{
			StatusComp->ApplyStun(0.45f); // Mikro-stun!
		}

		if (UHealthComponent* HealthComp = OwnerChar->FindComponentByClass<UHealthComponent>())
		{
			static const FGameplayTag ArrhythmiaTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Arrhythmia"), false);
			HealthComp->AddInjury(ArrhythmiaTag, EAnatomicalLimb::Chest);
		}

		CheckAndManageDoTTimer();
		return true;
	}

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

					if (Rule.StateToAdd.MatchesTag(TagElectrocuted))
					{
						if (UStatusEffectComponent* StatusComp = GetOwner()->FindComponentByClass<UStatusEffectComponent>())
						{
							StatusComp->ApplyStun(0.45f);
						}
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
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
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

		if (OwnerChar || OwnerMaterial.MatchesTag(WoodTag) || OwnerMaterial.MatchesTag(TagFlesh) || IsVulnerableTo(FireTag))
		{
			ActiveStates.AddTag(BurningStatus);
			OnStateApplied.Broadcast(BurningStatus, Intensity);
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
		StateExposureTimers.Remove(StateTag); // Automatyczny reset licznika ekspozycji!
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

	if (bNeedsTimer)
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
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UHealthComponent* HealthComp = Owner->FindComponentByClass<UHealthComponent>();
	if (!HealthComp || HealthComp->IsDead())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DoTTimerHandle);
		}
		return;
	}

	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	static const FGameplayTag CorrodingStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Corroding"), false);
	static const FGameplayTag ElectrocutedStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
	static const FGameplayTag WetStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Neutral.Wet"), false);

	TArray<FGameplayTag> StatesToAutoRemove;

	// ====================================================================
	// AUTOMATYCZNY ZEGAR EKSPOZYCJI I WYGASZANIA STANÓW (Z UŻYCIEM TMAP):
	// ====================================================================
	for (const FGameplayTag& ActiveTag : ActiveStates)
	{
		float& TimeExposed = StateExposureTimers.FindOrAdd(ActiveTag);
		TimeExposed += 1.0f;

		// 1. PRĄD: Samoczynnie gaśnie po 2.0s:
		if (ActiveTag.MatchesTag(ElectrocutedStatus) && TimeExposed >= 2.0f)
		{
			StatesToAutoRemove.Add(ActiveTag);
		}

		// 2. OGIEŃ: Po 5s nakłada Poparzenia (Burns), a po 8s gaśnie (dla ścian):
		if (ActiveTag.MatchesTag(BurningStatus))
		{
			if (TimeExposed >= 5.0f)
			{
				static const FGameplayTag BurnsTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Burns"), false);
				HealthComp->AddInjury(BurnsTag, EAnatomicalLimb::None);
			}

			if (AutoExtinguishDuration > 0.0f && TimeExposed >= AutoExtinguishDuration)
			{
				StatesToAutoRemove.Add(ActiveTag);
			}
		}

		// 3. ŚCIEKI: Po 45s -> Stopa Okopowa:
		if (ActiveTag.MatchesTag(WetStatus) && TimeExposed >= 45.0f)
		{
			static const FGameplayTag TrenchFootTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.TrenchFoot"), false);
			HealthComp->AddInjury(TrenchFootTag, EAnatomicalLimb::Legs);
		}
	}

	for (const FGameplayTag& ExpiredTag : StatesToAutoRemove)
	{
		RemoveState(ExpiredTag);
	}

	// ====================================================================
	// NALICZANIE OBRAŻEŃ (DoT):
	// ====================================================================
	if (HasState(BurningStatus))
	{
		HealthComp->TakeDamage(BurnDamagePerSecond, BurningStatus);

		if (ABaseInteractable* InterProp = Cast<ABaseInteractable>(Owner))
		{
			InterProp->TriggerStateEmission();
		}
	}

	if (HasState(CorrodingStatus))
	{
		HealthComp->TakeDamage(AcidDamagePerSecond, CorrodingStatus);
	}

	if (HasState(ElectrocutedStatus))
	{
		HealthComp->TakeDamage(ShockDamagePerSecond, ElectrocutedStatus);

		if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
		{
			StatusComp->ApplyStun(0.45f); // Szarpnięcie mięśni co sekundę!
		}
	}
}