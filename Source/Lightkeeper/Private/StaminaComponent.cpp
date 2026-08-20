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
	bWantsToSprint = true; // Zawsze pamiętamy wciśnięty Shift!

	ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	// Jeśli jesteśmy w locie, buforujemy zamiar (bWantsToSprint), ale nie zmieniamy prędkości w powietrzu:
	if (OwnerChar->GetCharacterMovement()->IsFalling())
	{
		return;
	}

	// Blokada przy ciężkich przedmiotach (> 5kg):
	if (UInteractionComponent* InterComp = OwnerChar->FindComponentByClass<UInteractionComponent>())
	{
		if (UPrimitiveComponent* HeldMesh = InterComp->GetGrabbedComponent())
		{
			if (HeldMesh->GetMass() > 5.0f)
			{
				bIsSprinting = false;
				OwnerChar->UpdateMovementSpeed();
				return;
			}
		}
	}

	if (Stamina > 10.0f)
	{
		bIsSprinting = true;
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
	// ====================================================================
	// BEZPOŚREDNIA AKTYWACJA PO LĄDOWANIU (Brak blokady IsFalling!):
	// ====================================================================
	if (bWantsToSprint && Stamina > 10.0f)
	{
		ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
		if (!OwnerChar) return;

		// Sprawdzamy czy nie trzymamy ciężkiej skrzyni:
		if (UInteractionComponent* InterComp = OwnerChar->FindComponentByClass<UInteractionComponent>())
		{
			if (UPrimitiveComponent* HeldMesh = InterComp->GetGrabbedComponent())
			{
				if (HeldMesh->GetMass() > 5.0f) return;
			}
		}

		bIsSprinting = true;
		OwnerChar->UpdateMovementSpeed(); // Natychmiast odpala prędkość 900.0 w klatce lądowania!
	}
}