#include "StaminaComponent.h"
#include "LightkeeperCharacter.h"
#include "GameFramework/Character.h"
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
	ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	// Blokada startu w powietrzu
	if (OwnerChar->GetCharacterMovement()->IsFalling() && !bIsSprinting) return;

	if (Stamina > 10.0f)
	{
		bIsSprinting = true;
		// Czytamy zmienną SprintSpeed bezpośrednio z Postaci!
		OwnerChar->GetCharacterMovement()->MaxWalkSpeed = OwnerChar->SprintSpeed;
	}
}

void UStaminaComponent::StopSprint()
{
	bIsSprinting = false;
	if (ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner()))
	{
		if (OwnerChar->GetCharacterMovement())
		{
			// Czytamy zmienną WalkSpeed bezpośrednio z Postaci!
			OwnerChar->GetCharacterMovement()->MaxWalkSpeed = OwnerChar->WalkSpeed;
		}
	}
}