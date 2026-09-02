#include "StatusEffectComponent.h"
#include "HealthComponent.h"
#include "ImSimSensorySubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UStatusEffectComponent::UStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStatusEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	bIsStunned = false;
	CurrentSpeedMultiplier = 1.0f;
	ActiveStatusTags.Reset();
	ActiveStatusInstances.Empty();
}

void UStatusEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MasterHeartbeatTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

const FStatusEffectDataRow* UStatusEffectComponent::FindStatusEffectRow(const FGameplayTag& StatusTag) const
{
	if (!StatusEffectDataTable || !StatusTag.IsValid()) return nullptr;

	const FName RowName = StatusTag.GetTagName();
	static const FString ContextString(TEXT("StatusEffectLookup"));
	return StatusEffectDataTable->FindRow<FStatusEffectDataRow>(RowName, ContextString);
}

void UStatusEffectComponent::ApplyStatusEffectFromTable(FGameplayTag StatusTag, float CustomDuration, float CustomDamagePerTick, float CustomTickInterval)
{
	if (!StatusTag.IsValid() || ImmuneStatusTags.HasTag(StatusTag)) return;

	const FStatusEffectDataRow* Row = FindStatusEffectRow(StatusTag);
	float Duration = (CustomDuration >= 0.0f) ? CustomDuration : (Row ? Row->DefaultDuration : 10.0f);

	if (Row && Row->bIsStun && bImmuneToStun) return;

	// Jeśli status już trwa -> odnawiamy czas:
	for (FActiveStatusInstance& Instance : ActiveStatusInstances)
	{
		if (Instance.StatusTag.MatchesTagExact(StatusTag))
		{
			if (Duration > 0.0f)
			{
				Instance.RemainingDuration = FMath::Max(Instance.RemainingDuration, Duration);
				Instance.bIsTimed = true;
			}
			if (CustomDamagePerTick > 0.0f) Instance.DamagePerTick = CustomDamagePerTick;
			if (CustomTickInterval > 0.0f) Instance.TickInterval = CustomTickInterval;
			return;
		}
	}

	static const FGameplayTag ElectrocutedHazard = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
	bool bIsPurePhysicalStun = (Row && Row->bIsStun && !StatusTag.MatchesTag(ElectrocutedHazard));

	FActiveStatusInstance NewInstance;
	NewInstance.StatusTag = StatusTag;
	NewInstance.RemainingDuration = Duration;
	NewInstance.bIsTimed = (Duration > 0.0f);
	NewInstance.DamagePerTick = (CustomDamagePerTick > 0.0f) ? CustomDamagePerTick : (Row ? Row->DamagePerTick : 0.0f);
	NewInstance.TickInterval = (CustomTickInterval > 0.0f) ? CustomTickInterval : (Row ? FMath::Max(0.1f, Row->TickInterval) : 1.0f);
	NewInstance.TimeUntilNextTick = NewInstance.TickInterval;
	NewInstance.bIsStun = bIsPurePhysicalStun;
	NewInstance.SpeedMultiplier = StatusTag.MatchesTag(ElectrocutedHazard) ? 0.40f : (Row ? Row->SpeedMultiplier : 1.0f);
	NewInstance.bSpawnsBloodScent = Row ? Row->bSpawnsBloodScent : false;

	ActiveStatusInstances.Add(NewInstance);
	ActiveStatusTags.AddTag(StatusTag);
	OnStatusEffectAdded.Broadcast(StatusTag);

	// ====================================================================
	// FAZA 1: POCZĄTKOWY SZOK PRĄDEM (NATYCHMIASTOWY STUN NA 1.5s!):
	// ====================================================================
	if (StatusTag.MatchesTag(ElectrocutedHazard))
	{
		ApplyStun(0.5f); // Pierwsze uderzenie paraliżuje na 1.5 sekundy!
	}

	UpdateAggregatedStates();

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(MasterHeartbeatTimerHandle))
		{
			World->GetTimerManager().SetTimer(MasterHeartbeatTimerHandle, this, &UStatusEffectComponent::ProcessMasterHeartbeat, 0.25f, true);
		}
	}
}

void UStatusEffectComponent::ProcessMasterHeartbeat()
{
	const float DeltaStep = 0.25f;
	AActor* Owner = GetOwner();
	UHealthComponent* HealthComp = Owner ? Owner->FindComponentByClass<UHealthComponent>() : nullptr;

	for (int32 i = ActiveStatusInstances.Num() - 1; i >= 0; --i)
	{
		FActiveStatusInstance& Instance = ActiveStatusInstances[i];

		if (Instance.bIsTimed)
		{
			Instance.RemainingDuration -= DeltaStep;
		}

		Instance.TimeUntilNextTick -= DeltaStep;

		// ====================================================================
		// WYKONANIE TICKA DOT (OBRAŻENIA CO INTERWAŁ):
		// ====================================================================
		if (Instance.TimeUntilNextTick <= 0.0f)
		{
			Instance.TimeUntilNextTick = Instance.TickInterval;

			// 1. Obrażenia elektryczne:
			if (Instance.DamagePerTick > 0.0f && HealthComp && !HealthComp->IsDead())
			{
				HealthComp->TakeDamage(Instance.DamagePerTick, Instance.StatusTag);
			}

			// ====================================================================
			// FAZA 2: WYRAŹNY MIKRO-STUN CO IMPULS (SZARPNIĘCIE NA 0.22s):
			// Kasujemy pęd i zamrażamy postać na 0.22s przy każdym uderzeniu 8 HP!
			// ====================================================================
			static const FGameplayTag ElectroTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
			if (Instance.StatusTag.MatchesTag(ElectroTag))
			{
				if (ACharacter* Char = Cast<ACharacter>(Owner))
				{
					if (Char->GetCharacterMovement())
					{
						Char->GetCharacterMovement()->Velocity = FVector::ZeroVector;
					}
				}

				// Jeśli postać skończyła już początkowy stun 1.5s -> aplikujemy wyraźny impuls 0.22s:
				if (!bIsStunned)
				{
					ApplyStun(0.35f);
				}
			}

			// 3. Emisja zapachu krwi:
			if (Instance.bSpawnsBloodScent && Owner)
			{
				if (UImSimSensorySubsystem* SensorySubsystem = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
				{
					static const FGameplayTag BloodScentTag = FGameplayTag::RequestGameplayTag(FName("Scent.Type.Blood"), false);
					FVector ScentLocation = Owner->GetActorLocation() - FVector(0.0f, 0.0f, 80.0f);
					SensorySubsystem->RegisterScent(ScentLocation, 2500.0f, BloodScentTag, 15.0f);
				}
				OnBloodDropSpawned.Broadcast(Owner->GetActorLocation());
			}
		}

		// WYGAŚNIĘCIE STATUSU:
		if (Instance.bIsTimed && Instance.RemainingDuration <= 0.0f)
		{
			FGameplayTag ExpiredTag = Instance.StatusTag;
			ActiveStatusInstances.RemoveAt(i);
			ActiveStatusTags.RemoveTag(ExpiredTag);
			OnStatusEffectRemoved.Broadcast(ExpiredTag);
		}
	}

	UpdateAggregatedStates();

	if (ActiveStatusInstances.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(MasterHeartbeatTimerHandle);
		}
	}
}

void UStatusEffectComponent::AddStatusEffect(FGameplayTag StatusTag, float Duration)
{
	ApplyStatusEffectFromTable(StatusTag, Duration);
}

void UStatusEffectComponent::UpdateAggregatedStates()
{
	bool bAnyStun = false;
	float SlowestSpeed = 1.0f;

	for (const FActiveStatusInstance& Instance : ActiveStatusInstances)
	{
		if (Instance.bIsStun) bAnyStun = true;
		// Zabezpieczenie: Mnożnik prędkości nie może być zerem:
		if (Instance.SpeedMultiplier < SlowestSpeed && Instance.SpeedMultiplier > 0.05f)
		{
			SlowestSpeed = Instance.SpeedMultiplier;
		}
	}

	bIsStunned = bAnyStun;

	// ====================================================================
	// ODBLOKOWANIE RUCHU POSTACI (Gwarantowane przywrócenie MOVE_Walking!):
	// ====================================================================
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (Char->GetCharacterMovement())
		{
			if (bIsStunned)
			{
				Char->GetCharacterMovement()->DisableMovement();
			}
			else if (Char->GetCharacterMovement()->MovementMode == MOVE_None)
			{
				// Natychmiast przywracamy normalne chodzenie pod [W]!
				Char->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
		}
	}

	// Minimalna prędkość to zawsze co najmniej 10% (nigdy 0!):
	CurrentSpeedMultiplier = FMath::Clamp(SlowestSpeed, 0.1f, 1.0f);
	OnStunStateChanged.Broadcast(bIsStunned);
}

void UStatusEffectComponent::RemoveStatusEffect(FGameplayTag StatusTag)
{
	for (int32 i = ActiveStatusInstances.Num() - 1; i >= 0; --i)
	{
		if (ActiveStatusInstances[i].StatusTag.MatchesTagExact(StatusTag))
		{
			ActiveStatusInstances.RemoveAt(i);
			ActiveStatusTags.RemoveTag(StatusTag);
			OnStatusEffectRemoved.Broadcast(StatusTag);
			break;
		}
	}
	UpdateAggregatedStates();
}

void UStatusEffectComponent::ClearAllStatusEffects()
{
	ActiveStatusInstances.Empty();
	ActiveStatusTags.Reset();
	UpdateAggregatedStates();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MasterHeartbeatTimerHandle);
	}
}

void UStatusEffectComponent::ApplyStun(float CustomDuration)
{
	if (bImmuneToStun) return;

	static const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("Status.Debuff.Stunned"), false);
	if (ImmuneStatusTags.HasTag(StunTag)) return;

	// Domyślny bezpieczny czas stuna (1.5s jeśli brak wpisu w tabeli):
	float FinalDuration = (CustomDuration > 0.0f) ? CustomDuration : 1.5f;

	ApplyStatusEffectFromTable(StunTag, FinalDuration);
}

void UStatusEffectComponent::ApplyBleed(float CustomDuration, float CustomDamagePerTick, float CustomTickInterval)
{
	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	ApplyStatusEffectFromTable(BleedTag, CustomDuration, CustomDamagePerTick, CustomTickInterval);
}


void UStatusEffectComponent::ApplySlow(float CustomSpeedMultiplier, float CustomDuration)
{
	static const FGameplayTag SlowTag = FGameplayTag::RequestGameplayTag(FName("Status.Debuff.Slowed"), false);
	ApplyStatusEffectFromTable(SlowTag, CustomDuration);
}

void UStatusEffectComponent::StopBleed()
{
	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	RemoveStatusEffect(BleedTag);
}

bool UStatusEffectComponent::IsBleeding() const
{
	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	return HasStatusEffect(BleedTag);
}