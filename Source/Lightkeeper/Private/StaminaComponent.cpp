#include "StaminaComponent.h"
#include "LightkeeperCharacter.h"
#include "GameFramework/Character.h"
#include "InteractionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	Stamina = MaxStamina;
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Regeneracja i Zużycie Staminy w tle:
	if (bIsSprinting)
	{
		Stamina = FMath::Clamp(Stamina - (DrainRate * DeltaTime), 0.0f, MaxStamina);
		if (Stamina <= 0.0f)
		{
			StopSprint(); // Brak staminy ➔ zatrzymaj sprint!
		}
	}
	else
	{
		Stamina = FMath::Clamp(Stamina + (RegenRate * DeltaTime), 0.0f, MaxStamina);
	}
}

void UStaminaComponent::StartSprint()
{
	bWantsToSprint = true;

	ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	if (OwnerChar->GetCharacterMovement()->IsFalling() && !bIsSprinting) return;

	// Blokada sprintu jeśli trzymany przedmiot waży więcej niż 5 kg:
	if (UInteractionComponent* InterComp = OwnerChar->FindComponentByClass<UInteractionComponent>())
	{
		if (UPrimitiveComponent* HeldMesh = InterComp->GetGrabbedComponent())
		{
			if (HeldMesh->GetMass() > 5.0f)
			{
				return; // ZAKAZ SPRINTU Z CIĘŻKĄ SKRZYNIĄ!
			}
		}
	}

	if (Stamina > 10.0f)
	{
		bIsSprinting = true;
		OwnerChar->UpdateMovementSpeed(); // Postać sama ustawi prędkość SprintSpeed!
	}
}

void UStaminaComponent::StopSprint()
{
	bWantsToSprint = false;
	bIsSprinting = false;

	if (ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner()))
	{
		OwnerChar->UpdateMovementSpeed(); // Postać sama przywróci prędkość WalkSpeed lub CrouchSpeed!
	}
}

void UStaminaComponent::HandleLanded()
{
	if (bWantsToSprint && !bIsSprinting && Stamina > 10.0f)
	{
		StartSprint();
	}
}