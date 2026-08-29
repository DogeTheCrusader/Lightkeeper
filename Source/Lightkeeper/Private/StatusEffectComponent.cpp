#include "StatusEffectComponent.h"
#include "HealthComponent.h"
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
	bIsBleeding = false;
	CurrentSpeedMultiplier = 1.0f;
	ActiveStatusTags.Reset();
}

void UStatusEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimerHandle);
		World->GetTimerManager().ClearTimer(SlowTimerHandle);
		World->GetTimerManager().ClearTimer(BleedTimerHandle);
		World->GetTimerManager().ClearTimer(BleedDurationTimerHandle);
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

void UStatusEffectComponent::ApplyStatusEffectFromTable(FGameplayTag StatusTag, float CustomDuration)
{
	if (!StatusTag.IsValid() || ImmuneStatusTags.HasTag(StatusTag)) return;

	const FStatusEffectDataRow* Row = FindStatusEffectRow(StatusTag);
	float Duration = (CustomDuration >= 0.0f) ? CustomDuration : (Row ? Row->DefaultDuration : 10.0f);

	// 1. Dodajemy tag do aktywnych:
	AddStatusEffect(StatusTag, Duration);

	if (Row)
	{
		// 2. Jeśli status to STUN:
		if (Row->bIsStun)
		{
			ApplyStun(Duration);
		}

		// 3. Jeśli status to SPOWOLNIENIE:
		if (Row->SpeedMultiplier < 1.0f)
		{
			ApplySlow(Row->SpeedMultiplier, Duration);
		}

		// 4. Jeśli status zadaje DoT (np. Krwawienie):
		if (Row->DamagePerTick > 0.0f)
		{
			ApplyBleed(Duration, Row->DamagePerTick, Row->TickInterval);
		}
	}
}

void UStatusEffectComponent::AddStatusEffect(FGameplayTag StatusTag, float Duration)
{
	if (!StatusTag.IsValid() || ImmuneStatusTags.HasTag(StatusTag) || ActiveStatusTags.HasTag(StatusTag)) return;

	ActiveStatusTags.AddTag(StatusTag);
	OnStatusEffectAdded.Broadcast(StatusTag);

	if (Duration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle TagTimer;
			World->GetTimerManager().SetTimer(TagTimer, [this, StatusTag]()
				{
					RemoveStatusEffect(StatusTag);
				}, Duration, false);
		}
	}
}

void UStatusEffectComponent::RemoveStatusEffect(FGameplayTag StatusTag)
{
	if (!StatusTag.IsValid() || !ActiveStatusTags.HasTag(StatusTag)) return;

	ActiveStatusTags.RemoveTag(StatusTag);
	OnStatusEffectRemoved.Broadcast(StatusTag);
}

void UStatusEffectComponent::ClearAllStatusEffects()
{
	ActiveStatusTags.Reset();
	StopBleed();
	EndStun();
	EndSlow();
}

void UStatusEffectComponent::ApplyStun(float Duration)
{
	if (bImmuneToStun || Duration <= 0.0f) return;

	static const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
	if (ImmuneStatusTags.HasTag(StunTag)) return;

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	bIsStunned = true;
	OwnerChar->GetCharacterMovement()->DisableMovement();
	OnStunStateChanged.Broadcast(true);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Purple,
			FString::Printf(TEXT("⚡ [STUN] %s został SPARALIŻOWANY na %.1fs!"), *OwnerChar->GetName(), Duration));
	}
#endif

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimerHandle);
		World->GetTimerManager().SetTimer(StunTimerHandle, this, &UStatusEffectComponent::EndStun, Duration, false);
	}
}

void UStatusEffectComponent::EndStun()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	bIsStunned = false;
	OwnerChar->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	OnStunStateChanged.Broadcast(false);
}

void UStatusEffectComponent::ApplyBleed(float Duration, float DamagePerTick, float TickInterval)
{
	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	if (ImmuneStatusTags.HasTag(BleedTag)) return;

	bIsBleeding = true;
	BleedDamageTick = DamagePerTick;
	AddStatusEffect(BleedTag, Duration);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BleedTimerHandle);
		World->GetTimerManager().ClearTimer(BleedDurationTimerHandle);

		World->GetTimerManager().SetTimer(BleedTimerHandle, this, &UStatusEffectComponent::ProcessBleedTick, TickInterval, true, 1.0f);

		if (Duration > 0.0f)
		{
			World->GetTimerManager().SetTimer(BleedDurationTimerHandle, this, &UStatusEffectComponent::StopBleed, Duration, false);
		}
	}
}

void UStatusEffectComponent::ProcessBleedTick()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UHealthComponent* HealthComp = Owner->FindComponentByClass<UHealthComponent>();
	if (!HealthComp || HealthComp->IsDead() || !bIsBleeding)
	{
		StopBleed();
		return;
	}

	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	HealthComp->TakeDamage(BleedDamageTick, BleedTag);

	OnBloodDropSpawned.Broadcast(Owner->GetActorLocation());

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor(255, 50, 50),
			FString::Printf(TEXT("🩸 [KRWAWIENIE] %s traci krew (-%.1f HP)!"), *Owner->GetName(), BleedDamageTick));
	}
#endif
}

void UStatusEffectComponent::StopBleed()
{
	bIsBleeding = false;
	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	RemoveStatusEffect(BleedTag);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BleedTimerHandle);
		World->GetTimerManager().ClearTimer(BleedDurationTimerHandle);
	}
}

void UStatusEffectComponent::ApplySlow(float SpeedMultiplier, float Duration)
{
	if (Duration <= 0.0f) return;

	CurrentSpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.1f, 1.0f);

	// AUTOMATYCZNIE NAKŁADAMY TAG SPOWOLNIENIA:
	static const FGameplayTag SlowTag = FGameplayTag::RequestGameplayTag(FName("Status.Debuff.Slowed"), false);
	AddStatusEffect(SlowTag, Duration);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlowTimerHandle);
		World->GetTimerManager().SetTimer(SlowTimerHandle, this, &UStatusEffectComponent::EndSlow, Duration, false);
	}
}

void UStatusEffectComponent::EndSlow()
{
	CurrentSpeedMultiplier = 1.0f;

	// ZDEJMUJEMY TAG PO UPŁYWIE CZASU:
	static const FGameplayTag SlowTag = FGameplayTag::RequestGameplayTag(FName("Status.Debuff.Slowed"), false);
	RemoveStatusEffect(SlowTag);
}