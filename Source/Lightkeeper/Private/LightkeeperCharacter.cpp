#include "LightkeeperCharacter.h"
#include "GameFramework/CharacterMovementComponent.h" // <--- Wymagane dla GetCharacterMovement()!
#include "Components/CapsuleComponent.h"            // <--- Wymagane dla GetCapsuleComponent()!

#include "InteractionComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "SanityComponent.h"
#include "LanternComponent.h"
#include "StatusEffectComponent.h"
#include "InventoryComponent.h"

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
	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
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

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		// ====================================================================
		// 1. TELEMETRIA LATARNI I NAFTY
		// ====================================================================
		if (ULanternComponent* LanternComp = FindComponentByClass<ULanternComponent>())
		{
			int32 OilBottleCount = 0;
			if (UInventoryComponent* InvComp = FindComponentByClass<UInventoryComponent>())
			{
				// OPTYMALIZACJA CPU: Pobieramy tag tylko raz przy starcie gry!
				static const FGameplayTag OilTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Oil"), false);

				for (const FInventorySlot& Slot : InvComp->StoredItems)
				{
					if (Slot.ItemData.ItemTag.MatchesTag(OilTag))
					{
						OilBottleCount++;
					}
				}
			}
			FString LightState = LanternComp->bIsLit ? TEXT("WŁĄCZONE [F]") : TEXT("ZGASZONE [F]");
			FColor LightColor = LanternComp->bIsLit ? FColor::Yellow : FColor(150, 150, 150);

			GEngine->AddOnScreenDebugMessage(
				101, 0.0f, LightColor,
				FString::Printf(TEXT("[LATARNIA] Światło: %s | Paliwo: %.1f / %.1f | Butelki w plecaku: %d szt. (Uzupełnij [R])"),
					*LightState, LanternComp->CurrentFuel, LanternComp->MaxFuel, OilBottleCount)
			);
		}

		// ====================================================================
		// 2. TELEMETRIA PSYCHIKI I MROKU (Używamy Twojego SanityComp!)
		// ====================================================================
		if (SanityComp)
		{
			FString DarkState = SanityComp->bIsInDarkness ? FString::Printf(TEXT("TAK (Czas: %.1fs)"), SanityComp->TimeInDarkness) : TEXT("NIE (W Świetle)");
			FString MinorMadnessStr = SanityComp->bHasMinorMadness ? TEXT("AKTYWNE (Drain x1.5!)") : TEXT("Brak");
			float DynamicCapVal = SanityComp->GetCurrentDynamicComfortCap();

			FColor SanityColor = SanityComp->bIsInDarkness ? FColor(200, 100, 255) : FColor::Cyan;

			GEngine->AddOnScreenDebugMessage(
				102, 0.0f, SanityColor,
				FString::Printf(TEXT("[SANITY] %.1f / %.1f (Limit: %.0f%%) | Mrok: %s | Sufit Ukojenia: %.1f | Drobne Szaleństwo: %s | Zapaści: %d/3"),
					SanityComp->CurrentSanity, SanityComp->GetMaxSanity(), SanityComp->MaxSanityCapMultiplier * 100.0f, *DarkState, DynamicCapVal, *MinorMadnessStr, SanityComp->MentalCollapseCount)
			);
		}

		// ====================================================================
		// 3. TELEMETRIA CIAŁA I ZDROWIA (Używamy Twojego HealthComp i StaminaComp!)
		// ====================================================================
		if (HealthComp)
		{
			float StaminaVal = StaminaComp ? StaminaComp->Stamina : 100.0f;
			float MaxStaminaVal = StaminaComp ? StaminaComp->MaxStamina : 100.0f;

			FColor HealthColor = (HealthComp->CurrentHealth > 30.0f) ? FColor::Green : FColor::Red;

			GEngine->AddOnScreenDebugMessage(
				103, 0.0f, HealthColor,
				FString::Printf(TEXT("[CIAŁO] HP: %.1f / %.1f | Pasek Pęknięcia: %.0f%% | Stamina: %.1f / %.1f"),
					HealthComp->CurrentHealth, HealthComp->GetMaxHealth(), HealthComp->FractureMeter * 100.0f, StaminaVal, MaxStaminaVal)
			);
		}
	}
#endif
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

	float CurrentCapsuleHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	bool bIsCurrentlyCrouching = CurrentCapsuleHeight < 70.0f;

	// ====================================================================
	// 1. POPRAWNA HIERARCHIA PRĘDKOŚCI (Sprint ma ZAWSZE pierwszeństwo!):
	// ====================================================================
	float TargetSpeed = WalkSpeed; // Domyślnie 600.0

	if (StaminaComp && StaminaComp->bIsSprinting)
	{
		TargetSpeed = SprintSpeed; // Jeśli gracz sprintuje -> od razu 900.0 (nie czeka na wstawanie!)
	}
	else if (bIsCurrentlyCrouching)
	{
		TargetSpeed = CrouchSpeed; // Jeśli kuca i nie sprintuje -> 200.0
	}

	// 2. MNOŻNIK WAGI PRZEDMIOTU (Dla wolnych propów):
	if (InteractionComp && InteractionComp->GetGrabbedActor())
	{
		EInteractionType HeldType = IPhysicalInteract::Execute_GetInteractionType(InteractionComp->GetGrabbedActor());

		if (HeldType == EInteractionType::Grab_Free)
		{
			if (UPrimitiveComponent* HeldMesh = InteractionComp->GetGrabbedComponent())
			{
				float PropMass = HeldMesh->GetMass();
				TargetSpeed = InteractionComp->CalculateMovementSpeed(TargetSpeed, PropMass);
			}
		}
	}

	// 3. APLIKUJEMY PRĘDKOŚĆ DO SILNIKA RUCHU:
	GetCharacterMovement()->MaxWalkSpeed = TargetSpeed;
}

void ALightkeeperCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit); // Silnik przestawia postać z Falling na Walking

	// ====================================================================
	// ZAMIENIONE: Używamy HandleLanded, które nie jest blokowane przez IsFalling!
	// ====================================================================
	if (StaminaComp)
	{
		StaminaComp->HandleLanded();
	}
	else
	{
		UpdateMovementSpeed();
	}
}