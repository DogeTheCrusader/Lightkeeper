#include "TriggerSenderComponent.h"
#include "ImSimSensorySubsystem.h"
#include "PhysicalInteract.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"

UTriggerSenderComponent::UTriggerSenderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	RequiredPartTag = FGameplayTag::RequestGameplayTag(FName("Item.Part.BrassGear"), false);
}

void UTriggerSenderComponent::BeginPlay()
{
	Super::BeginPlay();

	// Jeśli maszyna nie wymaga części, od razu jest sprawna:
	if (!bRequiresMissingPart)
	{
		bIsPartInserted = true;
	}
}

bool UTriggerSenderComponent::TryInsertPart(AActor* InstigatorActor, FGameplayTag ItemTag, AActor* PhysicalPropActor)
{
	if (!bRequiresMissingPart || bIsPartInserted) return false;

	if (RequiredPartTag.IsValid() && ItemTag.MatchesTag(RequiredPartTag))
	{
		bIsPartInserted = true;

		// 1. Jeśli gracz trzymał fizyczny obiekt w rękach -> przyciągamy go do gniazda:
		if (PhysicalPropActor)
		{
			if (UPrimitiveComponent* PropPrim = PhysicalPropActor->FindComponentByClass<UPrimitiveComponent>())
			{
				PropPrim->SetSimulatePhysics(false);
				PropPrim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}

			if (USceneComponent* ParentComp = GetOwner()->GetRootComponent())
			{
				PhysicalPropActor->AttachToComponent(ParentComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketAttachName);
			}

			// Dodajemy nowo włożoną zębatkę do listy kręcących się zębatek!
			if (UMeshComponent* AttachedMesh = PhysicalPropActor->FindComponentByClass<UMeshComponent>())
			{
				FLinkedGearData NewGear;
				NewGear.GearMesh = AttachedMesh;
				NewGear.GearRatio = 1.0f;
				LinkedGears.Add(NewGear);
			}
		}

		OnPartInserted.Broadcast();

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
				FString::Printf(TEXT("⚙️ [%s] Włożono brakującą część: %s! Mechanizm sprawny."), *GetOwner()->GetName(), *ItemTag.ToString()));
		}
#endif
		return true;
	}

	return false;
}

void UTriggerSenderComponent::ProcessMechanicalDelta(float AxisDelta)
{
	if (bRequiresMissingPart && !bIsPartInserted) return;

	// Obracamy każdą połączoną zębatkę z jej indywidualnym przełożeniem (GearRatio):
	for (const FLinkedGearData& Gear : LinkedGears)
	{
		if (Gear.GearMesh)
		{
			FRotator DeltaRot = FRotator(
				Gear.RotationAxis.Pitch * AxisDelta * Gear.GearRatio,
				Gear.RotationAxis.Yaw * AxisDelta * Gear.GearRatio,
				Gear.RotationAxis.Roll * AxisDelta * Gear.GearRatio
			);

			Gear.GearMesh->AddLocalRotation(DeltaRot);
		}
	}
}

void UTriggerSenderComponent::SetMechanismActivated(bool bNewActivated)
{
	if (bRequiresMissingPart && !bIsPartInserted) return;

	if (bIsActivated != bNewActivated)
	{
		bIsActivated = bNewActivated;

		// ====================================================================
		// HAŁAS METALICZNEGO DOPIĘCIA / ZATRZASKU MECHANIZMU DLA AI (3.5 m):
		// ====================================================================
		if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);

			FGameplayTag MatTag;
			if (GetOwner()->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
			{
				MatTag = IPhysicalInteract::Execute_GetMaterialTag(GetOwner());
			}

			// Trzask zapadki wajchy (2.5m, a dla stali 3.7m):
			Sensory->RegisterNoise(GetOwner()->GetActorLocation(), 250.0f, NoiseTag, MatTag);
		}

		OnMechanismStateChanged.Broadcast(bIsActivated);

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			FString StateStr = bIsActivated ? TEXT("🟢 [URUCHOMIONO / ZATRZASK]") : TEXT("🔴 [WYŁĄCZONO]");
			FColor MsgColor = bIsActivated ? FColor::Green : FColor::Red;
			GEngine->AddOnScreenDebugMessage(-1, 3.5f, MsgColor,
				FString::Printf(TEXT("%s Mechanizm: %s (Trzask zapadki 3.5m dla AI)"), *StateStr, *GetOwner()->GetName()));
		}
#endif
	}
}