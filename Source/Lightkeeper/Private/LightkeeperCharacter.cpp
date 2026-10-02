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
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"

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
	// TWORZENIE POSTPROCESSU ADAPTACJI OCZU:
	// ==========================================================
	DarknessAdaptationPP = CreateDefaultSubobject<UPostProcessComponent>(TEXT("DarknessAdaptationPP"));
	DarknessAdaptationPP->bUnbound = true; // Działa na całą kamerę gracza
	DarknessAdaptationPP->BlendWeight = 0.0f; // Domyślnie 0% (wzrok niezaadaptowany)
	DarknessAdaptationPP->SetupAttachment(RootComponent);

	// ==========================================================
	// SŁABE ŚWIATŁO ŹRENICY - ZŁOTY ŚRODEK:
	// ==========================================================
	EyeAdaptationLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeAdaptationLight"));
	EyeAdaptationLight->SetupAttachment(RootComponent);
	EyeAdaptationLight->SetCastShadows(false); // Zero cieni (ultra tanie)

	// 1. KLUCZ: Przesuwamy światło na wysokość OCZU i lekko przed gracza!
	// (Dzięki temu światło nie ginie w podłodze i nie zasłania go własna klatka piersiowa):
	EyeAdaptationLight->SetRelativeLocation(FVector(25.0f, 0.0f, 65.0f));

	// 2. ŁAGODNY SPADEK RETRO (Wyłączamy fizyczny kwadrat odległości):
	// Daje to równomierne, delikatne muśnięcie ścian bez jaskrawej plamy na środku:
	EyeAdaptationLight->bUseInverseSquaredFalloff = false;
	EyeAdaptationLight->SetLightFalloffExponent(1.9f); // Bardzo miękki, naturalny spadek

	EyeAdaptationLight->SetAttenuationRadius(600.0f);  // Zasięg na 9 metrów (zamiast 6)
	EyeAdaptationLight->SetIntensity(0.0f);
	EyeAdaptationLight->SetLightColor(FLinearColor(0.6f, 0.75f, 1.0f)); // Zimny, noktowizyjny odcień

	// ==========================================================
	// 2. KONFIGURACJA POSTACI I SILNIKA RUCHU:
	// ==========================================================
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		GetCharacterMovement()->SetCrouchedHalfHeight(CrouchingCapsuleHalfHeight);
		GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
		GetCharacterMovement()->AirControl = 0.4f;
		GetCharacterMovement()->JumpZVelocity = JumpHeight;
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
		GetCharacterMovement()->JumpZVelocity = JumpHeight;
	}
}

void ALightkeeperCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Płynna aktualizacje prędkości co klatkę (działa zawsze i wszędzie!):
	UpdateMovementSpeed();

	UpdateEyeAdaptation(DeltaTime);

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
			FString StateStr;
			FColor StateColor;

			switch (SanityComp->CurrentIllumination)
			{
			case EIlluminationState::DirectLight:
				StateStr = TEXT("ŚWIATŁO (Regeneracja)");
				StateColor = FColor::Cyan;
				break;

			case EIlluminationState::Penumbra:
				StateStr = FString::Printf(TEXT("PÓŁMROK (Bezpieczny | Bufor: %.1fs)"), SanityComp->TimeInDarkness);
				StateColor = FColor::Yellow; // Żółty kolor dla Półmroku!
				break;

			case EIlluminationState::Darkness:
				StateStr = FString::Printf(TEXT("GŁĘBOKI MROK (Drenaż | Czas: %.1fs)"), SanityComp->TimeInDarkness);
				StateColor = FColor(255, 50, 100); // Czerwony/Magenta dla Mroku!
				break;
			}

			float DynamicCapVal = SanityComp->GetCurrentDynamicComfortCap();
			float DynamicCapPercent = (DynamicCapVal / FMath::Max(1.0f, SanityComp->GetMaxSanity())) * 100.0f;
			FString MinorMadnessStr = SanityComp->bHasMinorMadness ? TEXT("AKTYWNE (Drain x1.5!)") : TEXT("Brak");

			GEngine->AddOnScreenDebugMessage(
				102, 0.0f, StateColor,
				FString::Printf(TEXT("[SANITY] %.1f / %.1f (Sufit Światła: %.0f%%) | Stan: %s | Trwały Cap: %.0f%% | Szaleństwo: %s | Zapaści: %d/3"),
					SanityComp->CurrentSanity,
					DynamicCapVal, // <--- Pokazuje faktyczny cel regeneracji!
					DynamicCapPercent, // <--- Pokazuje ile % max możesz teraz odzyskać!
					*StateStr,
					SanityComp->MaxSanityCapMultiplier * 100.0f,
					*MinorMadnessStr,
					SanityComp->MentalCollapseCount)
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
		//JumpHeight = JumpHeight * JumpMultiplier;
		GetCharacterMovement()->JumpZVelocity = JumpHeight * JumpMultiplier;
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

void ALightkeeperCharacter::UpdateEyeAdaptation(float DeltaTime)
{
	if (!DarknessAdaptationPP || !SanityComp) return;

	float TargetWeight = 0.0f;
	float InterpSpeed = LightInterpSpeed;

	switch (SanityComp->CurrentIllumination)
	{
	case EIlluminationState::DirectLight:
		// Pełne światło -> 0% adaptacji:
		TargetWeight = 0.0f;
		InterpSpeed = LightInterpSpeed;
		break;

	case EIlluminationState::Penumbra:
		// PÓŁMROK: Oko adaptuje się tylko MINIMALNIE (max 15%)!
		// Krawędzie są widoczne, ale ściany i niebo NIGDY się nie przepalą!
		TargetWeight = 0.15f;
		InterpSpeed = 1.0f;
		break;

	case EIlluminationState::Darkness:
		// GŁĘBOKI MROK: Pełna adaptacja Amnesii po odczekaniu czasu:
		if (SanityComp->TimeInDarkness > EyeAdaptationDelay)
		{
			float Range = FMath::Max(0.1f, EyeAdaptationFullTime - EyeAdaptationDelay);
			TargetWeight = FMath::Clamp((SanityComp->TimeInDarkness - EyeAdaptationDelay) / Range, 0.0f, 1.0f);
			InterpSpeed = DarkInterpSpeed;
		}
		break;
	}

	CurrentEyeAdaptationWeight = FMath::FInterpTo(CurrentEyeAdaptationWeight, TargetWeight, DeltaTime, InterpSpeed);
	DarknessAdaptationPP->BlendWeight = CurrentEyeAdaptationWeight;

	if (EyeAdaptationLight)
	{
		EyeAdaptationLight->SetIntensity(CurrentEyeAdaptationWeight * MaxEyeLightIntensity);
	}
}

bool ALightkeeperCharacter::CanStandUp() const
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule || !GetWorld()) return false;

	const float CurrentHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// Jeśli już stoimy wyprostowani, nie ma czego blokować:
	if (CurrentHalfHeight >= (StandingCapsuleHalfHeight - 2.0f))
	{
		return true;
	}

	// 1. Wyznaczamy pozycję stóp na ziemi:
	const FVector ActorLoc = GetActorLocation();
	const FVector FeetLocation = ActorLoc - FVector(0.0f, 0.0f, CurrentHalfHeight);

	// 2. Promień testowej sfery głowy (lekko węższy niż gracz, żeby nie haczyć o framugi):
	const float TestRadius = FMath::Max(10.0f, CapsuleRadius - 5.0f);

	// Start: Na obecnej wysokości czubka głowy w kuckach (bezpiecznie NAD podłogą!):
	const float CurrentHeadZ = FeetLocation.Z + (CurrentHalfHeight * 2.0f) - TestRadius;
	const FVector SweepStart = FVector(ActorLoc.X, ActorLoc.Y, CurrentHeadZ);

	// Cel: Wysokość głowy po pełnym wyprostowaniu się:
	const float StandingHeadZ = FeetLocation.Z + (StandingCapsuleHalfHeight * 2.0f) - TestRadius;
	const FVector SweepEnd = FVector(ActorLoc.X, ActorLoc.Y, StandingHeadZ);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// Ignorujemy trzymany obiekt:
	if (InteractionComp && InteractionComp->GetGrabbedActor())
	{
		Params.AddIgnoredActor(InteractionComp->GetGrabbedActor());
	}

	FHitResult Hit;
	// Skanujemy sferą wyłącznie przestrzeń pionowo NAD głową:
	bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(TestRadius),
		Params
	);

#if !UE_BUILD_SHIPPING
	// WIZUALNY DEBUG (Widoczny w PIE, jeśli coś zablokuje):
	if (bHit)
	{
		// Czerwona kropka w miejscu, gdzie sufit dotknął głowy:
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Red, false, 0.2f);
	}
#endif

	// Jeśli nad głową NIE MA sufitu -> zwracamy TRUE (można wstać!):
	return !bHit;
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