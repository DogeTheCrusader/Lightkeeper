#include "StaminaComponent.h"
#include "LightkeeperCharacter.h"
#include "ProgressionComponent.h"
#include "HealthComponent.h"
#include "InteractionComponent.h"
#include "InventoryComponent.h"
#include "ImSimSensorySubsystem.h"
#include "StatusEffectComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	Stamina = GetEffectiveMaxStamina();
	bIsFatigued = false;
	bSecondWindActive = false;
	RestTimer = 0.0f;
}

float UStaminaComponent::GetEffectiveMaxStamina() const
{
	float EffectiveMax = MaxStamina;
	if (ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner()))
	{
		if (UHealthComponent* HealthComp = OwnerChar->FindComponentByClass<UHealthComponent>())
		{
			// Mnożnik pobierany czysto z ciała (Pęknięte żebra = 0.5x):
			EffectiveMax *= HealthComp->GetMaxStaminaMultiplier();
		}
	}
	return EffectiveMax;
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
	UHealthComponent* HealthComp = OwnerChar ? OwnerChar->FindComponentByClass<UHealthComponent>() : nullptr;

	float EffectiveMaxStamina = GetEffectiveMaxStamina();
	Stamina = FMath::Clamp(Stamina, 0.0f, EffectiveMaxStamina);

	if (bIsSprinting && !CanSprint())
	{
		StopSprint();
	}

	// 1. ZDARZENIE NISKIEJ STAMINY (HealthComponent sam decyduje o bólu i arytmii!):
	if (Stamina <= (EffectiveMaxStamina * LowStaminaThresholdPercent))
	{
		OnLowStaminaTick.Broadcast(DeltaTime);
	}

	// 2. ZLICZANIE ZUŻYCIA:
	float SprintDrain = bIsSprinting ? DrainRate : 0.0f;
	float CarryDrain = GetCarryingStaminaDrain();
	float TotalDrainPerSecond = SprintDrain + CarryDrain;

	if (HealthComp && TotalDrainPerSecond > 0.0f)
	{
		TotalDrainPerSecond *= HealthComp->GetStaminaDrainMultiplier();
	}

	// 3. FAZA WYSIŁKU:
	if (TotalDrainPerSecond > 0.0f)
	{
		RestTimer = 0.0f;
		bSecondWindActive = false;

		if (bIsSprinting)
		{
			OnSprintTick.Broadcast(DeltaTime);
			FootstepNoiseTimer += DeltaTime;
			if (FootstepNoiseTimer >= 0.32f)
			{
				FootstepNoiseTimer = 0.0f;

				if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
				{
					static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
					Sensory->RegisterNoise(GetOwner()->GetActorLocation(), 450.0f, NoiseTag);
				}
			}
		}
		else
		{
			FootstepNoiseTimer = 0.0f;
		}

		Stamina = FMath::Clamp(Stamina - (TotalDrainPerSecond * DeltaTime), 0.0f, EffectiveMaxStamina);
		OnStaminaChanged.Broadcast(Stamina, EffectiveMaxStamina);

		if ((Stamina / EffectiveMaxStamina) <= FatigueThresholdPercent)
		{
			bIsFatigued = true;
		}

		if (Stamina <= 0.0f)
		{
			OnExhaustionTriggered.Broadcast();
			StopSprint();

			if (CarryDrain > 0.0f && OwnerChar)
			{
				if (UInteractionComponent* InterComp = OwnerChar->FindComponentByClass<UInteractionComponent>())
				{
					InterComp->StopInteraction();
					OnForcedDropDueToFatigue.Broadcast();
				}
			}
		}
	}
	// 4. FAZA REGENERACJI:
	else if (Stamina < EffectiveMaxStamina)
	{
		RestTimer += DeltaTime;

		// Czas na Drugi Oddech (HealthComponent wydłuża go przy obitych żebrach):
		float RequiredRestTime = RestTimeToTriggerRush * (HealthComp ? HealthComp->GetStaminaDrainMultiplier() : 1.0f);

		if (bIsFatigued && RestTimer >= RequiredRestTime && !bSecondWindActive)
		{
			bSecondWindActive = true;
			bIsFatigued = false;
			RestTimer = 0.0f;
			OnSecondWindActivated.Broadcast();
		}

		float EffectiveRegen = CalculateCurrentRegenRate();
		Stamina = FMath::Clamp(Stamina + (EffectiveRegen * DeltaTime), 0.0f, EffectiveMaxStamina);
		OnStaminaChanged.Broadcast(Stamina, EffectiveMaxStamina);

		if (bSecondWindActive && Stamina >= (EffectiveMaxStamina * SecondWindExitThreshold))
		{
			bSecondWindActive = false;
		}
	}
	else
	{
		bIsFatigued = false;
		bSecondWindActive = false;
		RestTimer = 0.0f;
	}
}

bool UStaminaComponent::CanSprint() const
{
	ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
	if (!OwnerChar) return false;

	if (!OwnerChar->CanPerformAction(EPlayerAction::Sprint)) return false;

	static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
	if (OwnerChar->StatusComp && OwnerChar->StatusComp->HasStatusEffect(GuardTag))
	{
		return false; // Nie można biegać w trakcie trzymania bloku!
	}

	if (UInteractionComponent* InterComp = OwnerChar->FindComponentByClass<UInteractionComponent>())
	{
		if (UPrimitiveComponent* HeldMesh = InterComp->GetGrabbedComponent())
		{
			if (HeldMesh->IsSimulatingPhysics() && HeldMesh->GetMass() > MaxCarryingMassForSprint)
			{
				return false;
			}
		}
	}

	return (Stamina > 0.5f);
}

bool UStaminaComponent::TryConsumeStamina(float Amount)
{
	if (Amount <= 0.0f) return true;

	if (Stamina < Amount)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[STAMINA] Za mało staminy!"));
#endif
		return false;
	}

	RestTimer = 0.0f;
	bSecondWindActive = false;

	float EffectiveMax = GetEffectiveMaxStamina();
	Stamina = FMath::Clamp(Stamina - Amount, 0.0f, EffectiveMax);
	OnStaminaChanged.Broadcast(Stamina, EffectiveMax);

	if ((Stamina / EffectiveMax) <= FatigueThresholdPercent)
	{
		bIsFatigued = true;
	}

	if (Stamina <= 0.0f)
	{
		OnExhaustionTriggered.Broadcast();
	}

	return true;
}

void UStaminaComponent::StartSprint()
{
	bWantsToSprint = true;

	ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	if (OwnerChar->GetCharacterMovement()->IsFalling()) return;

	if (CanSprint())
	{
		bIsSprinting = true;
		OwnerChar->UpdateMovementSpeed();
	}
	else
	{
		bIsSprinting = false;
		OwnerChar->UpdateMovementSpeed();
	}
}

void UStaminaComponent::StopSprint()
{
	bWantsToSprint = false;
	bIsSprinting = false;

	if (ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner()))
	{
		OwnerChar->UpdateMovementSpeed();
	}
}

void UStaminaComponent::HandleLanded()
{
	if (bWantsToSprint && CanSprint())
	{
		bIsSprinting = true;
		if (ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner()))
		{
			OwnerChar->UpdateMovementSpeed();
		}
	}
}

float UStaminaComponent::CalculateCurrentRegenRate() const
{
	float Multiplier = 1.0f;

	if (bSecondWindActive) Multiplier = SecondWindMultiplier;
	else if (bIsFatigued) Multiplier = FatigueRegenMultiplier;

	float InventoryPenalty = 1.0f;
	if (AActor* Owner = GetOwner())
	{
		if (UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>())
		{
			const int32 TotalCapacity = InvComp->Columns * InvComp->Rows;
			if (TotalCapacity > 0)
			{
				int32 OccupiedCells = 0;
				for (const FInventorySlot& Slot : InvComp->StoredItems)
				{
					int32 Area = Slot.ItemData.GridSize.X * Slot.ItemData.GridSize.Y;
					OccupiedCells += FMath::Max(1, Area);
				}

				float FillRatio = (float)OccupiedCells / (float)TotalCapacity;
				if (FillRatio > 0.75f) InventoryPenalty = 0.70f;
			}
		}
	}

	return BaseRegenRate * Multiplier * InventoryPenalty;
}

float UStaminaComponent::GetCarryingStaminaDrain() const
{
	if (AActor* Owner = GetOwner())
	{
		if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
		{
			if (AActor* GrabbedActor = InterComp->GetGrabbedActor())
			{
				EInteractionType HeldType = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
				if (HeldType == EInteractionType::Grab_Free)
				{
					if (UPrimitiveComponent* HeldMesh = InterComp->GetGrabbedComponent())
					{
						if (HeldMesh->IsSimulatingPhysics())
						{
							float Mass = HeldMesh->GetMass();
							if (Mass > HeavyCarryMassThreshold)
							{
								// ====================================================================
								// 1. GRACZ Z WIGOREM: NIESIE CIĘŻKIE PRZEDMIOTY ZA DARMO!
								// ====================================================================
								if (ALightkeeperCharacter* Player = Cast<ALightkeeperCharacter>(Owner))
								{
									static const FGameplayTag HeavyLifterTag = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.HeavyLifter"), false);
									static const FGameplayTag IronSpineTag = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.IronSpine"), false);

									if (Player->ProgressionComp && (Player->ProgressionComp->HasPerk(HeavyLifterTag) || Player->ProgressionComp->HasPerk(IronSpineTag)))
									{
										// Zdrowy siłacz niesie barykadę bez żadnego drenażu staminy w marszu:
										return 0.0f;
									}
								}

								// ====================================================================
								// 2. BEZ PERKA: TWARDY LIMIT DRENAŻU (Max 5.0 pkt/s zamiast 120 pkt/s!)
								// ====================================================================
								float ExcessMass = Mass - HeavyCarryMassThreshold;
								float BaseDrain = FMath::Clamp(ExcessMass * 0.04f, 1.5f, 5.0f);

								// ====================================================================
								// 3. SUROWOŚĆ SURVIVAL HORRORU: URAZY CIAŁA PODWAJAJĄ DRENAŻ!
								// (Zwichnięty bark = x1.8 drenażu, Pęknięte żebra = x1.25 drenażu)
								// ====================================================================
								if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
								{
									BaseDrain *= Health->GetMouseResistanceMultiplier();
									BaseDrain *= Health->GetStaminaDrainMultiplier();
								}

								return BaseDrain;
							}
						}
					}
				}
			}
		}
	}
	return 0.0f;
}

float UStaminaComponent::GetEncumbranceSpeedMultiplier() const
{
	float Multiplier = 1.0f;

	if (AActor* Owner = GetOwner())
	{
		if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
		{
			if (AActor* GrabbedActor = InterComp->GetGrabbedActor())
			{
				if (IPhysicalInteract::Execute_GetInteractionType(GrabbedActor) == EInteractionType::Grab_Free)
				{
					if (UPrimitiveComponent* HeldMesh = InterComp->GetGrabbedComponent())
					{
						if (HeldMesh->IsSimulatingPhysics())
						{
							float Mass = HeldMesh->GetMass();
							float MassPenalty = FMath::Clamp(Mass * 0.02f, 0.0f, 0.5f);

							// ====================================================================
							// BONUS WIGORU: Każdy Tier Wigoru redukuje karę spowolnienia o 35%!
							// ====================================================================
							if (ALightkeeperCharacter* Player = Cast<ALightkeeperCharacter>(Owner))
							{
								if (Player->ProgressionComp)
								{
									int32 VigorTier = Player->ProgressionComp->GetStatTier(ECharacterStat::Vigor);
									MassPenalty *= FMath::Clamp(1.0f - (VigorTier * 0.35f), 0.1f, 1.0f);
								}
							}

							Multiplier -= MassPenalty;
						}
					}
				}
			}
		}
	}

	return FMath::Clamp(Multiplier, 0.35f, 1.0f);
}