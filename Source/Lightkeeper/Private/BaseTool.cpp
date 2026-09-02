#include "BaseTool.h"
#include "LightkeeperCharacter.h"
#include "LanternComponent.h"
#include "InventoryComponent.h"
#include "StaminaComponent.h"
#include "ReactionReceiverComponent.h"
#include "HealthComponent.h"
#include "BaseInteractable.h"
#include "ToolManagerComponent.h"
#include "InteractionComponent.h"
#include "StatusEffectComponent.h"
#include "ProgressionComponent.h"
#include "ImSimSensorySubsystem.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h" 
#include "DrawDebugHelpers.h"

ABaseTool::ABaseTool()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootComp;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	StaticMeshComp->SetupAttachment(RootComponent);
	StaticMeshComp->SetCollisionProfileName(TEXT("NoCollision"));

	SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComp"));
	SkeletalMeshComp->SetupAttachment(RootComponent);
	SkeletalMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
}

void ABaseTool::BeginPlay()
{
	Super::BeginPlay();

	// Synchronizacja zmiennych ze struktury:
	ToolTag = ToolItemData.ItemTag;
	bUsesSharedFuel = ToolItemData.bUsesSharedFuel;
	EquipType = ToolItemData.EquipType;
	DropClass = ToolItemData.DropClass;

	if (!bUsesSharedFuel && CurrentAmmo <= 0.0f)
	{
		CurrentAmmo = ToolItemData.MaxAmmoCapacity;
	}
}

void ABaseTool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

UMeshComponent* ABaseTool::GetActiveMeshComponent() const
{
	if (SkeletalMeshComp && SkeletalMeshComp->GetSkeletalMeshAsset()) return SkeletalMeshComp;
	return StaticMeshComp;
}

void ABaseTool::EquipTool(AActor* NewOwner)
{
	SetOwner(NewOwner);
	CurrentState = EToolState::LowReady;
	SetActorHiddenInGame(false);
}

void ABaseTool::UnequipTool()
{
	CurrentState = EToolState::Holstered;
	SetActorHiddenInGame(true);
}

void ABaseTool::EnterAimState()
{
	if (CurrentState == EToolState::LowReady) CurrentState = EToolState::Aiming;
}

void ABaseTool::ExitAimState()
{
	PrimaryActionStartTime = 0.0f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HeavyChargeTimerHandle);
	}

	if (CurrentState == EToolState::Aiming || CurrentState == EToolState::Action)
	{
		CurrentState = EToolState::LowReady;
	}
}

void ABaseTool::StartPrimaryAction()
{
	if (CurrentState != EToolState::Aiming) return;

	PrimaryActionStartTime = GetWorld()->GetTimeSeconds();

	if (ToolItemData.EquipType != EItemEquipType::Throwable)
	{
		// ====================================================================
		// WYMÓG WIGORU 1A: Tylko postać z Wigorem potrafi ładować potężny cios!
		// ====================================================================
		bool bCanChargeHeavyAttack = false;
		if (AActor* MyOwner = GetOwner())
		{
			if (UProgressionComponent* ProgComp = MyOwner->FindComponentByClass<UProgressionComponent>())
			{
				bCanChargeHeavyAttack = ProgComp->CanPerformHeavyMelee(); // ODPYTUJEMY BRAMKĘ ZDOLNOŚCI!
			}
		}

		if (bCanChargeHeavyAttack)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					HeavyChargeTimerHandle,
					this,
					&ABaseTool::TriggerAutomaticHeavyAttack,
					0.35f,
					false
				);
			}
		}
	}
}

void ABaseTool::TriggerAutomaticHeavyAttack()
{
	if (CurrentState != EToolState::Aiming) return;

	PrimaryActionStartTime = 0.0f;

	if (!TryConsumeResources(true)) return;

	CurrentState = EToolState::Action;
	PerformHeavyAttack();

	GetWorld()->GetTimerManager().SetTimer(ActionCooldownTimer, this, &ABaseTool::ResetActionState, ToolItemData.ActionCooldown, false);
}

void ABaseTool::StopPrimaryAction()
{
	if (CurrentState != EToolState::Aiming || PrimaryActionStartTime <= 0.0f)
	{
		PrimaryActionStartTime = 0.0f;
		return;
	}

	float HoldDuration = GetWorld()->GetTimeSeconds() - PrimaryActionStartTime;
	PrimaryActionStartTime = 0.0f;

	// 1. DLA BRONI MIOTANEJ (GRANATY / MOŁOTOWY):
	if (ToolItemData.EquipType == EItemEquipType::Throwable)
	{
		float ChargeMultiplier = 1.0f;
		if (AActor* MyOwner = GetOwner())
		{
			// ====================================================================
			// WYMÓG WIGORU TIER 1 DLA ŁADOWANEGO RZUTU GRANATEM:
			// ====================================================================
			bool bCanChargeThrow = false;
			if (UProgressionComponent* ProgComp = MyOwner->FindComponentByClass<UProgressionComponent>())
			{
				bCanChargeThrow = ProgComp->CanPerformHeavyMelee();
			}

			// Tylko z Wigorem 1A gracz może naładować siłę rzutu granatu:
			if (bCanChargeThrow)
			{
				if (UInteractionComponent* InterComp = MyOwner->FindComponentByClass<UInteractionComponent>())
				{
					float ChargeAlpha = FMath::Clamp(HoldDuration / FMath::Max(0.1f, InterComp->MaxChargeTime), 0.0f, 1.0f);
					ChargeMultiplier = FMath::Lerp(1.0f, InterComp->MaxChargedThrowMultiplier, ChargeAlpha);
				}
			}

			if (ChargeMultiplier > 1.2f)
			{
				if (UHealthComponent* Health = MyOwner->FindComponentByClass<UHealthComponent>())
				{
					float Pain = Health->GetChargedThrowPainCost();
					if (Pain > 0.0f)
					{
						static const FGameplayTag BluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
						Health->TakeDamage(Pain, BluntTag);

#if !UE_BUILD_SHIPPING
						if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
							FString::Printf(TEXT("⚠️ [BÓL] Naładowany rzut granatem zadał Ci ból! -%.1f HP"), Pain));
#endif
					}
				}
			}
		}

		PerformThrowAction(ChargeMultiplier);
		return;
	}

	// ====================================================================
	// 2. DLA BRONI BIAŁEJ (BLOKADA HEAVY ATTACK POD WIGOR TIER 1):
	// ====================================================================
	bool bCanUseHeavy = false;
	if (AActor* MyOwner = GetOwner())
	{
		if (UProgressionComponent* ProgComp = MyOwner->FindComponentByClass<UProgressionComponent>())
		{
			bCanUseHeavy = ProgComp->CanPerformHeavyMelee(); // BRAMKA ZDOLNOŚCI!
		}
	}

	// Atak Ciężki odpali się TYLKO jeśli trzymano LPM >= 0.35s ORAZ gracz ma Wigor Tier 1:
	bool bIsHeavy = (HoldDuration >= 0.35f) && bCanUseHeavy;

	if (!TryConsumeResources(bIsHeavy)) return;

	CurrentState = EToolState::Action;

	if (bIsHeavy)
	{
		PerformHeavyAttack();
	}
	else
	{
		PerformLightAttack();
	}

	GetWorld()->GetTimerManager().SetTimer(ActionCooldownTimer, this, &ABaseTool::ResetActionState, ToolItemData.ActionCooldown, false);
}

void ABaseTool::PerformLightAttack()
{
	PerformUniversalAttackTrace(EAttackMode::Light);
}

void ABaseTool::PerformHeavyAttack()
{
	PerformUniversalAttackTrace(EAttackMode::Heavy);
}

void ABaseTool::QuickMelee()
{
	if (!bCanQuickMelee) return;

	AActor* MyOwner = GetOwner();
	if (!MyOwner) return;

	// 1. Blokada w gardzie:
	if (UStatusEffectComponent* StatusComp = MyOwner->FindComponentByClass<UStatusEffectComponent>())
	{
		static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
		if (StatusComp->HasStatusEffect(GuardTag))
		{
			return; // Zablokowane w trakcie trzymania gardy!
		}
	}

	// 2. Blokada przy zwichniętym barku (Major.LeftArm):
	if (UHealthComponent* Health = MyOwner->FindComponentByClass<UHealthComponent>())
	{
		if (!Health->IsActionAllowed(EPlayerAction::QuickMelee))
		{
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("❌ [URAZ] Zwichnięty bark uniemożliwia szybki cios kolbą [V]!"));
#endif
			return;
		}
	}

	// 3. Pobranie kosztu staminy:
	float StaminaCost = ToolItemData.StaminaCostPerAttack * 0.5f;
	UStaminaComponent* Stamina = MyOwner->FindComponentByClass<UStaminaComponent>();
	if (Stamina && !Stamina->TryConsumeStamina(StaminaCost)) return;

	bCanQuickMelee = false;
	ExecuteQuickMeleeEffect();

	// ====================================================================
	// WYZWOLENIE CENTRALNEGO SILNIKA WALKI WRĘCZ DLA QUICK MELEE [V]:
	// ====================================================================
	PerformUniversalAttackTrace(EAttackMode::QuickMelee);

	float FinalCooldown = (ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 0.4f : ToolItemData.QuickMeleeCooldown;
	if (FinalCooldown <= 0.0f) FinalCooldown = 0.5f;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(QuickMeleeTimerHandle, this, &ABaseTool::ResetQuickMeleeCooldown, FinalCooldown, false);
	}
}

void ABaseTool::PerformUniversalAttackTrace(EAttackMode AttackMode)
{
	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CamMgr) return;

	bool bIsHeavy = (AttackMode == EAttackMode::Heavy);
	bool bIsQuick = (AttackMode == EAttackMode::QuickMelee);

	float CurrentRange = bIsQuick ?
		((ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 100.0f : ToolItemData.QuickMeleeRange) :
		(bIsHeavy ? ToolItemData.HeavyAttackRange : ToolItemData.AttackRange);

	float CurrentDamage = bIsQuick ?
		((ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 15.0f : ToolItemData.QuickMeleeDamage) :
		(bIsHeavy ? ToolItemData.HeavyAttackDamage : ToolItemData.BaseDamage);

	float CurrentIntensity = bIsHeavy ? (ToolItemData.StateIntensity * 1.5f) : (bIsQuick ? 0.0f : ToolItemData.StateIntensity);
	EToolTraceShape ActiveShape = bIsQuick ? EToolTraceShape::SphereSweep : ToolItemData.AttackShape;
	bool bAllowStealthTakedown = !bIsQuick; // Quick Melee [V] NIGDY nie wykonuje Takedownu!

	// 1. Sprawdzamy Gardę i Ból Kości:
	bool bIsPlayerInGuard = false;

	if (AActor* MyOwner = GetOwner())
	{
		if (UStatusEffectComponent* StatusComp = MyOwner->FindComponentByClass<UStatusEffectComponent>())
		{
			static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
			bIsPlayerInGuard = StatusComp->HasStatusEffect(GuardTag);
		}

		if (UHealthComponent* Health = MyOwner->FindComponentByClass<UHealthComponent>())
		{
			CurrentDamage *= Health->GetMeleeDamageMultiplier();

			float Pain = Health->GetActionPainCost(bIsQuick ? EAnatomicalLimb::LeftArm : EAnatomicalLimb::RightArm);
			if (Pain > 0.0f && !bIsPlayerInGuard)
			{
				static const FGameplayTag InternalPainTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
				Health->TakeDamage(Pain, InternalPainTag);
			}
		}
	}

	// 2. Precyzyjny punkt startowy 15 cm przed celownikiem:
	FVector ForwardDir = CamMgr->GetCameraRotation().Vector();
	FVector Start = CamMgr->GetCameraLocation() + (ForwardDir * 15.0f);
	FVector End = Start + (ForwardDir * CurrentRange);
	FRotator CameraRot = CamMgr->GetCameraRotation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);

	// ====================================================================
	// 1. KSZTAŁT: STOŻEK (Cone - Palnik Nyberga / Garłacz)
	// ====================================================================
	if (ActiveShape == EToolTraceShape::Cone)
	{
		float ActiveConeAngle = bIsHeavy ? (ToolItemData.AttackConeAngle * 1.3f) : ToolItemData.AttackConeAngle;
		TArray<FHitResult> Overlaps;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(CurrentRange);

		GetWorld()->SweepMultiByChannel(Overlaps, Start, End, FQuat::Identity, ECC_Visibility, Sphere, Params);

		float ConeLimit = FMath::Cos(FMath::DegreesToRadians(ActiveConeAngle));
		TSet<AActor*> ProcessedActors;

		for (const FHitResult& HitOverlap : Overlaps)
		{
			if (AActor* HitActor = HitOverlap.GetActor())
			{
				if (ProcessedActors.Contains(HitActor)) continue;
				ProcessedActors.Add(HitActor);

				FVector TargetCenter = HitActor->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
				FVector DirToTarget = (TargetCenter - Start).GetSafeNormal();
				float Dot = FVector::DotProduct(ForwardDir, DirToTarget);

				if (Dot >= ConeLimit)
				{
					if (bIsPlayerInGuard)
					{
						if (ACharacter* TargetChar = Cast<ACharacter>(HitActor))
						{
							FVector LaunchDir = ForwardDir + FVector(0.0f, 0.0f, 0.20f);
							TargetChar->LaunchCharacter(LaunchDir * (bIsHeavy ? 900.0f : 550.0f), true, true);
						}
					}
					else
					{
						ApplyMeleeHit(HitActor, CurrentDamage, ToolItemData.PhysicalDamageTag, ToolItemData.AttackStateTag, CurrentIntensity);
					}
				}
			}
		}

#if !UE_BUILD_SHIPPING
		DrawDebugCone(GetWorld(), Start, ForwardDir, CurrentRange, FMath::DegreesToRadians(ActiveConeAngle), FMath::DegreesToRadians(ActiveConeAngle), 16, bIsHeavy ? FColor::Purple : FColor::Orange, false, 0.8f);
#endif
		return;
	}

	// ====================================================================
	// 2. POZOSTAŁE KSZTAŁTY (Linia, Pudełko, Kula)
	// ====================================================================
	FHitResult Hit;
	bool bHit = false;

	switch (ActiveShape)
	{
	case EToolTraceShape::LineTrace:
	{
		bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		if (!bHit) bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params);

#if !UE_BUILD_SHIPPING
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 0.8f, 0, 2.0f);
#endif
	}
	break;

	case EToolTraceShape::BoxSweep:
	{
		FVector ActiveBoxExtents = bIsHeavy ? (ToolItemData.BoxTraceHalfExtents * 1.5f) : ToolItemData.BoxTraceHalfExtents;
		FCollisionShape BoxShape = FCollisionShape::MakeBox(ActiveBoxExtents);

		bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, CameraRot.Quaternion(), ECC_Visibility, BoxShape, Params);
		if (!bHit) bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, CameraRot.Quaternion(), ECC_Pawn, BoxShape, Params);

#if !UE_BUILD_SHIPPING
		FColor BoxColor = bHit ? FColor::Green : FColor::Red;
		DrawDebugBox(GetWorld(), bHit ? Hit.ImpactPoint : ((Start + End) * 0.5f), ActiveBoxExtents, CameraRot.Quaternion(), BoxColor, false, 0.8f);
#endif
	}
	break;

	case EToolTraceShape::SphereSweep:
	default:
	{
		float ActiveRadius = bIsQuick ? 25.0f : (bIsHeavy ? ToolItemData.HeavyAttackRadius : ToolItemData.AttackRadius);
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(ActiveRadius);

		bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, SphereShape, Params);
		if (!bHit) bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, SphereShape, Params);

#if !UE_BUILD_SHIPPING
		FColor SphereColor = bHit ? (bIsQuick ? FColor::Cyan : (bIsHeavy ? FColor::Purple : FColor::Green)) : FColor::Red;
		DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, CurrentRange * 0.5f, ActiveRadius, FRotationMatrix::MakeFromZ(ForwardDir).ToQuat(), SphereColor, false, 0.8f, 0, 1.2f);
#endif
	}
	break;
	}

	// ====================================================================
	// 3. FIZYKA UDERZENIA, ODPYCHANIE W GARDZIE I STEALTH TAKEDOWN
	// ====================================================================
	if (bHit && Hit.GetActor())
	{
		AActor* TargetActor = Hit.GetActor();
		UPrimitiveComponent* HitComponent = Hit.GetComponent();

		// ------------------------------------------------------------
		// MIEJSCE 1: HAŁAS FIZYCZNEGO UDERZENIA W CEL (Skalowany materiałem):
		// ------------------------------------------------------------
		if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			FGameplayTag HitMaterialTag = UImSimSensorySubsystem::ExtractMaterialTagFromActor(TargetActor);

			float TargetMass = 20.0f;
			if (HitComponent && HitComponent->IsSimulatingPhysics())
			{
				TargetMass = HitComponent->GetMass();
			}

			float ToolMaterialMod = (bIsQuick || ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 0.5f : 1.3f;
			float MassFactor = FMath::Clamp(FMath::Sqrt(FMath::Max(1.0f, TargetMass) / 15.0f), 0.6f, 2.2f);
			float RawHitRadius = (CurrentDamage * 7.5f) * MassFactor * ToolMaterialMod;
			float FinalHitRadius = FMath::Clamp(RawHitRadius, 80.0f, 1800.0f);

			FVector SoundLocation = Hit.ImpactPoint.IsNearlyZero() ? End : FVector(Hit.ImpactPoint);

			// Subsystem sam przeliczy mnożnik materiału:
			Sensory->RegisterNoise(SoundLocation, FinalHitRadius, NoiseTag, HitMaterialTag);
		}

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			FString AttackTypeStr = bIsQuick ? TEXT("[QUICK MELEE V]") : (bIsHeavy ? TEXT("[HEAVY ATTACK]") : TEXT("[LIGHT ATTACK]"));
			FString CompName = HitComponent ? HitComponent->GetName() : TEXT("BrakKomponentu");
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan,
				FString::Printf(TEXT("🎯 %s Trafiono: %s -> %s (Dystans: %.0f cm)"),
					*AttackTypeStr, *TargetActor->GetName(), *CompName, Hit.Distance));
		}
#endif

		// A. ODPYCHANIE W GARDZIE (SHIELD BASH):
		if (bIsPlayerInGuard)
		{
			if (ACharacter* TargetChar = Cast<ACharacter>(TargetActor))
			{
				FVector LaunchDirection = ForwardDir + FVector(0.0f, 0.0f, 0.25f);
				float LaunchStrength = bIsHeavy ? 1100.0f : 550.0f;
				TargetChar->LaunchCharacter(LaunchDirection * LaunchStrength, true, true);

				if (UStatusEffectComponent* TargetStatus = TargetChar->FindComponentByClass<UStatusEffectComponent>())
				{
					TargetStatus->ApplyStun(bIsHeavy ? 1.5f : 0.7f);
				}
			}
			else if (HitComponent && HitComponent->IsSimulatingPhysics())
			{
				float PushImpulse = bIsHeavy ? 1200.0f : 600.0f;
				HitComponent->AddImpulse(ForwardDir * PushImpulse, NAME_None, true);
			}
		}
		// B. STEALTH TAKEDOWN (TYLKO DLA POTWORÓW APawn!) LUB ZWYKŁY CIOS:
		else
		{
			bool bIsBackstab = false;

			if (bAllowStealthTakedown && TargetActor->IsA<APawn>())
			{
				FVector TargetForward = TargetActor->GetActorForwardVector();
				FVector DirFromTargetToPlayer = (GetOwner()->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal();

				if (FVector::DotProduct(TargetForward, DirFromTargetToPlayer) < -0.5f)
				{
					bIsBackstab = true;
				}
			}

			FGameplayTag FinalDamageTag = bIsQuick ? FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false) : ToolItemData.PhysicalDamageTag;
			if (bIsBackstab)
			{
				FinalDamageTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.StealthTakedown"), false);
			}

			FGameplayTag FinalStateTag = bIsQuick ? FGameplayTag::EmptyTag : ToolItemData.AttackStateTag;
			float FinalIntensity = bIsQuick ? 0.0f : CurrentIntensity;

			ApplyMeleeHit(TargetActor, CurrentDamage, FinalDamageTag, FinalStateTag, FinalIntensity);
		}
	}
	// ====================================================================
	// MIEJSCE 2: PUDŁO W POWIETRZE (Świst zamachu / Whoosh dla stojących obok):
	// ====================================================================
	else
	{
		if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			float WhooshNoiseRadius = bIsHeavy ? 180.0f : (bIsQuick ? 60.0f : 120.0f);
			Sensory->RegisterNoise(Start, WhooshNoiseRadius, NoiseTag);
		}
	}
}

void ABaseTool::PerformThrowAction(float ChargeMultiplier)
{
	AActor* MyOwner = GetOwner();
	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);

	if (!MyOwner || !CamMgr || !ToolItemData.DropClass) return;

	const FVector CameraLoc = CamMgr->GetCameraLocation();
	const FVector CameraForward = CamMgr->GetCameraRotation().Vector();
	const FVector TargetSpawnLoc = CameraLoc + (CameraForward * 55.0f);

	FHitResult WallHit;
	FCollisionQueryParams SweepParams;
	SweepParams.AddIgnoredActor(MyOwner);
	SweepParams.AddIgnoredActor(this);

	FVector SafeSpawnLoc = TargetSpawnLoc;
	bool bIsPointBlank = false;

	// Skanujemy czy przed nosem nie ma drzwi lub ściany:
	if (GetWorld()->SweepSingleByChannel(WallHit, CameraLoc, TargetSpawnLoc, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(10.0f), SweepParams))
	{
		bIsPointBlank = true;
		FVector SurfaceNormal = WallHit.ImpactNormal.IsNearlyZero() ? -CameraForward : WallHit.ImpactNormal;
		SafeSpawnLoc = WallHit.Location + (SurfaceNormal * 14.0f);
	}

	FRotator SpawnRot = CamMgr->GetCameraRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseInteractable* ThrownProp = GetWorld()->SpawnActor<ABaseInteractable>(ToolItemData.DropClass, SafeSpawnLoc, SpawnRot, SpawnParams);
	if (ThrownProp)
	{
		ThrownProp->ApplyItemData(ToolItemData);

		UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(ThrownProp->GetRootComponent());
		if (!Prim) Prim = ThrownProp->FindComponentByClass<UPrimitiveComponent>();

		if (Prim)
		{
			// ====================================================================
			// TWARDA KOLIZJA Z POTWORAMI (ECC_Pawn = ECR_Block!):
			// ====================================================================
			Prim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

			// Ignorujemy TYLKO samego gracza:
			Prim->IgnoreActorWhenMoving(MyOwner, true);
			if (ACharacter* Char = Cast<ACharacter>(MyOwner))
			{
				if (Char->GetCapsuleComponent())
				{
					Char->GetCapsuleComponent()->IgnoreActorWhenMoving(ThrownProp, true);
				}
			}

			Prim->SetSimulatePhysics(true);
			Prim->SetUseCCD(true);
			Prim->BodyInstance.bUseCCD = true;

			if (UInteractionComponent* InterComp = MyOwner->FindComponentByClass<UInteractionComponent>())
			{
				FVector ThrowDir = CameraForward;
				FVector FinalThrowVelocity = InterComp->CalculateThrowVelocity(ThrowDir, Prim->GetMass(), ChargeMultiplier);

				if (bIsPointBlank)
				{
					FVector SurfaceNormal = WallHit.ImpactNormal.IsNearlyZero() ? -CameraForward : WallHit.ImpactNormal;
					FinalThrowVelocity = (SurfaceNormal * 140.0f) + FVector(0.0f, 0.0f, -40.0f);
				}

				Prim->SetPhysicsLinearVelocity(FinalThrowVelocity);
				IPhysicalInteract::Execute_SlamObject(ThrownProp, ThrowDir, FinalThrowVelocity.Size());
			}
		}
	}

	if (UInventoryComponent* InvComp = MyOwner->FindComponentByClass<UInventoryComponent>())
	{
		for (int32 i = 0; i < InvComp->StoredItems.Num(); ++i)
		{
			if (InvComp->StoredItems[i].ItemData.DropClass == ToolItemData.DropClass || InvComp->StoredItems[i].ItemData.ItemTag.MatchesTag(ToolItemData.ItemTag))
			{
				InvComp->StoredItems.RemoveAt(i);
				InvComp->OnInventoryUpdated.Broadcast();
				break;
			}
		}
	}

	UnequipTool();
	Destroy();
}

void ABaseTool::ApplyMeleeHit(AActor* TargetActor, float PhysicalDamage, FGameplayTag PhysicalDamageTag, FGameplayTag StateTag, float Intensity)
{
	if (!TargetActor || !IsValid(TargetActor)) return;

	UHealthComponent* TargetHealth = TargetActor->FindComponentByClass<UHealthComponent>();
	UReactionReceiverComponent* Receiver = TargetActor->FindComponentByClass<UReactionReceiverComponent>();

	// 1. GŁÓWNE OBRAŻENIA FIZYCZNE (Obuch / Cięcie):
	if (PhysicalDamage > 0.0f && TargetHealth)
	{
		static const FGameplayTag DefaultBluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
		FGameplayTag FinalPhysicalTag = PhysicalDamageTag.IsValid() ? PhysicalDamageTag : DefaultBluntTag;

		TargetHealth->TakeDamage(PhysicalDamage, FinalPhysicalTag);
	}

	// 2. PŁASKIE OBRAŻENIA ŻYWIOŁOWE PRZY UDERZENIU (Np. Ogień / Kwas):
	if (ToolItemData.ElementalDamage > 0.0f && StateTag.IsValid() && TargetHealth)
	{
		TargetHealth->TakeDamage(ToolItemData.ElementalDamage, StateTag);
	}

	// 3. EFEKT CHEMICZNY / STATUS (Podpalenie / Przewodnictwo):
	if (StateTag.IsValid() && Intensity > 0.0f)
	{
		if (TargetActor->FindComponentByClass<UReactionReceiverComponent>())
		{
			Receiver->ApplyStateImpact(StateTag, Intensity);
		}
	}
}

bool ABaseTool::TryConsumeResources(bool bIsHeavy)
{
	AActor* MyOwner = GetOwner();
	if (!MyOwner) return false;
	bool bIsPlayerInGuard = false;
	if (UStatusEffectComponent* StatusComp = MyOwner->FindComponentByClass<UStatusEffectComponent>())
	{
		static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
		bIsPlayerInGuard = StatusComp->HasStatusEffect(GuardTag);
	}


	float RequiredStamina = bIsHeavy ? ToolItemData.HeavyAttackStaminaCost : ToolItemData.StaminaCostPerAttack;

	if (UHealthComponent* Health = MyOwner->FindComponentByClass<UHealthComponent>())
	{
		RequiredStamina *= Health->GetStaminaDrainMultiplier();
	}

	float RequiredResource = bIsHeavy ? (ToolItemData.ResourceCostPerAction * 2.0f) : ToolItemData.ResourceCostPerAction;

	// 1. STAMINA:
	if (UStaminaComponent* Stamina = MyOwner->FindComponentByClass<UStaminaComponent>())
	{
		if (!Stamina->TryConsumeStamina(RequiredStamina))
		{
			return false;
		}
	}

	if (bIsPlayerInGuard)
	{
		return true; // Pchnięcie bronią nie zużywa ani kropli nafty ani amunicji!
	}

	// 2. PALIWO / AMUNICJA:
	if (RequiredResource > 0.0f)
	{
		if (ToolItemData.bUsesSharedFuel)
		{
			if (ULanternComponent* Lantern = MyOwner->FindComponentByClass<ULanternComponent>())
			{
				if (Lantern->CurrentFuel >= RequiredResource)
				{
					Lantern->CurrentFuel -= RequiredResource;
					Lantern->OnFuelChanged.Broadcast(Lantern->CurrentFuel, Lantern->MaxFuel);
					return true;
				}
			}
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[BROŃ] Brak nafty!"));
			return false;
		}
		else
		{
			if (CurrentAmmo >= RequiredResource)
			{
				CurrentAmmo -= RequiredResource;
				return true;
			}
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[BROŃ] Magazynek pusty!"));
			return false;
		}
	}

	return true;
}

void ABaseTool::InitializeFromItemData(const FInventoryItemData& Data, UStaticMesh* SourceMesh)
{
	ToolItemData = Data;
	ToolTag = Data.ItemTag;
	bUsesSharedFuel = Data.bUsesSharedFuel;
	EquipType = Data.EquipType;
	DropClass = Data.DropClass;

	if (!ToolItemData.bUsesSharedFuel && CurrentAmmo <= 0.0f)
	{
		CurrentAmmo = ToolItemData.MaxAmmoCapacity;
	}

	if (StaticMeshComp && SourceMesh)
	{
		StaticMeshComp->SetStaticMesh(SourceMesh);
		StaticMeshComp->SetRelativeScale3D(Data.MeshScale); // Używa czystej skali z Blueprinta!
		StaticMeshComp->SetRelativeLocation(FVector::ZeroVector);
		StaticMeshComp->SetCastShadow(false);
	}
}

void ABaseTool::ExecuteQuickMeleeEffect_Implementation() {}

void ABaseTool::ResetActionState()
{
	PrimaryActionStartTime = 0.0f;

	bool bStillHoldingAim = false;
	if (AActor* MyOwner = GetOwner())
	{
		if (UToolManagerComponent* ToolMgr = MyOwner->FindComponentByClass<UToolManagerComponent>())
		{
			bStillHoldingAim = ToolMgr->IsAimInputHeld();
		}
	}

	CurrentState = bStillHoldingAim ? EToolState::Aiming : EToolState::LowReady;
}

void ABaseTool::ResetQuickMeleeCooldown() { bCanQuickMelee = true; }
void ABaseTool::EndPrimaryAction() {}

bool ABaseTool::ReloadTool()
{
	if (ToolItemData.bUsesSharedFuel || CurrentAmmo >= ToolItemData.MaxAmmoCapacity || !ToolItemData.RequiredAmmoTag.IsValid()) return false;

	if (AActor* MyOwner = GetOwner())
	{
		if (UInventoryComponent* InvComp = MyOwner->FindComponentByClass<UInventoryComponent>())
		{
			if (InvComp->HasItemWithTag(ToolItemData.RequiredAmmoTag))
			{
				float RestoredAmount = 0.0f;
				for (int32 i = 0; i < InvComp->StoredItems.Num(); ++i)
				{
					if (InvComp->StoredItems[i].ItemData.ItemTag.MatchesTag(ToolItemData.RequiredAmmoTag))
					{
						RestoredAmount = InvComp->StoredItems[i].ItemData.PrimaryValue;
						InvComp->StoredItems.RemoveAt(i);
						InvComp->OnInventoryUpdated.Broadcast();
						break;
					}
				}

				CurrentAmmo = FMath::Clamp(CurrentAmmo + RestoredAmount, 0.0f, ToolItemData.MaxAmmoCapacity);
				return true;
			}
		}
	}
	return false;
}