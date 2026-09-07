#include "StaminaComponent.h"
#include "ProgressionComponent.h"
#include "HealthComponent.h"
#include "InteractionComponent.h"
#include "InventoryComponent.h"
#include "StatusEffectComponent.h"
#include "ReactionReceiverComponent.h"
#include "ImSimSensorySubsystem.h"
#include "Components/CapsuleComponent.h"  
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
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
	float VigorBonus = 0.0f;
	AActor* Owner = GetOwner();
	if (!Owner) return MaxStamina;

	// 1. PULL API Z PROGRESJI WIGORU (Czysta Kompozycja):
	if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
	{
		VigorBonus = ProgComp->GetMaxHealthBonus();
	}

	float EffectiveMax = MaxStamina + VigorBonus;

	// 2. PULL API Z URAZÓW KLATKI (Pęknięte żebra = -50% staminy):
	if (UHealthComponent* HealthComp = Owner->FindComponentByClass<UHealthComponent>())
	{
		EffectiveMax *= HealthComp->GetMaxStaminaMultiplier();
	}

	return EffectiveMax;
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner) return;

	UHealthComponent* HealthComp = Owner->FindComponentByClass<UHealthComponent>();
	UCharacterMovementComponent* MoveComp = Owner->FindComponentByClass<UCharacterMovementComponent>();
	ACharacter* OwnerChar = Cast<ACharacter>(Owner);

	float EffectiveMaxStamina = GetEffectiveMaxStamina();
	Stamina = FMath::Clamp(Stamina, 0.0f, EffectiveMaxStamina);

	if (bIsSprinting && !CanSprint())
	{
		StopSprint();
	}

	// 1. ZDARZENIE NISKIEJ STAMINY (Ból i arytmia w HealthComp):
	if (Stamina <= (EffectiveMaxStamina * LowStaminaThresholdPercent))
	{
		OnLowStaminaTick.Broadcast(DeltaTime);
	}

	// 2. SIATKA AKUSTYCZNA KROKÓW DLA AI (Działa w ruchu niezależnie od drenażu staminy!):
	if (MoveComp && !MoveComp->IsFalling())
	{
		float Speed2D = Owner->GetVelocity().Size2D();

		if (Speed2D > 30.0f)
		{
			FootstepNoiseTimer += DeltaTime;

			UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
			bool bIsPlayerCrouched = (OwnerChar && OwnerChar->bIsCrouched) ||
				MoveComp->IsCrouching() ||
				(Capsule && Capsule->GetScaledCapsuleHalfHeight() < 70.0f);

			// Weryfikacja głośnego podłoża (Metal / Szkło / Woda):
			FHitResult FloorHit = MoveComp->CurrentFloor.HitResult;
			FGameplayTag FloorMaterialTag;

			if (FloorHit.bBlockingHit && FloorHit.GetActor())
			{
				AActor* FloorActor = FloorHit.GetActor();

				if (FloorActor->ActorHasTag(FName("Material.Glass")))
				{
					FloorMaterialTag = FGameplayTag::RequestGameplayTag(FName("Material.Glass"), false);
				}
				else if (FloorActor->ActorHasTag(FName("Material.Metal")))
				{
					FloorMaterialTag = FGameplayTag::RequestGameplayTag(FName("Material.Metal"), false);
				}
				else if (FloorActor->ActorHasTag(FName("State.Element.Moisture.Water")) || FloorActor->ActorHasTag(FName("Material.Water")))
				{
					FloorMaterialTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Moisture.Water"), false);
				}
			}

			// Dobieramy interwał i głośność:
			float StepInterval = 0.55f;
			float BaseNoiseRadius = 220.0f; // Chód: 2.2m

			if (bIsSprinting)
			{
				StepInterval = 0.35f;
				BaseNoiseRadius = 650.0f; // Sprint: 6.5m
			}
			else if (bIsPlayerCrouched)
			{
				StepInterval = 0.70f;
				// Gwarantowane 0 cm na zwykłej podłodze, 140 cm na głośnej:
				BaseNoiseRadius = FloorMaterialTag.IsValid() ? 140.0f : 0.0f;
			}

			if (FootstepNoiseTimer >= StepInterval)
			{
				FootstepNoiseTimer = 0.0f;

				if (BaseNoiseRadius > 0.0f)
				{
					// PULL API Z PRECYZJI:
					float StealthMod = 1.0f;
					if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
					{
						StealthMod = ProgComp->GetStealthNoiseMultiplier();
					}

					float FinalFootstepRadius = BaseNoiseRadius * StealthMod;

					if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
					{
						static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
						float FloorCustomMod = UImSimSensorySubsystem::ExtractNoiseMultiplierFromActor(FloorHit.GetActor());

						Sensory->RegisterNoise(Owner->GetActorLocation(), BaseNoiseRadius, NoiseTag, FloorMaterialTag, FloorCustomMod);
					}
				}
			}
		}
		else
		{
			FootstepNoiseTimer = 0.0f;
		}
	}

	// 3. ZLICZANIE DRENAŻU STAMINY:
	float SprintDrain = bIsSprinting ? DrainRate : 0.0f;
	float CarryDrain = GetCarryingStaminaDrain();
	float TotalDrainPerSecond = SprintDrain + CarryDrain;

	if (HealthComp && TotalDrainPerSecond > 0.0f)
	{
		TotalDrainPerSecond *= HealthComp->GetStaminaDrainMultiplier();
	}

	if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
	{
		TotalDrainPerSecond *= ProgComp->GetStaminaCostReductionMultiplier();
	}

	// 4. FAZA WYSIŁKU:
	if (TotalDrainPerSecond > 0.0f)
	{
		RestTimer = 0.0f;
		bSecondWindActive = false;

		if (bIsSprinting)
		{
			OnSprintTick.Broadcast(DeltaTime);
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

			if (CarryDrain > 0.0f)
			{
				if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
				{
					InterComp->StopInteraction();
					OnForcedDropDueToFatigue.Broadcast();
				}
			}
		}
	}
	// 5. FAZA REGENERACJI:
	else if (Stamina < EffectiveMaxStamina)
	{
		RestTimer += DeltaTime;

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
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	// Blokada biegu w trakcie trzymania gardy:
	if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
	{
		static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
		if (StatusComp->HasStatusEffect(GuardTag))
		{
			return false;
		}
	}

	// Blokada biegu z ciężkimi przedmiotami (> 15kg):
	if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
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

bool UStaminaComponent::TryConsumeJumpStamina()
{
	float FinalCost = JumpStaminaCost;
	AActor* Owner = GetOwner();
	if (!Owner) return TryConsumeStamina(FinalCost);

	// Wigor redukuje koszt staminy za skok (PULL API):
	if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
	{
		FinalCost *= ProgComp->GetStaminaCostReductionMultiplier();
	}
	if (UHealthComponent* HealthComp = Owner->FindComponentByClass<UHealthComponent>())
	{
		FinalCost *= HealthComp->GetStaminaDrainMultiplier();
	}

	return TryConsumeStamina(FinalCost);
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

	AActor* Owner = GetOwner();
	if (!Owner) return;

	UCharacterMovementComponent* MoveComp = Owner->FindComponentByClass<UCharacterMovementComponent>();
	if (MoveComp && MoveComp->IsFalling()) return;

	bIsSprinting = CanSprint();
}

void UStaminaComponent::StopSprint()
{
	bWantsToSprint = false;
	bIsSprinting = false;
}

void UStaminaComponent::HandleLanded()
{
	if (bWantsToSprint && CanSprint())
	{
		bIsSprinting = true;
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
	AActor* Owner = GetOwner();
	if (!Owner) return 0.0f;

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

						if (Mass <= 20.0f) return 0.0f;

						float ExcessMass = Mass - 20.0f;
						float MassAddedDrain = FMath::Clamp(FMath::Sqrt(ExcessMass) * 0.22f, 0.0f, 3.2f);
						float BaseDrain = 2.0f + MassAddedDrain;

						// PULL API Z WIGORU:
						if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
						{
							static const FGameplayTag IronSpineTag = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.1B"), false);
							if (ProgComp->HasPerk(IronSpineTag)) return 0.0f;

							if (ProgComp->CanLiftHeavyProps()) BaseDrain *= 0.5f;
						}

						// PULL API Z URAZÓW CIAŁA:
						if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
						{
							BaseDrain *= Health->GetStaminaDrainMultiplier();
						}

						return BaseDrain;
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

							// PULL API Z WIGORU:
							if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
							{
								int32 VigorTier = ProgComp->GetStatTier(ECharacterStat::Vigor);
								MassPenalty *= FMath::Clamp(1.0f - (VigorTier * 0.35f), 0.1f, 1.0f);
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