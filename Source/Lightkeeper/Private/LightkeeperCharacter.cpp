#include "LightkeeperCharacter.h"
#include "GameFramework/CharacterMovementComponent.h" // <--- Wymagane dla GetCharacterMovement()!
#include "Components/CapsuleComponent.h"            // <--- Wymagane dla GetCapsuleComponent()!
#include "InteractionComponent.h"
#include "StaminaComponent.h"

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