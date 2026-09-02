#include "MetroidvaniaGateComponent.h"
#include "LightkeeperCharacter.h"
#include "ProgressionComponent.h"
#include "HealthComponent.h"
#include "ReactionReceiverComponent.h"
#include "ImSimSensorySubsystem.h"
#include "Engine/Engine.h"

UMetroidvaniaGateComponent::UMetroidvaniaGateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	//RequiredPerkTag = FGameplayTag::RequestGameplayTag(FName("Perk.Precision.2A"), false);
}

void UMetroidvaniaGateComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// ====================================================================
	// PEWNE WYKRYCIE DRZWI (Działa z ChildActorComponent i AttachToActor):
	// ====================================================================
	if (!TargetObjectToLock)
	{
		// 1. Sprawdzamy czy to ChildActor wewnątrz Blueprinta drzwi:
		if (Owner->GetParentActor())
		{
			TargetObjectToLock = Owner->GetParentActor();
		}
		// 2. Sprawdzamy czy podpięto w Outlinerze:
		else if (Owner->GetAttachParentActor())
		{
			TargetObjectToLock = Owner->GetAttachParentActor();
		}
		// 3. Sprawdzamy czy wisi bezpośrednio na drzwiach:
		else if (Owner->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			TargetObjectToLock = Owner;
		}
	}

	// Blokujemy drzwi na starcie:
	if (TargetObjectToLock && TargetObjectToLock->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		IPhysicalInteract::Execute_SetLocked(TargetObjectToLock, bIsLocked);
	}

	// Niszczenie fizyczne:
	if (UHealthComponent* HealthComp = Owner->FindComponentByClass<UHealthComponent>())
	{
		HealthComp->OnDeath.AddDynamic(this, &UMetroidvaniaGateComponent::HandleOwnerDeath);
	}

	// Kwas:
	if (UReactionReceiverComponent* ReactionComp = Owner->FindComponentByClass<UReactionReceiverComponent>())
	{
		ReactionComp->OnStateApplied.AddDynamic(this, &UMetroidvaniaGateComponent::HandleChemicalReaction);
	}
}

bool UMetroidvaniaGateComponent::TryUnlock(AActor* Instigator, FGameplayTag ToolOrKeyTag)
{
	if (!bIsLocked) return true;
	if (!Instigator) return false;

	if (!RequiredKeyTag.IsValid() && !RequiredPerkTag.IsValid())
	{
		UnlockGate(FName("Manual"));
		return true;
	}


	// 1. Sprawdzamy Perk bezpośrednio przez komponent progresji (BEZ CASTOWANIA):
	if (RequiredPerkTag.IsValid())
	{
		if (UProgressionComponent* ProgComp = Instigator->FindComponentByClass<UProgressionComponent>())
		{
			if (ProgComp->HasPerk(RequiredPerkTag))
			{
				UnlockGate(FName("Perk"));
				return true;
			}
		}
	}

	// 2. Sprawdzamy klucz:
	if (RequiredKeyTag.IsValid() && ToolOrKeyTag.IsValid())
	{
		if (ToolOrKeyTag.MatchesTag(RequiredKeyTag))
		{
			UnlockGate(FName("Key"));
			return true;
		}
	}

	return false;
}

void UMetroidvaniaGateComponent::ToggleGate()
{
	if (bIsLocked)
	{
		UnlockGate(FName("Manual"));
	}
	else
	{
		bIsLocked = true;
		OnGateStateChanged.Broadcast(true);

		// Blokujemy powiązane drzwi nadrzędne:
		if (TargetObjectToLock && TargetObjectToLock->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			IPhysicalInteract::Execute_SetLocked(TargetObjectToLock, true);
		}

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, TEXT("🔒 [ZASUWKA ZASUNIĘTA] Drzwi zaryglowane."));
		}
#endif
	}
}

void UMetroidvaniaGateComponent::UnlockGate(FName MethodName)
{
	if (!bIsLocked) return;

	bIsLocked = false;

	if (MethodName == FName("Key"))
	{
		bKeyDiscovered = true;
	}

	// 1. AUTOMATYCZNE ODRYGLOWANIE POWIĄZANYCH DRZWI:
	if (TargetObjectToLock && TargetObjectToLock->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		IPhysicalInteract::Execute_SetLocked(TargetObjectToLock, false);
	}

	// 2. FIZYCZNY UPADEK (TYLKO DLA KŁÓDEK - NIGDY DLA FRAMUGI DRZWI!):
// 2. FIZYCZNY UPADEK (100% INTERFEJS - ZERO CASTOWANIA NA ABaseInteractable!):
	if (bDropWithPhysicsOnUnlock)
	{
		if (AActor* Owner = GetOwner())
		{
			bool bIsDoorOrFurniture = false;

			// Pytamy czysto przez Interfejs:
			if (Owner->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
			{
				EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(Owner);
				bIsDoorOrFurniture = (Type == EInteractionType::Hinge ||
					Type == EInteractionType::Translation ||
					Type == EInteractionType::Crank);
			}

			// Upadek z fizyką odpala się TYLKO gdy obiekt NIE jest drzwiami/meblem (czyli jest małą kłódką!):
			if (!bIsDoorOrFurniture)
			{
				TargetObjectToLock = nullptr;
				bKeyDiscovered = false;

				if (USceneComponent* Root = Owner->GetRootComponent())
				{
					Root->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
				}
				Owner->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

				TArray<UPrimitiveComponent*> PrimComps;
				Owner->GetComponents<UPrimitiveComponent>(PrimComps);

				FVector DropImpulse = (Owner->GetActorForwardVector() * 35.0f) + FVector(0.0f, 0.0f, -50.0f);

				for (UPrimitiveComponent* PrimComp : PrimComps)
				{
					if (PrimComp)
					{
						PrimComp->SetMobility(EComponentMobility::Movable);
						PrimComp->SetSimulatePhysics(true);
						/*PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
						PrimComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
						PrimComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
						PrimComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
						PrimComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
						PrimComp->SetGenerateOverlapEvents(true);
						PrimComp->SetNotifyRigidBodyCollision(true);*/
						PrimComp->WakeRigidBody();
						PrimComp->AddImpulse(DropImpulse, NAME_None, true);
					}
				}
			}
		}
	}

	// 3. Hałas dla AI przy wyważeniu siłowym:
	if (MethodName == FName("BruteForce"))
	{
		if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
			Sensory->RegisterNoise(GetOwner()->GetActorLocation(), 1400.0f, NoiseTag);
		}
	}

	OnGateUnlocked.Broadcast(MethodName);
	OnGateStateChanged.Broadcast(false);
}

void UMetroidvaniaGateComponent::SetGateLocked(bool bNewLocked)
{
	bIsLocked = bNewLocked;
	OnGateStateChanged.Broadcast(bIsLocked);
}

void UMetroidvaniaGateComponent::HandleOwnerDeath()
{
	if (bIsLocked && bCanBeForcedByDamage)
	{
		UnlockGate(FName("BruteForce"));
	}
}

void UMetroidvaniaGateComponent::HandleChemicalReaction(FGameplayTag StateTag, float Intensity)
{
	if (!bIsLocked || !bCanBeMeltedByAcid) return;

	static const FGameplayTag CorrodingTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Corroding"), false);
	if (StateTag.MatchesTag(CorrodingTag))
	{
		UnlockGate(FName("AcidBypass"));
	}
}