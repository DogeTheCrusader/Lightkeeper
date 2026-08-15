#include "LightkeeperCharacter.h"
#include "GameFramework/CharacterMovementComponent.h" // <--- Wymagane dla GetCharacterMovement()!
#include "Components/CapsuleComponent.h"            // <--- Wymagane dla GetCapsuleComponent()!

#include "InteractionComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "SanityComponent.h"
#include "StatusEffectComponent.h"

ALightkeeperCharacter::ALightkeeperCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// ==========================================================
	// LINIJKI W KONSTRUKTORZE POSTACI!
	// ==========================================================
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->SetCrouchedHalfHeight(CrouchingCapsuleHalfHeight);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, StandingCapsuleHalfHeight);
	GetCharacterMovement()->AirControl = 0.4f;

	// ==========================================================
	// PODPINANIE KOMPONENTÓW
	// ==========================================================
	InteractionComp = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	StaminaComp = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComponent"));
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	SanityComp = CreateDefaultSubobject<USanityComponent>(TEXT("SanityComponent"));
	StatusComp = CreateDefaultSubobject<UStatusEffectComponent>(TEXT("StatusEffectComponent"));
}

void ALightkeeperCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
}

void ALightkeeperCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Płynna aktualizacje prędkości co klatkę (działa zawsze i wszędzie!):
	UpdateMovementSpeed();
}

void ALightkeeperCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ALightkeeperCharacter::ForwardMouseLook(float MouseX, float MouseY)
{
	if (InteractionComp)
	{
		if (InteractionComp->ProcessMouseLook(MouseX, MouseY, CameraSensitivity))
		{
			return; // Mebel przejął ruch myszką!
		}
	}

	AddControllerYawInput(MouseX * CameraSensitivity);
	AddControllerPitchInput(MouseY * CameraSensitivity);
}

void ALightkeeperCharacter::UpdateMovementSpeed()
{
	if (!GetCharacterMovement() || !GetCapsuleComponent()) return;

	// ====================================================================
	// SPRAWDZAMY FIZYCZNY STAN POZYCJI (Wysokość kapsuły):
	// Jeśli kapsuła jest niska (< 70 cm), to znaczy, że postać kuca!
	// ====================================================================
	float CurrentCapsuleHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	bool bIsCurrentlyCrouching = CurrentCapsuleHeight < 70.0f;

	// 1. BAZOWA PRĘDKOŚĆ (Kucanie / Sprint / Chód):
	float TargetSpeed = WalkSpeed; // Domyślnie 600.0

	if (bIsCurrentlyCrouching)
	{
		TargetSpeed = CrouchSpeed; // Jeśli kuca -> 200.0
	}
	else if (StaminaComp && StaminaComp->bIsSprinting)
	{
		TargetSpeed = SprintSpeed; // Jeśli biegnie -> 900.0
	}

	// 2. MNOŻNIK WAGI PRZEDMIOTU (Tylko dla wolnych propów - Grab_Free):
	if (InteractionComp && InteractionComp->GetGrabbedActor())
	{
		EInteractionType HeldType = IPhysicalInteract::Execute_GetInteractionType(InteractionComp->GetGrabbedActor());

		if (HeldType == EInteractionType::Grab_Free)
		{
			if (UPrimitiveComponent* HeldComp = InteractionComp->GetGrabbedComponent())
			{
				float PropMass = HeldComp->GetMass();
				TargetSpeed = InteractionComp->CalculateMovementSpeed(TargetSpeed, PropMass);
			}
		}
	}

	// 3. W PRZYSZŁOŚCI: MNOŻNIK URAZÓW (Z StatusEffectComponent):
	// if (StatusComp && StatusComp->HasStatusEffect(FGameplayTag::RequestGameplayTag("Status.Injury.Major.Legs")))
	// {
	//     TargetSpeed *= 0.5f; // Złamana noga spowalnia o połowę!
	// }

	// 3. APLIKUJEMY PRĘDKOŚĆ DO SILNIKA RUCHU:
	GetCharacterMovement()->MaxWalkSpeed = TargetSpeed;
}