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

	if (ToolItemData.EquipType == EItemEquipType::Throwable)
	{
		float ChargeMultiplier = 1.0f;
		if (AActor* MyOwner = GetOwner())
		{
			if (UInteractionComponent* InterComp = MyOwner->FindComponentByClass<UInteractionComponent>())
			{
				float ChargeAlpha = FMath::Clamp(HoldDuration / FMath::Max(0.1f, InterComp->MaxChargeTime), 0.0f, 1.0f);
				ChargeMultiplier = FMath::Lerp(1.0f, InterComp->MaxChargedThrowMultiplier, ChargeAlpha);
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

	bool bIsHeavy = (HoldDuration >= 0.35f);

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
	PerformUniversalAttackTrace(false);
}

void ABaseTool::PerformHeavyAttack()
{
	PerformUniversalAttackTrace(true);
}

void ABaseTool::PerformUniversalAttackTrace(bool bIsHeavy)
{
	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CamMgr) return;

	float CurrentRange = bIsHeavy ? ToolItemData.HeavyAttackRange : ToolItemData.AttackRange;
	float CurrentDamage = bIsHeavy ? ToolItemData.HeavyAttackDamage : ToolItemData.BaseDamage;
	float CurrentIntensity = bIsHeavy ? (ToolItemData.StateIntensity * 1.5f) : ToolItemData.StateIntensity;

	// Sprawdzamy czy gracz trzyma gardę (do pchnięcia obronnego):
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

			float Pain = Health->GetActionPainCost(EAnatomicalLimb::RightArm);
			if (Pain > 0.0f && !bIsPlayerInGuard)
			{
				static const FGameplayTag InternalPainTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
				Health->TakeDamage(Pain, InternalPainTag);

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("⚠️ [BÓL] Wyprowadzenie ciosu złamaną ręką zadało Ci rany!"));
#endif
			}
		}
	}

	FVector Start = CamMgr->GetCameraLocation();
	FVector ForwardDir = CamMgr->GetCameraRotation().Vector();
	FVector End = Start + (ForwardDir * CurrentRange);
	FRotator CameraRot = CamMgr->GetCameraRotation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn); // Trafia w potwory!

	// ====================================================================
	// 1. STOŻEK (Cone - Palnik Nyberga / Strzelba)
	// ====================================================================
	if (ToolItemData.AttackShape == EToolTraceShape::Cone)
	{
		float ActiveConeAngle = bIsHeavy ? (ToolItemData.AttackConeAngle * 1.3f) : ToolItemData.AttackConeAngle;

		TArray<FOverlapResult> Overlaps;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(CurrentRange);

		if (GetWorld()->OverlapMultiByObjectType(Overlaps, Start, FQuat::Identity, ObjectQueryParams, Sphere, Params))
		{
			float ConeLimit = FMath::Cos(FMath::DegreesToRadians(ActiveConeAngle));
			TSet<AActor*> ProcessedActors;

			for (const FOverlapResult& HitOverlap : Overlaps)
			{
				if (AActor* HitActor = HitOverlap.GetActor())
				{
					if (ProcessedActors.Contains(HitActor)) continue;
					ProcessedActors.Add(HitActor);

					FVector DirToTarget = (HitActor->GetActorLocation() - Start).GetSafeNormal();
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
		}

#if !UE_BUILD_SHIPPING
		// DIAGNOSTYKA WIZUALNA STOŻKA:
		DrawDebugCone(GetWorld(), Start, ForwardDir, CurrentRange, FMath::DegreesToRadians(ActiveConeAngle), FMath::DegreesToRadians(ActiveConeAngle), 16, bIsHeavy ? FColor::Purple : FColor::Orange, false, 1.5f);
#endif
		return;
	}

	// ====================================================================
	// 2. POZOSTAŁE KSZTAŁTY (Linia, Pudełko, Kula):
	// ====================================================================
	FHitResult Hit;
	bool bHit = false;

	switch (ToolItemData.AttackShape)
	{
	case EToolTraceShape::LineTrace:
		bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectQueryParams, Params);
#if !UE_BUILD_SHIPPING
		// DIAGNOSTYKA WIZUALNA LINII:
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.5f, 0, 2.0f);
#endif
		break;

	case EToolTraceShape::BoxSweep:
	{
		FVector ActiveBoxExtents = bIsHeavy ? (ToolItemData.BoxTraceHalfExtents * 1.5f) : ToolItemData.BoxTraceHalfExtents;
		bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, End, CameraRot.Quaternion(), ObjectQueryParams, FCollisionShape::MakeBox(ActiveBoxExtents), Params);
#if !UE_BUILD_SHIPPING
		// DIAGNOSTYKA WIZUALNA PUDEŁKA:
		DrawDebugBox(GetWorld(), bHit ? Hit.ImpactPoint : End, ActiveBoxExtents, CameraRot.Quaternion(), bHit ? FColor::Green : FColor::Red, false, 1.5f);
#endif
	}
	break;

	case EToolTraceShape::SphereSweep:
	default:
	{
		float ActiveRadius = bIsHeavy ? ToolItemData.HeavyAttackRadius : ToolItemData.AttackRadius;
		bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(ActiveRadius), Params);
#if !UE_BUILD_SHIPPING
		// DIAGNOSTYKA WIZUALNA KULI:
		DrawDebugSphere(GetWorld(), bHit ? Hit.ImpactPoint : End, ActiveRadius, 12, bHit ? (bIsHeavy ? FColor::Purple : FColor::Green) : FColor::Red, false, 1.5f);
#endif
	}
	break;
	}

	// ====================================================================
	// 3. FIZYKA UDERZENIA I TAKEDOWN
	// ====================================================================
	if (bHit && Hit.GetActor())
	{
		AActor* TargetActor = Hit.GetActor();

		// A. ODPYCHANIE W GARDZIE (SHIELD BASH)
		if (bIsPlayerInGuard)
		{
			if (ACharacter* TargetChar = Cast<ACharacter>(TargetActor))
			{
				FVector LaunchDirection = ForwardDir + FVector(0.0f, 0.0f, 0.25f);
				float LaunchStrength = bIsHeavy ? 1100.0f : 550.0f;

				TargetChar->LaunchCharacter(LaunchDirection * LaunchStrength, true, true);

#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
					FString::Printf(TEXT("🛡️ [ODEPCHNIĘCIE] %s! Siła: %.0f"), bIsHeavy ? TEXT("POTĘŻNY TARAN") : TEXT("Szybki wstrząs"), LaunchStrength));
#endif
			}
			else if (UPrimitiveComponent* TargetPrim = Cast<UPrimitiveComponent>(Hit.GetComponent()))
			{
				if (TargetPrim->IsSimulatingPhysics())
				{
					float PushImpulse = bIsHeavy ? 1200.0f : 600.0f;
					TargetPrim->AddImpulse(ForwardDir * PushImpulse, NAME_None, true);
				}
			}
		}
		// B. ATAK Z ZASKOCZENIA LUB ZWYKŁY CIOS BRONIĄ:
		else
		{
			bool bIsBackstab = false;
			FVector TargetForward = TargetActor->GetActorForwardVector();
			FVector DirFromTargetToPlayer = (GetOwner()->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal();

			if (FVector::DotProduct(TargetForward, DirFromTargetToPlayer) < -0.5f)
			{
				bIsBackstab = true;
			}

			FGameplayTag FinalTag = ToolItemData.PhysicalDamageTag;
			if (bIsBackstab)
			{
				FinalTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.StealthTakedown"), false);
			}

			ApplyMeleeHit(TargetActor, CurrentDamage, FinalTag, ToolItemData.AttackStateTag, CurrentIntensity);
		}
	}
}

void ABaseTool::QuickMelee()
{
	if (!bCanQuickMelee) return;

	AActor* MyOwner = GetOwner();
	if (!MyOwner) return;

	// ====================================================================
	// BLOKADA QUICK MELEE W GARDZIE:
	// ====================================================================
	if (UStatusEffectComponent* StatusComp = MyOwner->FindComponentByClass<UStatusEffectComponent>())
	{
		static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Combat.Guarding"), false);
		if (StatusComp->HasStatusEffect(GuardTag))
		{
			return; // Zablokowane w gardzie!
		}
	}

	float FinalDamage = (ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 15.0f : ToolItemData.QuickMeleeDamage;
	float FinalRange = (ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 100.0f : ToolItemData.QuickMeleeRange;
	float FinalCooldown = (ToolItemData.QuickMeleeType == EMeleeAttackType::FistPunch) ? 0.4f : ToolItemData.QuickMeleeCooldown;

	UHealthComponent* Health = MyOwner->FindComponentByClass<UHealthComponent>();
	UStaminaComponent* Stamina = MyOwner->FindComponentByClass<UStaminaComponent>();

	float StaminaCost = ToolItemData.StaminaCostPerAttack * 0.5f;

	if (Health)
	{
		float LeftArmPain = Health->GetActionPainCost(EAnatomicalLimb::LeftArm);
		if (LeftArmPain > 0.0f)
		{
			Health->TakeDamage(LeftArmPain, FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt")));

			if (Stamina)
			{
				StaminaCost = FMath::Max(StaminaCost, Stamina->GetCurrentStamina() * 0.5f);
			}

#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
				FString::Printf(TEXT("⚠️ [BÓL] Cios zwichniętym barkiem! -%.1f HP i utrata staminy!"), LeftArmPain));
#endif
		}
	}

	if (Stamina && !Stamina->TryConsumeStamina(StaminaCost)) return;

	bCanQuickMelee = false;
	ExecuteQuickMeleeEffect();

	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (CamMgr)
	{
		FVector Start = CamMgr->GetCameraLocation();
		FVector End = Start + (CamMgr->GetCameraRotation().Vector() * FinalRange);

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(GetOwner());

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		bool bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(25.0f), Params);

#if !UE_BUILD_SHIPPING
		DrawDebugSphere(GetWorld(), bHit ? Hit.ImpactPoint : End, 25.0f, 12, bHit ? FColor::Cyan : FColor::Yellow, false, 1.5f);
#endif

		if (bHit && Hit.GetActor())
		{
			static const FGameplayTag BluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
			ApplyMeleeHit(Hit.GetActor(), FinalDamage, BluntTag, FGameplayTag(), 0.0f);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(QuickMeleeTimerHandle, this, &ABaseTool::ResetQuickMeleeCooldown, FinalCooldown, false);
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
	if (StateTag.IsValid() && Receiver)
	{
		Receiver->ApplyStateImpact(StateTag, Intensity);
	}
}

bool ABaseTool::TryConsumeResources(bool bIsHeavy)
{
	AActor* MyOwner = GetOwner();
	if (!MyOwner) return false;

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