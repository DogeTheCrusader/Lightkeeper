#include "ToolManagerComponent.h"
#include "BaseTool.h"
#include "InteractionComponent.h"
#include "UtilityManagerComponent.h"
#include "InventoryComponent.h"
#include "InventoryTypes.h"  
#include "BaseInteractable.h"    
#include "ProgressionComponent.h"
#include "StatusEffectComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"

UToolManagerComponent::UToolManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UToolManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Jeśli w edytorze ustawiono domyślną broń (np. Kij Latarnika) -> załóż ją na starcie!
	if (DefaultToolClass)
	{
		EquipToolByClass(DefaultToolClass);
	}
}

void UToolManagerComponent::EquipToolByClass(TSubclassOf<ABaseTool> ToolClass)
{
	if (!ToolClass) return;

	// Jeśli już coś trzymamy -> chowamy starą broń:
	if (CurrentEquippedTool)
	{
		HolsterCurrentTool();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawnujemy narzędzie w świecie:
	CurrentEquippedTool = GetWorld()->SpawnActor<ABaseTool>(ToolClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (CurrentEquippedTool)
	{
		AttachToolToHand(CurrentEquippedTool);
		CurrentEquippedTool->EquipTool(GetOwner());
		OnToolEquipped.Broadcast(CurrentEquippedTool);
	}
}

void UToolManagerComponent::AttachToolToHand(ABaseTool* ToolToAttach)
{
	if (!ToolToAttach) return;

	if (ACharacter* CharOwner = Cast<ACharacter>(GetOwner()))
	{
		if (CharOwner->GetMesh() && CharOwner->GetMesh()->DoesSocketExist(RightHandSocketName))
		{
			ToolToAttach->AttachToComponent(CharOwner->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RightHandSocketName);
		}
		else if (UCameraComponent* Cam = CharOwner->FindComponentByClass<UCameraComponent>())
		{
			ToolToAttach->AttachToComponent(Cam, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			// DOMYŚLNA BAZA (Dla prostych przedmiotów):
			FVector FinalOffset = FVector(30.0f, 15.0f, -12.0f);
			FRotator FinalRot = FRotator(0.0f, 0.0f, 0.0f);

			// JEŚLI BROŃ MA WŁASNY OFFSET W BLUEPRINCIE -> UŻYWAMY JEGO WARTOŚCI!
			if (!ToolToAttach->ToolItemData.HandGripOffset.Equals(FTransform::Identity))
			{
				FinalOffset = ToolToAttach->ToolItemData.HandGripOffset.GetLocation();
				FinalRot = ToolToAttach->ToolItemData.HandGripOffset.GetRotation().Rotator();
			}

			ToolToAttach->SetActorRelativeLocation(FinalOffset);
			ToolToAttach->SetActorRelativeRotation(FinalRot);
		}
	}
}

void UToolManagerComponent::HolsterCurrentTool()
{
	if (!CurrentEquippedTool) return;

	CurrentEquippedTool->UnequipTool();
	CurrentEquippedTool->Destroy();
	CurrentEquippedTool = nullptr;

	OnToolHolstered.Broadcast();
}

// ==========================================================
// MATRYCA STEROWANIA (INTENT SWITCH)
// ==========================================================

bool UToolManagerComponent::IsPlayerBusyWithInteraction() const
{
	if (AActor* Owner = GetOwner())
	{
		if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
		{
			// Jeśli gracz jest w trybie inspekcji LUB trzyma fizyczny obiekt/drzwi w dłoni:
			if (InterComp->IsInspecting() || InterComp->GetGrabbedComponent() != nullptr)
			{
				return true;
			}
		}
	}
	return false;
}

void UToolManagerComponent::Input_Aim_Pressed()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// ====================================================================
	// 1. JEŚLI NIESIESZ FIZYCZNY PROP -> ROZPOCZYNAMY ŁADOWANIE RZUTU (PPM)!
	// ====================================================================
	if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
	{
		if (InterComp->GetGrabbedComponent() != nullptr)
		{
			InterComp->StartSlamCharge(); // ZAPAMIĘTUJEMY CZAS STARTU ŁADOWANIA!
			return;
		}

		if (InterComp->IsInspecting()) return;
	}

	bIsAimInputHeld = true;

	if (CurrentEquippedTool)
	{
		CurrentEquippedTool->EnterAimState();
		OnAimStateChanged.Broadcast(true);
	}
}

void UToolManagerComponent::Input_Aim_Released()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
	{
		if (InterComp->GetGrabbedComponent() != nullptr)
		{
			InterComp->ReleaseSlamThrow();
			return;
		}
	}

	bIsAimInputHeld = false;

	// ====================================================================
	// ZDJĘCIE GARDY: PULL API - ZERO RZUTOWAŃ (Modularne dla Gracza i NPC!):
	// ====================================================================
	if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
	{
		static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
		if (StatusComp->HasStatusEffect(GuardTag))
		{
			StatusComp->RemoveStatusEffect(GuardTag);
			// StatusComp sam wyśle Broadcast(OnStatusEffectRemoved) -> Postać sama go podsłucha i zaktualizuje swoją prędkość!
		}
	}

	if (CurrentEquippedTool)
	{
		CurrentEquippedTool->ExitAimState();
		OnAimStateChanged.Broadcast(false);
	}
}

bool UToolManagerComponent::IsAiming() const
{
	// Gracz celuje TYLKO wtedy, gdy FIZYCZNIE trzyma wciśnięty PPM!
	return bIsAimInputHeld && CurrentEquippedTool != nullptr;
}

void UToolManagerComponent::Input_PrimaryAction_Pressed()
{
	if (IsPlayerBusyWithInteraction())
	{
		if (AActor* Owner = GetOwner())
		{
			if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
			{
				InterComp->StartInteraction();
			}
		}
		return;
	}

	// W pozycji bojowej -> rozpoczynamy ładowanie ciosu:
	if (IsAiming())
	{
		if (CurrentEquippedTool)
		{
			CurrentEquippedTool->StartPrimaryAction();
		}
	}
	else
	{
		if (AActor* Owner = GetOwner())
		{
			if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
			{
				InterComp->StartInteraction();
			}
		}
	}
}

void UToolManagerComponent::Input_Reload()
{
	// Zabezpieczenie przed inspekcją obiektów:
	if (IsPlayerBusyWithInteraction()) return;

	// 1. Jeśli trzymamy broń z własnym magazynkiem (Rewolwer / Strzelba) -> Ładujemy broń!
	if (CurrentEquippedTool && !CurrentEquippedTool->bUsesSharedFuel)
	{
		CurrentEquippedTool->ReloadTool();
		return;
	}

	// 2. FALLBACK: Jeśli mamy puste ręce LUB trzymamy Kij Latarnika -> dolewamy nafty do centralnego baku!
	if (AActor* Owner = GetOwner())
	{
		if (UUtilityManagerComponent* UtilMgr = Owner->FindComponentByClass<UUtilityManagerComponent>())
		{
			UtilMgr->RefillActiveUtility();
		}
	}
}

void UToolManagerComponent::Input_PrimaryAction_Released()
{
	if (IsAiming())
	{
		// Puszczenie LPM -> Odpala Atak Lekki LUB Atak Ciężki!
		if (CurrentEquippedTool)
		{
			CurrentEquippedTool->StopPrimaryAction();
		}
	}
	else
	{
		if (AActor* Owner = GetOwner())
		{
			if (UInteractionComponent* InterComp = Owner->FindComponentByClass<UInteractionComponent>())
			{
				InterComp->StopInteraction();
			}
		}
	}
}

void UToolManagerComponent::Input_QuickMelee()
{
	if (IsPlayerBusyWithInteraction()) return;

	if (CurrentEquippedTool)
	{
		CurrentEquippedTool->QuickMelee();
	}
}

void UToolManagerComponent::Input_Guard_Pressed()
{
	if (IsAiming())
	{
		if (AActor* Owner = GetOwner())
		{
			if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
			{
				static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
				StatusComp->AddStatusEffect(GuardTag);

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("🛡️ [GARDA] Przyjęto postawę obronną!"));
#endif
			}
		}
	}
}

void UToolManagerComponent::Input_Guard_Released()
{
	if (AActor* Owner = GetOwner())
	{
		if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
		{
			static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
			StatusComp->RemoveStatusEffect(GuardTag);
		}
	}
}

void UToolManagerComponent::EquipToolFromInventorySlot(int32 SlotIndex)
{
	if (AActor* Owner = GetOwner())
	{
		if (UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>())
		{
			if (InvComp->StoredItems.IsValidIndex(SlotIndex))
			{
				const FInventorySlot& Slot = InvComp->StoredItems[SlotIndex];

				if (Slot.ItemData.EquipType != EItemEquipType::None)
				{
					if (Slot.ItemData.EquipToolClass)
					{
						EquipToolByClass(Slot.ItemData.EquipToolClass);
					}
					else if (Slot.ItemData.DropClass)
					{
						if (CurrentEquippedTool) HolsterCurrentTool();

						FActorSpawnParameters SpawnParams;
						SpawnParams.Owner = Owner;
						SpawnParams.Instigator = Cast<APawn>(Owner);
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

						CurrentEquippedTool = GetWorld()->SpawnActor<ABaseTool>(ABaseTool::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
						if (CurrentEquippedTool)
						{
							// Pobieramy model 3D ze struktury ItemData:
							UStaticMesh* MeshToUse = Slot.ItemData.ItemMesh;

							CurrentEquippedTool->InitializeFromItemData(Slot.ItemData, MeshToUse);
							AttachToolToHand(CurrentEquippedTool);
							CurrentEquippedTool->EquipTool(Owner);
							OnToolEquipped.Broadcast(CurrentEquippedTool);
						}
					}
				}
			}
		}
	}
}

void UToolManagerComponent::ToggleSignatureTool()
{
	// 1. Jeśli aktualnie trzymamy już Kij Latarnika w dłoni -> chowamy go na plecy (puste ręce):
	if (CurrentEquippedTool && SignatureToolClass && CurrentEquippedTool->IsA(SignatureToolClass))
	{
		HolsterCurrentTool();
		return;
	}

	// 2. Jeśli mamy puste ręce LUB trzymamy inną broń (np. nóż) -> wyciągamy Kij Latarnika!
	if (SignatureToolClass)
	{
		EquipToolByClass(SignatureToolClass);
	}
}