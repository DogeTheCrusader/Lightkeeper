#include "LightkeeperCharacter.h"
#include "GameFramework/CharacterMovementComponent.h" // <--- Wymagane dla GetCharacterMovement()!
#include "Components/CapsuleComponent.h"            // <--- Wymagane dla GetCapsuleComponent()!
#include "ReactionReceiverComponent.h"
#include "InteractionComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "ProgressionComponent.h"
#include "SanityComponent.h"
#include "StatusEffectComponent.h"
#include "InventoryComponent.h"
#include "ToolManagerComponent.h"
#include "UtilityManagerComponent.h"
#include "LanternComponent.h"
#include "ImSimSensorySubsystem.h"

ALightkeeperCharacter::ALightkeeperCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// ==========================================================
	// 1. NAJPIERW TWORZYMY WSZYSTKIE KOMPONENTY (CreateDefaultSubobject):
	// ==========================================================
	InteractionComp = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	StaminaComp = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComponent"));
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	SanityComp = CreateDefaultSubobject<USanityComponent>(TEXT("SanityComponent"));
	StatusComp = CreateDefaultSubobject<UStatusEffectComponent>(TEXT("StatusEffectComponent"));
	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	ToolManagerComp = CreateDefaultSubobject<UToolManagerComponent>(TEXT("ToolManagerComp"));
	UtilityManagerComp = CreateDefaultSubobject<UUtilityManagerComponent>(TEXT("UtilityManagerComp"));
	LanternComp = CreateDefaultSubobject<ULanternComponent>(TEXT("LanternComp"));
	ReactionComp = CreateDefaultSubobject<UReactionReceiverComponent>(TEXT("ReactionComp"));
	ProgressionComp = CreateDefaultSubobject<UProgressionComponent>(TEXT("ProgressionComp"));

	// ==========================================================
	// 2. KONFIGURACJA POSTACI I SILNIKA RUCHU:
	// ==========================================================
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		GetCharacterMovement()->SetCrouchedHalfHeight(CrouchingCapsuleHalfHeight);
		GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
		GetCharacterMovement()->AirControl = 0.4f;
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, StandingCapsuleHalfHeight);
	}

	// ==========================================================
	// 3. DOPIERO TERAZ KONFIGURUJEMY WŁAŚCIWOŚCI KOMPONENTÓW (BEZPIECZNIE!):
	// ==========================================================
	if (HealthComp)
	{
		HealthComp->bCanReceiveInjuries = true; // Gracz jako jedyny otrzymuje urazy kości!
	}
}

void ALightkeeperCharacter::BeginPlay()
{
	Super::BeginPlay();

	// ====================================================================
	// SYNCHRONIZACJA PASKÓW Z PROGRESJĄ RPG NA STARCIE GRY:
	// ====================================================================
	if (HealthComp)
	{
		HealthComp->CurrentHealth = HealthComp->GetMaxHealth(); // Napełnia do pełnych 125/150 HP!
	}

	if (StaminaComp)
	{
		StaminaComp->Stamina = StaminaComp->GetEffectiveMaxStamina(); // Napełnia staminę do powiększonego limitu!
	}

#if !UE_BUILD_SHIPPING
	// Auto-aplikacja urazu startowego z panelu Details:
	if (HealthComp && DebugStartInjury.IsValid())
	{
		HealthComp->AddInjury(DebugStartInjury);
	}
#endif

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

	if (LandingRecoveryTimer > 0.0f)
	{
		LandingRecoveryTimer -= DeltaTime;
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		// ====================================================================
		// 1. TELEMETRIA LATARNI I NAFTY
		// ====================================================================
		if (LanternComp)
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
			// ODCZYTUJEMY EFEKTYWNĄ STAMINĘ UWZGLĘDNIAJĄCĄ URAZ KLATKI:
			float MaxStaminaVal = StaminaComp ? StaminaComp->GetEffectiveMaxStamina() : 100.0f;

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

#if !UE_BUILD_SHIPPING
	// ====================================================================
	// BŁYSKAWICZNE KLAWISZE TESTOWE (NUMPAD):
	// ====================================================================
	PlayerInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &ALightkeeperCharacter::Debug_TestLegs);
	PlayerInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &ALightkeeperCharacter::Debug_TestRightArm);
	PlayerInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &ALightkeeperCharacter::Debug_TestChest);
	PlayerInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &ALightkeeperCharacter::Debug_TestSprain);
	PlayerInputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &ALightkeeperCharacter::Debug_TestLaudanum);
	PlayerInputComponent->BindKey(EKeys::NumPadSix, IE_Pressed, this, &ALightkeeperCharacter::Debug_TestBandage);
	PlayerInputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &ALightkeeperCharacter::Debug_ClearInjuries);
#endif
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

void ALightkeeperCharacter::Jump()
{
	// 1. ANTY-BUNNYHOP: Sprawdzamy czy gracz ma siłę na skok:
	if (StaminaComp)
	{
		if (!StaminaComp->TryConsumeJumpStamina())
		{
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("⚠️ [ZADYSZKA] Zbyt mało staminy na skok!"));
#endif
			return; // Brak staminy -> blokada skoku!
		}
	}

	// 2. Skok z bonusem Precyzji:
	if (GetCharacterMovement())
	{
		float JumpMultiplier = ProgressionComp ? ProgressionComp->GetJumpBonusMultiplier() : 1.0f;
		GetCharacterMovement()->JumpZVelocity = 420.0f * JumpMultiplier;
	}

	Super::Jump();

	float StealthMod = ProgressionComp ? ProgressionComp->GetStealthNoiseMultiplier() : 1.0f;
	float JumpNoise = (bIsCrouched ? 120.0f : 280.0f) * StealthMod;

	if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
	{
		static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
		Sensory->RegisterNoise(GetActorLocation(), JumpNoise, NoiseTag);
	}
}

void ALightkeeperCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	float FallSpeed = FMath::Abs(GetVelocity().Z);
	OnCharacterLanded.Broadcast(Hit, FallSpeed);

	if (StaminaComp)
	{
		StaminaComp->HandleLanded();
	}

	// 3. AMORTYZACJA LĄDOWANIA (0.35s powrotu do pełnego sprintu):
	LandingRecoveryTimer = 0.35f;

	if (FallSpeed > 200.0f)
	{
		float LandingNoiseRadius = FMath::Clamp(FallSpeed * 1.35f, 350.0f, 1600.0f);

		if (Hit.bBlockingHit && Hit.GetActor())
		{
			AActor* FloorActor = Hit.GetActor();
			if (FloorActor->ActorHasTag(FName("Material.Metal")) || FloorActor->ActorHasTag(FName("Material.Glass")))
			{
				LandingNoiseRadius *= 1.4f;
			}
		}

		if (bIsCrouched)
		{
			LandingNoiseRadius *= 0.5f;
		}

		float StealthMod = ProgressionComp ? ProgressionComp->GetStealthNoiseMultiplier() : 1.0f;
		LandingNoiseRadius *= StealthMod;

		if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
			Sensory->RegisterNoise(GetActorLocation(), LandingNoiseRadius, NoiseTag);
		}
	}

	UpdateMovementSpeed();
}

void ALightkeeperCharacter::UpdateMovementSpeed()
{
	if (!GetCharacterMovement() || !GetCapsuleComponent()) return;

	float GeneralAgilityMod = ProgressionComp ? ProgressionComp->GetGeneralMovementSpeedMultiplier() : 1.0f;
	float CrouchMod = ProgressionComp ? ProgressionComp->GetCrouchSpeedMultiplier() : 1.0f;

	float TargetSpeed = WalkSpeed * GeneralAgilityMod;

	if (StaminaComp && StaminaComp->bIsSprinting)
	{
		TargetSpeed = SprintSpeed * GeneralAgilityMod;

		if (HealthComp)
		{
			float SprintCap = HealthComp->GetSprintSpeedCap();
			if (SprintCap > 0.0f)
			{
				TargetSpeed = FMath::Min(TargetSpeed, SprintCap);
			}
		}
	}
	else if (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < 70.0f)
	{
		TargetSpeed = CrouchSpeed * CrouchMod;
	}

	// Lekkie wyhamowanie prędkości tuż po dotknięciu ziemi (amortyzacja nóg na 0.35s):
	if (LandingRecoveryTimer > 0.0f)
	{
		TargetSpeed *= 0.75f;
	}

	static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
	if (StatusComp && StatusComp->HasStatusEffect(GuardTag))
	{
		TargetSpeed *= 0.65f;
	}

	if (HealthComp)
	{
		TargetSpeed *= HealthComp->GetMovementSpeedMultiplier();
	}

	if (InteractionComp && InteractionComp->GetGrabbedActor())
	{
		if (IPhysicalInteract::Execute_GetInteractionType(InteractionComp->GetGrabbedActor()) == EInteractionType::Grab_Free)
		{
			if (UPrimitiveComponent* HeldMesh = InteractionComp->GetGrabbedComponent())
			{
				if (HeldMesh->IsSimulatingPhysics())
				{
					TargetSpeed = InteractionComp->CalculateMovementSpeed(TargetSpeed, HeldMesh->GetMass());
				}
			}
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = TargetSpeed;
}

void ALightkeeperCharacter::Debug_TestLegs() { Debug_AddInjury(TEXT("Legs")); }
void ALightkeeperCharacter::Debug_TestRightArm() { Debug_AddInjury(TEXT("RightArm")); }
void ALightkeeperCharacter::Debug_TestChest() { Debug_AddInjury(TEXT("Chest")); }
void ALightkeeperCharacter::Debug_TestSprain() { Debug_AddInjury(TEXT("Sprain")); }

void ALightkeeperCharacter::Debug_AddInjury(FString InjuryName)
{
	if (!HealthComp)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[DEBUG] Brak HealthComponent na graczu!"));
		return;
	}

	InjuryName = InjuryName.TrimStartAndEnd();
	FGameplayTag Tag;

	// 1. Pełny tag (np. Status.Injury.Minor.LeftArm):
	if (InjuryName.StartsWith(TEXT("Status.")))
	{
		Tag = FGameplayTag::RequestGameplayTag(*InjuryName, false);
	}
	// 2. Jeśli wpisano z przedrostkiem Minor (np. MinorLeftArm, MinorChest, MinorHead):
	else if (InjuryName.StartsWith(TEXT("Minor"), ESearchCase::IgnoreCase))
	{
		FString PureName = InjuryName.RightChop(5); // Usuwa słowo "Minor"
		FString FullTagStr = FString::Printf(TEXT("Status.Injury.Minor.%s"), *PureName);
		Tag = FGameplayTag::RequestGameplayTag(*FullTagStr, false);
	}
	// 3. Drobne urazy po pojedynczych nazwach:
	else if (InjuryName.Equals(TEXT("Sprain"), ESearchCase::IgnoreCase) ||
		InjuryName.Equals(TEXT("Bleeding"), ESearchCase::IgnoreCase) ||
		InjuryName.Equals(TEXT("Burns"), ESearchCase::IgnoreCase) ||
		InjuryName.Equals(TEXT("Arrhythmia"), ESearchCase::IgnoreCase) ||
		InjuryName.Equals(TEXT("TrenchFoot"), ESearchCase::IgnoreCase) ||
		InjuryName.Equals(TEXT("SootLungs"), ESearchCase::IgnoreCase))
	{
		FString FullTagStr = FString::Printf(TEXT("Status.Injury.Minor.%s"), *InjuryName);
		Tag = FGameplayTag::RequestGameplayTag(*FullTagStr, false);
	}
	// 4. Domyślnie Major (np. Legs, RightArm, LeftArm, Chest, Head):
	else
	{
		FString FullTagStr = FString::Printf(TEXT("Status.Injury.Major.%s"), *InjuryName);
		Tag = FGameplayTag::RequestGameplayTag(*FullTagStr, false);
	}

	// WERYFIKACJA I DIAGNOSTYKA:
	if (!Tag.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
				FString::Printf(TEXT("⚠️ [DEBUG] Nie znaleziono tagu dla nazwy: '%s'!"), *InjuryName));
		}
		return;
	}

	if (HealthComp->ActiveInjuries.HasTagExact(Tag))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("ℹ️ [DEBUG] Gracz JUŻ MA ten uraz: %s! (Wciśnij NumPad 0 aby zresetować)"), *Tag.ToString()));
		}
		return;
	}

	HealthComp->AddInjury(Tag);
	UpdateMovementSpeed();
}

void ALightkeeperCharacter::Debug_ClearInjuries()
{
	if (HealthComp)
	{
		HealthComp->ActiveInjuries.Reset();
		UpdateMovementSpeed();
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, TEXT("[DEBUG] Usunięto wszystkie urazy!"));
	}
}

void ALightkeeperCharacter::Debug_TestLaudanum()
{
	if (HealthComp) HealthComp->UseLaudanum(10.0f); // 10 sekund do szybkiego testu crasha!
}

void ALightkeeperCharacter::Debug_TestBandage()
{
	if (HealthComp) HealthComp->UseBandage(35.0f);
}