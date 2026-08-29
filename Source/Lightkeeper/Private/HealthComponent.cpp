#include "HealthComponent.h"
#include "LightkeeperCharacter.h"
#include "StaminaComponent.h"
#include "StatusEffectComponent.h"
#include "SensoryComponent.h"
#include "ReactionReceiverComponent.h"
#include "ImSimSensorySubsystem.h"
#include "InventoryTypes.h"
#include "ToolManagerComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = GetMaxHealth();
	FractureMeter = 0.0f;
	NightFatigueFloor = 0.0f;
	ActiveInjuries.Reset();
	LimbTraumaHistory.Empty();
	bIsPalliativeActive = false;
	BrokenLegRunTimer = 0.0f;
	LowStaminaChestTimer = 0.0f;

	RebuildCachedModifiers();

	if (ALightkeeperCharacter* OwnerChar = Cast<ALightkeeperCharacter>(GetOwner()))
	{
		OwnerChar->OnCharacterLanded.AddDynamic(this, &UHealthComponent::HandleOwnerLanded);

		if (UStaminaComponent* StaminaComp = OwnerChar->FindComponentByClass<UStaminaComponent>())
		{
			StaminaComp->OnSprintTick.AddDynamic(this, &UHealthComponent::HandleOwnerSprintTick);
			StaminaComp->OnLowStaminaTick.AddDynamic(this, &UHealthComponent::HandleOwnerLowStaminaTick);
		}
	}
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PalliativeTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

const FInjuryDataRow* UHealthComponent::FindInjuryDataRow(const FGameplayTag& InjuryTag) const
{
	if (!InjuryDataTable || !InjuryTag.IsValid()) return nullptr;

	const FName RowName = InjuryTag.GetTagName();
	static const FString ContextString(TEXT("InjuryLookupContext"));
	if (const FInjuryDataRow* FastRow = InjuryDataTable->FindRow<FInjuryDataRow>(RowName, ContextString))
	{
		return FastRow;
	}

	TArray<FInjuryDataRow*> AllRows;
	InjuryDataTable->GetAllRows<FInjuryDataRow>(ContextString, AllRows);

	for (const FInjuryDataRow* Row : AllRows)
	{
		if (Row && InjuryTag.MatchesTag(Row->InjuryTag))
		{
			return Row;
		}
	}

	return nullptr;
}

// ====================================================================
// PRZELICZANIE BUFORA MODYFIKATORÓW (Wywoływane TYLKO przy zmianie ran!):
// ====================================================================
void UHealthComponent::RebuildCachedModifiers()
{
	CachedWalkSpeedMultiplier = 1.0f;
	CachedSprintSpeedCap = 0.0f;
	CachedMeleeDamageMultiplier = 1.0f;
	CachedMaxStaminaMultiplier = 1.0f;
	CachedStaminaDrainMultiplier = 1.0f;
	CachedThrowPowerMultiplier = 1.0f;
	CachedMouseResistanceMultiplier = 1.0f;
	CachedChargedThrowPain = 0.0f;
	CachedGuardAbsorptionMultiplier = 1.0f;

	if (ActiveInjuries.IsEmpty() || !InjuryDataTable) return;

	for (const FGameplayTag& ActiveTag : ActiveInjuries)
	{
		if (const FInjuryDataRow* Row = FindInjuryDataRow(ActiveTag))
		{
			CachedWalkSpeedMultiplier *= Row->WalkSpeedMultiplier;
			CachedMeleeDamageMultiplier *= Row->MeleeDamageMultiplier;
			CachedMaxStaminaMultiplier *= Row->MaxStaminaMultiplier;
			CachedStaminaDrainMultiplier *= Row->StaminaDrainMultiplier;
			CachedThrowPowerMultiplier *= Row->ThrowPowerMultiplier;
			CachedMouseResistanceMultiplier *= Row->MouseResistanceMultiplier;
			CachedChargedThrowPain += Row->ChargedThrowPainCost;
			CachedGuardAbsorptionMultiplier *= Row->GuardAbsorptionMultiplier;

			if (Row->SprintSpeedCap > 0.0f)
			{
				CachedSprintSpeedCap = (CachedSprintSpeedCap == 0.0f) ? Row->SprintSpeedCap : FMath::Min(CachedSprintSpeedCap, Row->SprintSpeedCap);
			}
		}
	}

	CachedWalkSpeedMultiplier = FMath::Clamp(CachedWalkSpeedMultiplier, 0.2f, 1.0f);
	CachedMeleeDamageMultiplier = FMath::Clamp(CachedMeleeDamageMultiplier, 0.1f, 2.0f);
	CachedMaxStaminaMultiplier = FMath::Clamp(CachedMaxStaminaMultiplier, 0.25f, 1.0f);
	CachedStaminaDrainMultiplier = FMath::Clamp(CachedStaminaDrainMultiplier, 1.0f, 3.0f);
	CachedThrowPowerMultiplier = FMath::Clamp(CachedThrowPowerMultiplier, 0.2f, 1.0f);
	CachedMouseResistanceMultiplier = FMath::Clamp(CachedMouseResistanceMultiplier, 1.0f, 3.0f);
}

// ====================================================================
// BŁYSKAWICZNE GETTERY O(1) - KOSZT 0 NANOSEKUND W TICKU!
// ====================================================================
float UHealthComponent::GetMovementSpeedMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedWalkSpeedMultiplier; }
float UHealthComponent::GetSprintSpeedCap() const { return bIsPalliativeActive ? 0.0f : CachedSprintSpeedCap; }
float UHealthComponent::GetMeleeDamageMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedMeleeDamageMultiplier; }
float UHealthComponent::GetMaxStaminaMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedMaxStaminaMultiplier; }
float UHealthComponent::GetStaminaDrainMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedStaminaDrainMultiplier; }
float UHealthComponent::GetThrowPowerMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedThrowPowerMultiplier; }
float UHealthComponent::GetMouseResistanceMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedMouseResistanceMultiplier; }
float UHealthComponent::GetChargedThrowPainCost() const { return bIsPalliativeActive ? 0.0f : CachedChargedThrowPain; }
float UHealthComponent::GetGuardAbsorptionMultiplier() const { return bIsPalliativeActive ? 1.0f : CachedGuardAbsorptionMultiplier; }

bool UHealthComponent::IsActionAllowed(EPlayerAction Action) const
{
	if (bIsPalliativeActive) return true;

	for (const FGameplayTag& ActiveTag : ActiveInjuries)
	{
		if (const FInjuryDataRow* Row = FindInjuryDataRow(ActiveTag))
		{
			if (Action == EPlayerAction::QuickMelee && Row->bBlocksQuickMelee) return false;
			if (Action == EPlayerAction::HoldBreath && Row->bBlocksHoldBreath) return false;
		}
	}
	return true;
}

float UHealthComponent::GetActionPainCost(EAnatomicalLimb Limb) const
{
	if (bIsPalliativeActive) return 0.0f;

	float TotalPain = 0.0f;
	for (const FGameplayTag& ActiveTag : ActiveInjuries)
	{
		if (const FInjuryDataRow* Row = FindInjuryDataRow(ActiveTag))
		{
			if (Row->AffectedLimb == Limb || Row->AffectedLimb == EAnatomicalLimb::None)
			{
				TotalPain += Row->ActionPainCost;
			}
		}
	}
	return TotalPain;
}

void UHealthComponent::HandleOwnerLanded(const FHitResult& Hit, float FallSpeed)
{
	if (bIsPalliativeActive) return;

	// ====================================================================
	// 1. ZWYKŁY FALL DAMAGE (Bezpieczny próg: 650 cm/s = ok. 2 metrów):
	// ====================================================================
	const float SafeFallSpeed = 650.0f;

	if (FallSpeed > SafeFallSpeed)
	{
		float ExcessSpeed = FallSpeed - SafeFallSpeed;
		float CalculatedFallDamage = ExcessSpeed * 0.075f;

		static const FGameplayTag FallDamageTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Fall"), false);

		// TakeDamage sam zadba o złamanie nóg (przy >= 40 HP) lub skręcenie kostki (przy < 40 HP)!
		TakeDamage(CalculatedFallDamage, FallDamageTag, Hit);
	}

	// ====================================================================
	// 2. DODATKOWY BÓL ZE ZŁAMANEJ KOŚCI (Z tabeli DT_Injuries):
	// ====================================================================
	float TotalLandingPain = 0.0f;
	for (const FGameplayTag& ActiveTag : ActiveInjuries)
	{
		if (const FInjuryDataRow* Row = FindInjuryDataRow(ActiveTag))
		{
			TotalLandingPain += Row->LandingPainDamage;
		}
	}

	if (TotalLandingPain > 0.0f && FallSpeed > 600.0f)
	{
		TakeDamage(TotalLandingPain, FGameplayTag());
#if !UE_BUILD_SHIPPING
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red,
			FString::Printf(TEXT("⚠️ [BÓL] Zeskok ze złamaną kością! -%.1f HP"), TotalLandingPain));
#endif
	}
}

void UHealthComponent::HandleOwnerSprintTick(float DeltaTime)
{
	if (bIsPalliativeActive) return;

	float TotalSprintPain = 0.0f;
	for (const FGameplayTag& ActiveTag : ActiveInjuries)
	{
		if (const FInjuryDataRow* Row = FindInjuryDataRow(ActiveTag))
		{
			TotalSprintPain += Row->SprintPainPerSecond;
		}
	}

	if (TotalSprintPain > 0.0f)
	{
		BrokenLegRunTimer += DeltaTime;
		if (BrokenLegRunTimer >= 1.0f)
		{
			BrokenLegRunTimer = 0.0f;
			TakeDamage(TotalSprintPain, FGameplayTag());
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.9f, FColor::Red,
				FString::Printf(TEXT("⚠️ [BÓL] Bieg na złamanej kości! -%.1f HP"), TotalSprintPain));
#endif
		}
	}
}

void UHealthComponent::HandleOwnerLowStaminaTick(float DeltaTime)
{
	if (bIsPalliativeActive) return;

	float ChestPain = GetActionPainCost(EAnatomicalLimb::Chest);
	if (ChestPain > 0.0f)
	{
		LowStaminaChestTimer += DeltaTime;
		if (LowStaminaChestTimer >= 1.0f)
		{
			LowStaminaChestTimer = 0.0f;
			TakeDamage(ChestPain, FGameplayTag());
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
				FString::Printf(TEXT("🫁 [ODMA PŁUCNA] Skrajne wyczerpanie przy uszkodzonej klatce! -%.1f HP"), ChestPain));
#endif
		}
	}
	else
	{
		LowStaminaChestTimer = 0.0f;
	}

	static const FGameplayTag ArrhythmiaTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Arrhythmia"), false);
	if (ActiveInjuries.HasTag(ArrhythmiaTag))
	{
		if (AActor* Owner = GetOwner())
		{
			if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
			{
				if (!StatusComp->IsStunned())
				{
					StatusComp->ApplyStun(1.5f);
				}
			}
		}
	}
}

void UHealthComponent::TakeDamage(float DamageAmount, FGameplayTag DamageTypeTag, const FHitResult& HitInfo)
{
	if (!bCanBeDestroyed || DamageAmount <= 0.0f || IsDead()) return;

	if (!AllowedDamageTypes.IsEmpty() && DamageTypeTag.IsValid())
	{
		if (!DamageTypeTag.MatchesAny(AllowedDamageTypes)) return;
	}

	static const FGameplayTag StealthTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.StealthTakedown"), false);
	if (DamageTypeTag.MatchesTag(StealthTag))
	{
		bool bWasUnaware = true;

		// Jeśli to potwór, sprawdzamy jego zmysły:
		if (USensoryComponent* Sensory = GetOwner()->FindComponentByClass<USensoryComponent>())
		{
			// Jeśli ofiara była w trakcie ataku/pościgu -> Takedown jest zanegowany!
			if (Sensory->CurrentState == EAIBehaviorState::Chase_Attack)
			{
				bWasUnaware = false;
			}
		}

		if (bWasUnaware)
		{
			// Jeśli wróg ma dodany tag "Odporny na Takedown" w edytorze:
			static const FGameplayTag BossImmunity = FGameplayTag::RequestGameplayTag(FName("Status.Immunity.Takedown"), false);
			if (Immunities.HasTag(BossImmunity))
			{
				// ODPORNY WRÓG: Przeżywa, ale obrywa x1.5 mocniej i dostaje Stun na 1.5s!
				DamageAmount *= 1.5f;
				if (UStatusEffectComponent* Status = GetOwner()->FindComponentByClass<UStatusEffectComponent>())
				{
					Status->ApplyStun(1.5f);
				}

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("⚠️ [TAKEDOWN] Elitarny wróg przeżył cios, ale został potężnie ogłuszony!"));
#endif
			}
			else
			{
				// ZWYKŁY WRÓG: Natychmiastowa śmierć! Obrażenia = x2 Max HP
				DamageAmount = GetMaxHealth() * 2.0f;

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Magenta, TEXT("🥷 [TAKEDOWN] Bezgłośny, zabójczy cios od tyłu!"));
#endif
			}
		}
	}

	if (!Immunities.IsEmpty() && DamageTypeTag.IsValid() && DamageTypeTag.MatchesAny(Immunities)) return;

	bool bIsVulnerable = !Vulnerabilities.IsEmpty() && DamageTypeTag.IsValid() && DamageTypeTag.MatchesAny(Vulnerabilities);
	bool bIsResistant = !Resistances.IsEmpty() && DamageTypeTag.IsValid() && DamageTypeTag.MatchesAny(Resistances);

	float ProfileMultiplier = 1.0f;
	if (bIsVulnerable) ProfileMultiplier *= VulnerabilityMultiplier;
	else if (bIsResistant) ProfileMultiplier *= ResistanceMultiplier;

	static const FGameplayTag BurnsTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Burns"), false);
	if (HasActiveInjury(BurnsTag) && DamageTypeTag.ToString().Contains(TEXT("Damage.Type")))
	{
		ProfileMultiplier *= 1.20f;
	}

	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"), false);
	static const FGameplayTag OiledTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Neutral.Oiled"), false);
	if (DamageTypeTag.MatchesTag(FireTag))
	{
		if (AActor* Owner = GetOwner())
		{
			if (UReactionReceiverComponent* Receiver = Owner->FindComponentByClass<UReactionReceiverComponent>())
			{
				if (Receiver->HasState(OiledTag)) ProfileMultiplier *= 1.50f;
			}
		}
	}

	float FinalCalculatedDamage = DamageAmount * DamageSusceptibility * CustomDamageMultiplier * ProfileMultiplier;

	// ====================================================================
	// SYSTEM GARDY (W 100% CZYSTE PULL API Z TABELI DT_Injuries):
	// ====================================================================
	static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
	UStatusEffectComponent* StatusComp = GetOwner()->FindComponentByClass<UStatusEffectComponent>();

	if (StatusComp && StatusComp->HasStatusEffect(GuardTag) && DamageTypeTag.ToString().Contains(TEXT("Damage.Type")))
	{
		float Absorption = 0.35f; // Domyślna garda pięściami (-35% DMG)
		float StaminaCostMod = 1.0f;

		// 1. Sprawdzamy broń w dłoni (Łom / Kostur):
		if (UToolManagerComponent* ToolMgr = GetOwner()->FindComponentByClass<UToolManagerComponent>())
		{
			if (ToolMgr->CurrentEquippedTool)
			{
				const FInventoryItemData& ToolData = ToolMgr->CurrentEquippedTool->ToolItemData;
				if (ToolData.GuardType != EGuardBlockType::FistGuard)
				{
					Absorption = ToolData.GuardDamageAbsorption;
					StaminaCostMod = ToolData.GuardStaminaCostMultiplier;
				}
			}
		}

		// 2. OSŁABIENIE GARDY PRZEZ URAZY RĄK (PULL API z kolumny GuardAbsorptionMultiplier):
		Absorption *= GetGuardAbsorptionMultiplier();

		// 3. KOSZT BÓLU KOŚCI (PULL API z kolumny ActionPainCost dla prawej i lewej dłoni):
		float GuardPain = GetActionPainCost(EAnatomicalLimb::RightArm) + GetActionPainCost(EAnatomicalLimb::LeftArm);
		if (GuardPain > 0.0f)
		{
			CurrentHealth = FMath::Clamp(CurrentHealth - GuardPain, 0.0f, GetMaxHealth());
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red,
				FString::Printf(TEXT("⚠️ [BÓL] Cios w uszkodzone kończyny podczas gardy! -%.1f HP"), GuardPain));
#endif
		}

		// 4. MNOŻNIK DRENAŻU STAMINY (PULL API z kolumny StaminaDrainMultiplier):
		StaminaCostMod *= GetStaminaDrainMultiplier();

		float AbsorbedDamage = FinalCalculatedDamage * Absorption;
		FinalCalculatedDamage -= AbsorbedDamage; // Redukujemy obrażenia HP!

		// 5. SIŁA CIOSU ZABIERA STAMINĘ:
		if (UStaminaComponent* StaminaComp = GetOwner()->FindComponentByClass<UStaminaComponent>())
		{
			float StaminaLoss = AbsorbedDamage * 0.85f * StaminaCostMod;

			// GUARD BREAK (Złamanie gardy przy wyzerowaniu staminy):
			if (!StaminaComp->TryConsumeStamina(StaminaLoss))
			{
				StaminaComp->Stamina = 0.0f;
				StatusComp->RemoveStatusEffect(GuardTag); // Zdejmujemy gardę!
				StatusComp->ApplyStun(1.3f);             // Ogłuszenie na 1.3s!

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("💥 [GUARD BREAK] Garda złamana! Ogłuszenie!"));
#endif
			}
			else
			{
#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan,
					FString::Printf(TEXT("🛡️ [BLOK] Zablokowano %.0f%% obrażeń! (-%.0f Staminy)"), Absorption * 100.0f, StaminaLoss));
#endif
			}
		}
	}

	bool bBypassesArmor = bIsVulnerable && bVulnerabilitiesBypassThreshold;
	if (!bBypassesArmor && FinalCalculatedDamage < DamageThreshold) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - FinalCalculatedDamage, 0.0f, GetMaxHealth());
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());

	// Hałas krzyku bólu dla AI:
	if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
	{
		static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
		float PainNoiseRadius = FMath::Clamp(FinalCalculatedDamage * 35.0f, 250.0f, 1800.0f);
		Sensory->RegisterNoise(GetOwner()->GetActorLocation(), PainNoiseRadius, NoiseTag);
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		FString TypeStr = DamageTypeTag.IsValid() ? DamageTypeTag.ToString() : TEXT("Fizyczne");
		FColor MsgColor = bIsVulnerable ? FColor::Red : (bIsResistant ? FColor::Blue : FColor::Orange);
		FString ProfileStr = bIsVulnerable ? TEXT(" [SŁABOŚĆ!]") : (bIsResistant ? TEXT(" [ODPORNOŚĆ!]") : TEXT(""));

		GEngine->AddOnScreenDebugMessage(-1, 3.0f, MsgColor,
			FString::Printf(TEXT("[HP] %s | -%.1f HP | Stan: %.1f / %.1f HP | Typ: %s%s"),
				*GetOwner()->GetName(), FinalCalculatedDamage, CurrentHealth, GetMaxHealth(), *TypeStr, *ProfileStr));
	}
#endif

	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	static const FGameplayTag BurnStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	static const FGameplayTag CorrodeStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Corroding"), false);
	static const FGameplayTag ShockStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);

	bool bIsContinuousDoT = DamageTypeTag.MatchesTag(BleedTag) || DamageTypeTag.MatchesTag(BurnStatus) || DamageTypeTag.MatchesTag(CorrodeStatus) || DamageTypeTag.MatchesTag(ShockStatus);

	if (bCanReceiveInjuries && !bIsContinuousDoT && FinalCalculatedDamage >= 5.0f)
	{
		FractureMeter = FMath::Clamp(FractureMeter + FractureIncreasePerHit, NightFatigueFloor, FractureCap);
		ProcessDamageAndWounds(FinalCalculatedDamage, DamageTypeTag, HitInfo);
	}

	if (CurrentHealth <= 0.0f)
	{
		OnDeath.Broadcast();
	}
}

void UHealthComponent::ProcessDamageAndWounds(float DamageAmount, FGameplayTag DamageTypeTag, const FHitResult& HitInfo)
{
	static const FGameplayTag FallTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Fall"), false);

	EAnatomicalLimb TargetLimb = EAnatomicalLimb::Chest; // Domyślnie tors

	// ====================================================================
	// 1. DETEKCJA KIERUNKOWA Z-AXIS I WEKTORA LOKALNEGO:
	// ====================================================================
	// Upadek z wysokości ZAWSZE uderza w nogi:
	if (DamageTypeTag.MatchesTag(FallTag))
	{
		TargetLimb = EAnatomicalLimb::Legs;
	}
	else if (HitInfo.bBlockingHit && GetOwner())
	{
		FVector LocalHit = GetOwner()->GetTransform().InverseTransformPosition(HitInfo.ImpactPoint);

		if (LocalHit.Z > 45.0f)
		{
			TargetLimb = EAnatomicalLimb::Head;
		}
		else if (LocalHit.Z < -20.0f)
		{
			TargetLimb = EAnatomicalLimb::Legs;
		}
		else
		{
			if (LocalHit.Y < -20.0f)      TargetLimb = EAnatomicalLimb::LeftArm;
			else if (LocalHit.Y > 20.0f) TargetLimb = EAnatomicalLimb::RightArm;
			else                         TargetLimb = EAnatomicalLimb::Chest;
		}
	}

	// Przekazujemy do weryfikacji urazów (Cios >= 40 HP ma gwarantowane wejście!):
	TestFractureInjury(DamageTypeTag, TargetLimb, DamageAmount);
}

void UHealthComponent::TestFractureInjury(FGameplayTag DamageTypeTag, EAnatomicalLimb HitLimb, float DamageAmount)
{
	static const FGameplayTag FallTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Fall"), false);
	static const FGameplayTag SlashTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Slashing"), false);
	static const FGameplayTag BluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
	static const FGameplayTag ExplosiveTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Explosive"), false);

	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"), false);
	static const FGameplayTag BurningTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
	static const FGameplayTag ElectricityTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Electricity"), false);
	static const FGameplayTag ElectrocutedTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
	static const FGameplayTag WaterTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Moisture.Water"), false);

	// 1. Sprawdzamy stan kończyny (Maksymalnie 1 Major + 1 Minor):
	bool bHasMajorOnLimb = false;
	bool bHasMinorOnLimb = false;

	for (const FGameplayTag& ActiveTag : ActiveInjuries)
	{
		if (const FInjuryDataRow* Row = FindInjuryDataRow(ActiveTag))
		{
			if (Row->AffectedLimb == HitLimb && HitLimb != EAnatomicalLimb::None)
			{
				if (ActiveTag.ToString().Contains(TEXT("Major"))) bHasMajorOnLimb = true;
				if (ActiveTag.ToString().Contains(TEXT("Minor"))) bHasMinorOnLimb = true;
			}
		}
	}

	if (bHasMajorOnLimb && bHasMinorOnLimb) return; // Kończyna maksymalnie zmasakrowana

	// ====================================================================
	// 2. GWARANTOWANY KRYTYCZNY MAJOR DLA CIOSÓW >= 40 HP (BEZ ŻADNEGO RZUTU RNG!):
	// ====================================================================
	if (DamageAmount >= 40.0f && !bHasMajorOnLimb)
	{
		FGameplayTag GuaranteedMajor;
		switch (HitLimb)
		{
		case EAnatomicalLimb::Head:     GuaranteedMajor = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.Head")); break;
		case EAnatomicalLimb::Chest:    GuaranteedMajor = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.Chest")); break;
		case EAnatomicalLimb::RightArm: GuaranteedMajor = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.RightArm")); break;
		case EAnatomicalLimb::LeftArm:  GuaranteedMajor = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.LeftArm")); break;
		case EAnatomicalLimb::Legs:     GuaranteedMajor = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.Legs")); break;
		default: break;
		}

		if (GuaranteedMajor.IsValid())
		{
			AddInjury(GuaranteedMajor, HitLimb);
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Red,
				FString::Printf(TEXT("💥 [KRYTYCZNE TRAFIENIE] Potężny cios (%.1f HP) natychmiast strzaskał kończynę!"), DamageAmount));
#endif
			return; // Gwarantowany Major wbity w 100%!
		}
	}

	// ====================================================================
	// 3. DOPIERO DLA LŻEJSZYCH CIOSÓW (< 40 HP) RZUCAMY KOŚCIĄ Z PASEKA PĘKNIĘCIA:
	// ====================================================================
	float Roll = FMath::FRand();
	if (Roll > FractureMeter) return; // Uniknięcie rany przy małym ciosie!

	FGameplayTag SelectedInjury;

	if (DamageTypeTag.MatchesTag(FireTag) || DamageTypeTag.MatchesTag(BurningTag))
	{
		SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Burns"));
	}
	else if (DamageTypeTag.MatchesTag(ElectricityTag) || DamageTypeTag.MatchesTag(ElectrocutedTag))
	{
		SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Arrhythmia"));
	}
	else if (DamageTypeTag.MatchesTag(WaterTag))
	{
		SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.TrenchFoot"));
	}
	else
	{
		switch (HitLimb)
		{
		case EAnatomicalLimb::Legs:
			if (DamageTypeTag.MatchesTag(FallTag)) SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Sprain"));
			else SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"));
			break;

		case EAnatomicalLimb::RightArm:
			if (DamageTypeTag.MatchesTag(SlashTag)) SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"));
			else SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.RightArm"));
			break;

		case EAnatomicalLimb::LeftArm:
			SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.LeftArm"));
			break;

		case EAnatomicalLimb::Chest:
			if (DamageTypeTag.MatchesTag(BluntTag)) SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Chest"));
			else SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.Chest"));
			break;

		case EAnatomicalLimb::Head:
			SelectedInjury = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Head"));
			break;
		}
	}

	int32 CurrentLimbTrauma = LimbTraumaHistory.Contains(HitLimb) ? LimbTraumaHistory[HitLimb] : 0;
	bool bTissueExhausted = (CurrentLimbTrauma >= 2);

	if (SelectedInjury.IsValid())
	{
		// A. Jeśli wylosowano Minor, ale tkanka jest skrajnie osłabiona (>= 2 rany) -> SKOK DO MAJOR:
		if (SelectedInjury.ToString().Contains(TEXT("Minor")) && bTissueExhausted && !bHasMajorOnLimb)
		{
			if (FMath::FRand() < 0.70f) // 70% szansy na pęknięcie zmęczonej kości!
			{
				FString EvoName = SelectedInjury.ToString().Replace(TEXT("Minor"), TEXT("Major"));
				SelectedInjury = FGameplayTag::RequestGameplayTag(*EvoName, false);
#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Red,
					TEXT("🦴 [ZMĘCZENIE TKANKI] Wielokrotnie opatrywana kończyna nie wytrzymała i pękła!"));
#endif
			}
		}

		// B. Standardowa blokada duplikatów:
		if (SelectedInjury.ToString().Contains(TEXT("Major")) && bHasMajorOnLimb) return;

		if (SelectedInjury.ToString().Contains(TEXT("Minor")) && ActiveInjuries.HasTagExact(SelectedInjury))
		{
			if (!bHasMajorOnLimb)
			{
				RemoveInjury(SelectedInjury);
				FString EvoName = SelectedInjury.ToString().Replace(TEXT("Minor"), TEXT("Major"));
				SelectedInjury = FGameplayTag::RequestGameplayTag(*EvoName, false);
			}
			else return;
		}

		if (SelectedInjury.IsValid())
		{
			AddInjury(SelectedInjury, HitLimb);
		}
	}

	NightFatigueFloor = FMath::Clamp(NightFatigueFloor + 0.05f, 0.0f, 0.25f);
	FractureMeter = NightFatigueFloor;
}

void UHealthComponent::AddInjury(FGameplayTag InjuryTag, EAnatomicalLimb Limb)
{
	if (!bCanReceiveInjuries || !InjuryTag.IsValid() || ActiveInjuries.HasTagExact(InjuryTag)) return;

	ActiveInjuries.AddTag(InjuryTag);

	EAnatomicalLimb FinalLimb = Limb;
	FString TagStr = InjuryTag.ToString();
	if (TagStr.Contains(TEXT("Legs")) || TagStr.Contains(TEXT("Sprain"))) FinalLimb = EAnatomicalLimb::Legs;
	else if (TagStr.Contains(TEXT("RightArm"))) FinalLimb = EAnatomicalLimb::RightArm;
	else if (TagStr.Contains(TEXT("LeftArm"))) FinalLimb = EAnatomicalLimb::LeftArm;
	else if (TagStr.Contains(TEXT("Head"))) FinalLimb = EAnatomicalLimb::Head;
	else if (TagStr.Contains(TEXT("Chest"))) FinalLimb = EAnatomicalLimb::Chest;

	int32& TraumaCount = LimbTraumaHistory.FindOrAdd(FinalLimb);
	TraumaCount++;

	RebuildCachedModifiers(); // PRZELICZAMY BUFOR W 0 NANOSEKUND!
	OnInjuryAdded.Broadcast(InjuryTag, FinalLimb);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red,
			FString::Printf(TEXT("🦴 [URAZ ANATOMICZNY] Otrzymano ranę: %s!"), *InjuryTag.ToString()));
	}
#endif

	static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor.Bleeding"), false);
	if (InjuryTag.MatchesTag(BleedTag))
	{
		if (AActor* Owner = GetOwner())
		{
			if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
			{
				StatusComp->ApplyBleed(30.0f, 1.0f, 3.0f);
			}
		}
	}
}

void UHealthComponent::RemoveInjury(FGameplayTag InjuryTag)
{
	if (!InjuryTag.IsValid() || !ActiveInjuries.HasTagExact(InjuryTag)) return;

	ActiveInjuries.RemoveTag(InjuryTag);
	RebuildCachedModifiers(); // PRZELICZAMY BUFOR!
	OnInjuryRemoved.Broadcast(InjuryTag);
}

bool UHealthComponent::HasActiveInjury(FGameplayTag InjuryTag) const
{
	if (bIsPalliativeActive) return false;
	return ActiveInjuries.HasTag(InjuryTag);
}

void UHealthComponent::UseBandage(float HealAmount)
{
	Heal(HealAmount);
	ClearAllMinorInjuries();

	if (AActor* Owner = GetOwner())
	{
		if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
		{
			StatusComp->StopBleed();
		}
	}

#if !UE_BUILD_SHIPPING
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("🩹 [MEDYCYNA] Założono Bandaż Karbolowy (+HP, Rany opatrzone)"));
#endif
}

void UHealthComponent::UseLaudanum(float Duration)
{
	bIsPalliativeActive = true;

#if !UE_BUILD_SHIPPING
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Purple,
		FString::Printf(TEXT("💉 [MEDYCYNA] Wstrzyknięto Laudanum! Ból zamaskowany na %.0fs!"), Duration));
#endif

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PalliativeTimerHandle, this, &UHealthComponent::EndPalliativeEffect, Duration, false);
	}
}

void UHealthComponent::EndPalliativeEffect()
{
	bIsPalliativeActive = false;

#if !UE_BUILD_SHIPPING
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("📉 [CRASH / ZJAZD] Działanie Laudanum minęło! Ból powraca ze zdwojoną siłą!"));
#endif

	if (AActor* Owner = GetOwner())
	{
		if (UStaminaComponent* StaminaComp = Owner->FindComponentByClass<UStaminaComponent>())
		{
			StaminaComp->TryConsumeStamina(100.0f);
		}
	}

	OnPalliativeCrash.Broadcast();
}

void UHealthComponent::UseLeeches()
{
	static const FGameplayTag HeadInjuryTag = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Major.Head"), false);
	RemoveInjury(HeadInjuryTag);
	TakeDamage(10.0f, FGameplayTag());

	if (AActor* Owner = GetOwner())
	{
		if (UStatusEffectComponent* StatusComp = Owner->FindComponentByClass<UStatusEffectComponent>())
		{
			StatusComp->ApplyBleed(15.0f, 1.0f, 3.0f);
		}
	}

#if !UE_BUILD_SHIPPING
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("🪱 [MEDYCYNA] Przyłożono Pijawki (Wyleczono wstrząs mózgu, -10 HP)"));
#endif
}

void UHealthComponent::ClearAllMinorInjuries()
{
	static const FGameplayTag MinorInjuryRoot = FGameplayTag::RequestGameplayTag(FName("Status.Injury.Minor"), false);

	TArray<FGameplayTag> TagsToRemove;
	for (const FGameplayTag& Tag : ActiveInjuries)
	{
		if (Tag.MatchesTag(MinorInjuryRoot))
		{
			TagsToRemove.Add(Tag);
		}
	}

	for (const FGameplayTag& Tag : TagsToRemove)
	{
		RemoveInjury(Tag);
	}
}

void UHealthComponent::Heal(float HealAmount)
{
	if (HealAmount <= 0.0f || IsDead()) return;

	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, GetMaxHealth());
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
}

void UHealthComponent::ApplyFaintCap(float CapMultiplier)
{
	MaxHealthCapMultiplier = FMath::Clamp(CapMultiplier, 0.1f, 1.0f);
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, GetMaxHealth());
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
}