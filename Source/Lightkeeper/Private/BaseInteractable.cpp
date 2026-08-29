#include "BaseInteractable.h"
#include "LightkeeperCharacter.h"
#include "BaseTool.h"
#include "LanternComponent.h"
#include "SanityComponent.h"
#include "HealthComponent.h"
#include "StaminaComponent.h"
#include "ReactionReceiverComponent.h"
#include "InventoryComponent.h"
#include "SafeLightComponent.h"
#include "MetroidvaniaGateComponent.h"
#include "ProgressionComponent.h"
#include "ToolManagerComponent.h"
#include "ImSimSensorySubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"      
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"

ABaseInteractable::ABaseInteractable()
{
	PrimaryActorTick.bCanEverTick = true;

	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	ReactionComp = CreateDefaultSubobject<UReactionReceiverComponent>(TEXT("ReactionReceiverComponent"));

	BlockingHazardStates.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard"), false));
}

void ABaseInteractable::BeginPlay()
{
	Super::BeginPlay();

	GateComp = FindComponentByClass<UMetroidvaniaGateComponent>();

	if (HealthComp)
	{
		HealthComp->OnDeath.AddDynamic(this, &ABaseInteractable::HandleDeath);
	}

	if (ReactionComp)
	{
		ReactionComp->OnStateApplied.AddDynamic(this, &ABaseInteractable::HandleStateApplied);
	}

	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Prim : PrimComps)
	{
		if (Prim)
		{
			if (Prim->IsA<USafeLightComponent>()) continue;

			Prim->SetNotifyRigidBodyCollision(true);
			Prim->OnComponentHit.AddDynamic(this, &ABaseInteractable::OnHit);

			Prim->SetCanEverAffectNavigation(true);

			if (Prim->IsSimulatingPhysics())
			{
				Prim->SetLinearDamping(0.01f);
				Prim->SetAngularDamping(0.05f);
				Prim->SetUseCCD(true);
				Prim->BodyInstance.bUseCCD = true;
				Prim->BodyInstance.SetMaxDepenetrationVelocity(250.0f);
				Prim->WakeRigidBody();
			}
		}
	}

	if (ItemData.bIsStateEmitter && ItemData.TriggerType == EEmissionTrigger::ContinuousZone)
	{
		GetWorld()->GetTimerManager().SetTimer(ContinuousTimerHandle, this, &ABaseInteractable::TriggerStateEmission, 0.5f, true);
	}
}

void ABaseInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABaseInteractable::HandleStateApplied(FGameplayTag StateTag, float Intensity)
{
}

void ABaseInteractable::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bIsBroken) return;
	if (!OtherActor || OtherActor == this) return;

	if (IsAttachedTo(OtherActor) || OtherActor->IsAttachedTo(this)) return;
	if (GetAttachParentActor() == OtherActor || OtherActor->GetAttachParentActor() == this) return;
	if (GetWorld()->GetTimeSeconds() < 0.3f) return;

	if (Hit.bBlockingHit)
	{
		LastImpactNormal = Hit.ImpactNormal;
	}

	ABaseInteractable* OtherInteractable = Cast<ABaseInteractable>(OtherActor);
	const bool bOtherHeld = OtherInteractable && OtherInteractable->bIsHeld;
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// 1. BLOKADA SPYCHACZA:
	if (this->bIsHeld || bOtherHeld) return;

	// ====================================================================
	// 2. PRAWDZIWA PRĘDKOŚĆ LOTU (BEZ FAŁSZYWYCH IMPULSÓW CHAOSU):
	// ====================================================================
	FVector MyVel = HitComponent ? HitComponent->GetComponentVelocity() : FVector::ZeroVector;
	float ImpactSpeed = MyVel.Size(); // Prawdziwa prędkość lotu w centymetrach/sekundę!
	float ActualMass = (HitComponent && HitComponent->IsSimulatingPhysics()) ? HitComponent->GetMass() : ReferenceMass;

	// ====================================================================
	// 3. HAŁAS DLA AI (Tylko uderzenia z prędkością >= 240 cm/s):
	// ====================================================================
	if (ImpactSpeed >= 55.0f && (CurrentTime - LastNoiseTime > 0.25f))
	{
		float MassFactor = FMath::Clamp(FMath::Sqrt(ActualMass / 2.0f), 0.45f, 3.5f);
		float CalculatedNoiseRadius = (ImpactSpeed * MassFactor) * 0.9f;

		// Rejestrujemy nawet mały hałas FootKickera (od 45 cm w górę):
		if (CalculatedNoiseRadius >= 45.0f)
		{
			LastNoiseTime = CurrentTime;

			if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
			{
				static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
				FVector SoundLoc = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);
				float FinalNoiseRadius = FMath::Clamp(CalculatedNoiseRadius, 45.0f, 2200.0f);

				Sensory->RegisterNoise(SoundLoc, FinalNoiseRadius, NoiseTag);
			}
		}
	}

	// ====================================================================
	// 4. DETONACJA WYBUCHU (OnImpact):
	// ====================================================================
	if (ItemData.bIsStateEmitter && ItemData.TriggerType == EEmissionTrigger::OnImpact)
	{
		const float DetonationThreshold = 200.0f;

		if (CurrentTime - LastEmissionTime < 0.5f) return;

		if (ImpactSpeed >= DetonationThreshold)
		{
			LastEmissionTime = CurrentTime;

#if !UE_BUILD_SHIPPING
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					(uint64)GetUniqueID() + 500,
					2.5f,
					FColor::Green,
					FString::Printf(TEXT("💥 [%s] Prędkość: %.1f / %.1f cm/s | DETONACJA / WYBUCH!"),
						*GetName(), ImpactSpeed, DetonationThreshold)
				);
			}
#endif
			TriggerStateEmission();
			return;
		}
	}

	// 5. PRZEKAZYWANIE STANÓW CHEMICZNYCH:
	if (ReactionComp)
	{
		static const FGameplayTag BurningTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
		static const FGameplayTag FireElementTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"), false);
		static const FGameplayTag MetalTag = FGameplayTag::RequestGameplayTag(FName("Material.Metal"), false);
		static const FGameplayTag ElectrocutedTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
		static const FGameplayTag CurrentElementTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Electricity.Current"), false);

		if (ReactionComp->HasState(BurningTag))
		{
			if (UReactionReceiverComponent* TargetReceiver = OtherActor->FindComponentByClass<UReactionReceiverComponent>())
			{
				TargetReceiver->ApplyStateImpact(FireElementTag, 1.0f);
			}
		}

		if (MaterialTag.MatchesTag(MetalTag) && ReactionComp->HasState(ElectrocutedTag))
		{
			if (UReactionReceiverComponent* TargetReceiver = OtherActor->FindComponentByClass<UReactionReceiverComponent>())
			{
				TargetReceiver->ApplyStateImpact(CurrentElementTag, 1.0f);
			}
		}
	}

	// ====================================================================
	// 6. UDERZENIE W POTWORA (ZADANIE OBRAŻEŃ DLA APawn):
	// ====================================================================
	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (bIsHeld) return;
		if (CurrentTime - LastHitTime < 0.35f) return;

		// Rzucony obiekt uderza potwora z prędkością >= 250 cm/s:
		if (ImpactSpeed >= 140.0f)
		{
			LastHitTime = CurrentTime;

			if (UHealthComponent* PawnHealth = HitPawn->FindComponentByClass<UHealthComponent>())
			{
				static const FGameplayTag BluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
				float RawDamage = (ImpactSpeed - 180.0f) * 0.035f * FMath::Sqrt(ActualMass / 6.0f);
				float DamageToPawn = FMath::Clamp(RawDamage, 10.0f, 25.0f);

				PawnHealth->TakeDamage(DamageToPawn, BluntTag, Hit);

#if !UE_BUILD_SHIPPING
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange,
						FString::Printf(TEXT("🎯 [%s] Trafiono potwora %s! Obrażenia: -%.1f HP"),
							*GetName(), *HitPawn->GetName(), DamageToPawn));
				}
#endif
			}
		}
		return;
	}

	// ====================================================================
	// 7. OBRAŻENIA MECHANICZNE DLA DRZWI I MEBLI (35 - 45 HP):
	// ====================================================================
	float BaseMinSpeed = 450.0f;
	if (HealthComp && HealthComp->DamageSusceptibility > 2.0f)
	{
		BaseMinSpeed = 150.0f;
	}

	float MassDivisor = FMath::Sqrt(FMath::Max(1.0f, ActualMass / 6.0f));
	float MinSpeedToDamage = FMath::Clamp(BaseMinSpeed / MassDivisor, 180.0f, BaseMinSpeed);

	if (ImpactSpeed < MinSpeedToDamage) return;

	if (CurrentTime - LastHitTime < 0.35f) return;
	LastHitTime = CurrentTime;

	float ExcessSpeed = ImpactSpeed - MinSpeedToDamage;
	float RawKineticEnergy = (ExcessSpeed * 0.08f) * FMath::Sqrt(ActualMass / 6.0f);

	if (RawKineticEnergy < 8.0f) return;

	float MaxDamageCap = (HealthComp && HealthComp->DamageSusceptibility > 2.0f) ? 60.0f : 45.0f;
	float KineticEnergy = FMath::Clamp(RawKineticEnergy, 8.0f, MaxDamageCap);

	static const FGameplayTag BluntDamageTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);

	if (HealthComp)
	{
		HealthComp->TakeDamage(KineticEnergy, BluntDamageTag);
	}

	if (UHealthComponent* TargetHealth = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		float IncomingDamageToTarget = KineticEnergy * ImpactHardness;
		TargetHealth->TakeDamage(IncomingDamageToTarget, BluntDamageTag);
	}
}

void ABaseInteractable::HandleDeath()
{
	if (bIsBroken) return;
	bIsBroken = true;

	if (ItemData.bIsStateEmitter && ItemData.TriggerType == EEmissionTrigger::OnDestroy)
	{
		TriggerStateEmission();
		return;
	}

	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Prim : PrimComps)
	{
		if (Prim)
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	Destroy();
}

void ABaseInteractable::TriggerStateEmission()
{
	if (!ItemData.bIsStateEmitter) return;

	FVector EmissionLocation = GetActorLocation();
	FVector EmissionForward = GetActorForwardVector();
	FRotator EmissionRotation = GetActorRotation();

	if (UPrimitiveComponent* PrimComp = FindComponentByClass<UPrimitiveComponent>())
	{
		if (ItemData.EmissionSocketName != NAME_None && PrimComp->DoesSocketExist(ItemData.EmissionSocketName))
		{
			EmissionLocation = PrimComp->GetSocketLocation(ItemData.EmissionSocketName);
			EmissionForward = PrimComp->GetSocketRotation(ItemData.EmissionSocketName).Vector();
			EmissionRotation = PrimComp->GetSocketRotation(ItemData.EmissionSocketName);
		}
		else
		{
			EmissionLocation = PrimComp->GetComponentLocation();

			if (ItemData.bAlignToSurfaceNormal)
			{
				FVector SurfaceNormal = LastImpactNormal.IsNearlyZero() ? FVector::UpVector : LastImpactNormal;
				EmissionRotation = FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator();
				EmissionForward = SurfaceNormal;
			}
			else
			{
				EmissionForward = GetActorForwardVector();
				EmissionRotation = GetActorRotation();
			}
		}
	}

	// ====================================================================
	// W 100% DYNAMICZNY HAŁAS WYBUCHU (Zależy od DMG, Promienia i Mocy Żywiołu):
	// ====================================================================
	if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
	{
		static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);

		// Wyliczamy hałas: Promień rozprysku + Siła obrażeń * Mnożnik intensywności
		float DynamicExplosionNoise = (ItemData.SplashRadius * 3.0f) + (ItemData.EmissionBurstDamage * 25.0f);
		DynamicExplosionNoise *= FMath::Max(1.0f, ItemData.EmissionStateIntensity);

		// Zabezpieczenie limitu (od 6m dla małych fiolek do 50m dla wielkich kotłów):
		float FinalNoiseRadius = FMath::Clamp(DynamicExplosionNoise, 600.0f, 5000.0f);

		Sensory->RegisterNoise(EmissionLocation, FinalNoiseRadius, NoiseTag);

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("💥 [%s] EKSPLOZJA! DMG: %.0f | Zasięg fali: %.0f cm (%.1f m)"),
					*GetName(), ItemData.EmissionBurstDamage, FinalNoiseRadius, FinalNoiseRadius / 100.0f));
		}
#endif
	}

	if (ItemData.bInvertEmissionDirection)
	{
		EmissionForward = -EmissionForward;
		EmissionRotation = EmissionForward.Rotation();
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	if (ItemData.EmissionShape == EEmissionShape::LineLaser)
	{
		FVector End = EmissionLocation + (EmissionForward * ItemData.SplashRadius);
		FHitResult Hit;

		if (GetWorld()->LineTraceSingleByObjectType(Hit, EmissionLocation, End, ObjectQueryParams, Params))
		{
			if (AActor* HitActor = Hit.GetActor())
			{
				if (ItemData.EmittedStateTag.IsValid())
				{
					if (UReactionReceiverComponent* Receiver = HitActor->FindComponentByClass<UReactionReceiverComponent>())
					{
						Receiver->ApplyStateImpact(ItemData.EmittedStateTag, ItemData.EmissionStateIntensity);
					}
				}

				if (ItemData.EmissionBurstDamage > 0.0f)
				{
					if (UHealthComponent* TargetHealth = HitActor->FindComponentByClass<UHealthComponent>())
					{
						TargetHealth->TakeDamage(ItemData.EmissionBurstDamage, ItemData.EmittedStateTag);
					}
				}
			}
		}

#if !UE_BUILD_SHIPPING
		DrawDebugLine(GetWorld(), EmissionLocation, End, FColor::Red, false, 1.5f, 0, 2.0f);
#endif
	}
	else
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionShape OverlapShape;

		if (ItemData.EmissionShape == EEmissionShape::BoxVolume)
		{
			OverlapShape = FCollisionShape::MakeBox(ItemData.BoxEmissionExtents);
		}
		else if (ItemData.EmissionShape == EEmissionShape::CylinderDisc)
		{
			OverlapShape = FCollisionShape::MakeBox(FVector(ItemData.SplashRadius, ItemData.SplashRadius, ItemData.DiscHeight));
		}
		else
		{
			OverlapShape = FCollisionShape::MakeSphere(ItemData.SplashRadius);
		}

		if (GetWorld()->OverlapMultiByObjectType(Overlaps, EmissionLocation, EmissionRotation.Quaternion(), ObjectQueryParams, OverlapShape, Params))
		{
			float ConeLimit = FMath::Cos(FMath::DegreesToRadians(ItemData.StreamConeAngle));
			TSet<AActor*> ProcessedActors;

			for (const FOverlapResult& Hit : Overlaps)
			{
				if (AActor* HitActor = Hit.GetActor())
				{
					if (ProcessedActors.Contains(HitActor)) continue;
					ProcessedActors.Add(HitActor);

					if (ItemData.EmissionShape == EEmissionShape::DirectionalCone)
					{
						FVector DirToTarget = (HitActor->GetActorLocation() - EmissionLocation).GetSafeNormal();
						float Dot = FVector::DotProduct(EmissionForward, DirToTarget);
						if (Dot < ConeLimit) continue;
					}

					if (ItemData.EmissionShape == EEmissionShape::CylinderDisc)
					{
						FVector LocalPos = EmissionRotation.UnrotateVector(HitActor->GetActorLocation() - EmissionLocation);
						float Dist2D = FVector2D(LocalPos.X, LocalPos.Y).Size();
						float DistZ = FMath::Abs(LocalPos.Z);

						if (Dist2D > ItemData.SplashRadius || DistZ > ItemData.DiscHeight) continue;
					}

					if (ItemData.EmittedStateTag.IsValid())
					{
						if (UReactionReceiverComponent* Receiver = HitActor->FindComponentByClass<UReactionReceiverComponent>())
						{
							Receiver->ApplyStateImpact(ItemData.EmittedStateTag, 1.0f);
						}
					}

					if (ItemData.EmissionBurstDamage > 0.0f)
					{
						if (UHealthComponent* TargetHealth = HitActor->FindComponentByClass<UHealthComponent>())
						{
							TargetHealth->TakeDamage(ItemData.EmissionBurstDamage, ItemData.EmittedStateTag);
						}
					}
				}
			}
		}

#if !UE_BUILD_SHIPPING
		if (ItemData.EmissionShape == EEmissionShape::CylinderDisc)
		{
			FVector CylinderNormal = EmissionForward.IsNearlyZero() ? FVector::UpVector : EmissionForward;
			FVector CylinderStart = EmissionLocation - (CylinderNormal * ItemData.DiscHeight);
			FVector CylinderEnd = EmissionLocation + (CylinderNormal * ItemData.DiscHeight);
			DrawDebugCylinder(GetWorld(), CylinderStart, CylinderEnd, ItemData.SplashRadius, 24, FColor::Red, false, 2.0f);
		}
		else if (ItemData.EmissionShape == EEmissionShape::BoxVolume)
		{
			DrawDebugBox(GetWorld(), EmissionLocation, ItemData.BoxEmissionExtents, EmissionRotation.Quaternion(), FColor::Red, false, 1.5f);
		}
		else if (ItemData.EmissionShape == EEmissionShape::DirectionalCone)
		{
			DrawDebugCone(GetWorld(), EmissionLocation, EmissionForward, ItemData.SplashRadius, FMath::DegreesToRadians(ItemData.StreamConeAngle), FMath::DegreesToRadians(ItemData.StreamConeAngle), 16, FColor::Red, false, 1.5f);
		}
		else
		{
			DrawDebugSphere(GetWorld(), EmissionLocation, ItemData.SplashRadius, 16, FColor::Red, false, 2.0f);
		}
#endif
	}

	if (ItemData.bIsPersistentZone)
	{
		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(ZoneExpiryTimerHandle))
			{
				if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(GetRootComponent()))
				{
					Prim->SetSimulatePhysics(false);
					Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Prim->SetVisibility(false);
				}

				World->GetTimerManager().SetTimer(ContinuousTimerHandle, this, &ABaseInteractable::TriggerStateEmission, 0.5f, true);
				World->GetTimerManager().SetTimer(ZoneExpiryTimerHandle, this, &ABaseInteractable::EndPersistentZone, ItemData.EmissionDuration, false);

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
						FString::Printf(TEXT("[%s] Strefa płonie przez %.1fs!"), *GetName(), ItemData.EmissionDuration));
				}
				return;
			}
		}
		return;
	}

	if (ItemData.bDestroyOnEmission && (ItemData.TriggerType == EEmissionTrigger::OnImpact ||
		ItemData.TriggerType == EEmissionTrigger::OnDestroy ||
		ItemData.TriggerType == EEmissionTrigger::TimedFuse ||
		ItemData.TriggerType == EEmissionTrigger::Proximity))
	{
		bIsBroken = true;
		Destroy();
	}
}

void ABaseInteractable::EndPersistentZone()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ContinuousTimerHandle);
		World->GetTimerManager().ClearTimer(ZoneExpiryTimerHandle);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, FString::Printf(TEXT("[%s] Strefa wygasła."), *GetName()));
	}

	if (ItemData.bDestroyOnEmission)
	{
		bIsBroken = true;
		Destroy();
	}
	else
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(GetRootComponent()))
		{
			Prim->SetVisibility(true);
			Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

void ABaseInteractable::DeactivateEmitter()
{
	ItemData.bIsStateEmitter = false;
	GetWorld()->GetTimerManager().ClearTimer(ContinuousTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(FuseTimerHandle);
}

float ABaseInteractable::CalculateMovementResistance(UPrimitiveComponent* MovingComponent)
{
	if (!MovingComponent) return 1.0f;

	float MassMultiplier = 1.0f;
	if (MovingComponent->IsSimulatingPhysics())
	{
		float ActualMass = FMath::Max(1.0f, MovingComponent->GetMass());
		float MassRatio = ReferenceMass / ActualMass;
		MassMultiplier = FMath::Clamp(MassRatio, 0.2f, 1.5f);
	}

	return MassMultiplier / FMath::Max(0.1f, MechanicalFriction);
}

bool ABaseInteractable::CanBePocketed_Implementation()
{
	if (!bCanBePocketed) return false;

	if (ReactionComp)
	{
		if (ReactionComp->ActiveStates.HasAny(BlockingHazardStates)) return false;
	}

	return true;
}

void ABaseInteractable::PickupObject_Implementation(AActor* InstigatorActor)
{
	if (!InstigatorActor) return;

	// Jeśli przedmiot nie jest przeznaczony do kieszeni (np. heavy szafa / drzwi) -> ignoruj
	if (!bCanBePocketed || !CanBePocketed_Implementation()) return;

	// Chowamy przedmiot do plecaka gracza:
	if (UInventoryComponent* InvComp = InstigatorActor->FindComponentByClass<UInventoryComponent>())
	{
		CaptureItemData();

		if (InvComp->TryAddItem(ItemData))
		{
			Destroy(); // Przechodzi ze świata 3D do bazy danych ekwipunku
		}
	}
}

bool ABaseInteractable::ConsumeObject_Implementation(AActor* InstigatorActor)
{
	if (!InstigatorActor || !bCanBePocketed || !bCanBeConsumed) return false;

	static const FGameplayTag OilTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Oil"), false);
	static const FGameplayTag BandageTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Bandage"), false);
	static const FGameplayTag WineTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Wine"), false);

	if (ItemData.ItemTag.MatchesTag(OilTag))
	{
		if (ULanternComponent* Lantern = InstigatorActor->FindComponentByClass<ULanternComponent>())
		{
			if (Lantern->CurrentFuel >= (Lantern->MaxFuel - 1.0f)) return false;
			Lantern->RefillFuel(ItemData.PrimaryValue);
			Destroy();
			return true;
		}
	}

	if (ItemData.ItemTag.MatchesTag(BandageTag))
	{
		if (UHealthComponent* Health = InstigatorActor->FindComponentByClass<UHealthComponent>())
		{
			if (Health->CurrentHealth >= Health->GetMaxHealth()) return false;
			Health->Heal(ItemData.PrimaryValue);
			Destroy();
			return true;
		}
	}

	if (ItemData.ItemTag.MatchesTag(WineTag))
	{
		if (USanityComponent* Sanity = InstigatorActor->FindComponentByClass<USanityComponent>())
		{
			if (Sanity->CurrentSanity >= Sanity->GetMaxSanity()) return false;
			Sanity->RestoreSanity(ItemData.PrimaryValue);
			Destroy();
			return true;
		}
	}

	return false;
}

void ABaseInteractable::CaptureItemData()
{
	if (HealthComp)
	{
		ItemData.SavedHealth = HealthComp->CurrentHealth;
		ItemData.bSavedCanBeDestroyed = HealthComp->bCanBeDestroyed;
		ItemData.SavedDamageThreshold = HealthComp->DamageThreshold;
		ItemData.SavedDamageSusceptibility = HealthComp->DamageSusceptibility;
	}

	if (UPrimitiveComponent* MeshComp = FindComponentByClass<UPrimitiveComponent>())
	{
		if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(MeshComp))
		{
			ItemData.ItemMesh = StaticMesh->GetStaticMesh();
		}
		ItemData.MeshScale = MeshComp->GetComponentScale();
	}

	if (!ItemData.DropClass)
	{
		ItemData.DropClass = GetClass();
	}
}

void ABaseInteractable::ApplyItemData(const FInventoryItemData& InData)
{
	ItemData = InData;
	bCanBePocketed = true;
	bCanBeConsumed = true;

	if (HealthComp)
	{
		if (InData.SavedHealth > 0.0f)
		{
			HealthComp->CurrentHealth = FMath::Min(InData.SavedHealth, HealthComp->GetMaxHealth());
		}
		else
		{
			HealthComp->CurrentHealth = HealthComp->GetMaxHealth();
		}

		HealthComp->bCanBeDestroyed = InData.bSavedCanBeDestroyed;
		HealthComp->DamageThreshold = InData.SavedDamageThreshold;
		HealthComp->DamageSusceptibility = InData.SavedDamageSusceptibility;
	}

	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(GetRootComponent());
	if (!Prim)
	{
		Prim = FindComponentByClass<UPrimitiveComponent>();
	}

	if (Prim)
	{
		Prim->SetWorldScale3D(InData.MeshScale);
		Prim->SetSimulatePhysics(true);
		Prim->SetUseCCD(true);
		Prim->BodyInstance.bUseCCD = true;
	}
}

EInteractionType ABaseInteractable::GetInteractionType_Implementation() { return InteractionType; }
EMouseAxis ABaseInteractable::GetPreferredMouseAxis_Implementation() { return PreferredMouseAxis; }

void ABaseInteractable::GrabObject_Implementation(AActor* Grabber)
{
	static const FGameplayTag HeavyPropTag = FGameplayTag::RequestGameplayTag(FName("Prop.Size.Heavy"), false);
	static const FGameplayTag HeavyLifterPerkTag = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.HeavyLifter"), false);

	if (PropSizeTag.MatchesTag(HeavyPropTag))
	{
		if (UProgressionComponent* ProgComp = Grabber->FindComponentByClass<UProgressionComponent>())
		{
			if (!ProgComp->HasPerk(HeavyLifterPerkTag))
			{
#if !UE_BUILD_SHIPPING
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[WIGOR] Zbyt ciężkie! Wymaga zdolności: Dźwigar (Wigor 1A)"));
#endif
				bIsHeld = false;
				return;
			}
		}
		else
		{
			// Jeśli chwytający nie ma w ogóle systemu progresji (np. skryptowa pułapka) - traktujemy jako słabego:
			bIsHeld = false;
			return;
		}

		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetRootComponent()))
		{
			RootPrim->SetSimulatePhysics(true);
			RootPrim->WakeRigidBody();
		}
	}

	bIsHeld = true;
}

void ABaseInteractable::ReleaseObject_Implementation() { bIsHeld = false; }
void ABaseInteractable::MoveObject_Implementation(float AxisDelta) {}
bool ABaseInteractable::IsLocked_Implementation()
{
	if (GateComp)
	{
		return GateComp->bIsLocked;
	}
	return false;
}
void ABaseInteractable::SetLocked_Implementation(bool bNewLocked)
{
	if (GateComp)
	{
		GateComp->bIsLocked = bNewLocked;
	}
}
bool ABaseInteractable::IsLatched_Implementation() { return bIsLatched; }
FGameplayTag ABaseInteractable::GetPropSizeTag_Implementation() { return PropSizeTag; }

void ABaseInteractable::SlamObject_Implementation(FVector PushDirection, float PushForce)
{
	bIsHeld = false;

	if (ItemData.bIsStateEmitter && ItemData.TriggerType == EEmissionTrigger::TimedFuse)
	{
		GetWorld()->GetTimerManager().SetTimer(FuseTimerHandle, this, &ABaseInteractable::TriggerStateEmission, ItemData.FuseTime, false);
	}
}

void ABaseInteractable::OnLockedInteraction_Implementation(AActor* InstigatorActor)
{
	// TA FUNKCJA JEST WYWOŁYWANA PRZEZ [LPM].
	// W Twoim Blueprincie (BP_BaseDoor) odpali się zdarzenie "Event OnLockedInteraction",
	// które odtworzy Twój efekt wibracji klamki i dźwięk!
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("🔒 [KLAMKA] Szarpnięcie zablokowanej klamki! Użyj [E] aby dopasować klucz."));
	}
#endif
}

void ABaseInteractable::TryUnlockFromInput_Implementation(AActor* InstigatorActor)
{
	ALightkeeperCharacter* Player = Cast<ALightkeeperCharacter>(InstigatorActor);
	if (!Player || !GateComp) return;

	// 1. Zczytujemy tag z lewej ręki LPM lub prawej dłoni FPP:
	FGameplayTag KeyTagFromHand = FGameplayTag::EmptyTag;
	ABaseInteractable* PhysicalKeyInHand = nullptr;

	if (Player->InteractionComp)
	{
		if (AActor* HeldActor = Player->InteractionComp->GetGrabbedActor())
		{
			if (ABaseInteractable* HeldProp = Cast<ABaseInteractable>(HeldActor))
			{
				KeyTagFromHand = HeldProp->ItemData.ItemTag;
				PhysicalKeyInHand = HeldProp;
			}
		}
	}

	if (!KeyTagFromHand.IsValid() && Player->ToolManagerComp && Player->ToolManagerComp->CurrentEquippedTool)
	{
		KeyTagFromHand = Player->ToolManagerComp->CurrentEquippedTool->ToolItemData.ItemTag;
	}

	// ====================================================================
	// SCENARIUSZ 1: DRZWI SĄ ZAMKNIĘTE -> PRÓBA OTWARCIA [E]:
	// ====================================================================
	if (GateComp->bIsLocked)
	{
		if (GateComp->TryUnlock(Player, KeyTagFromHand))
		{
			bIsLatched = false;

			// Jeśli użyliśmy fizycznego klucza z lewej ręki -> Chowamy go do plecaka!
			if (PhysicalKeyInHand)
			{
				PhysicalKeyInHand->CaptureItemData();
				if (UInventoryComponent* InvComp = Player->FindComponentByClass<UInventoryComponent>())
				{
					InvComp->TryAddItem(PhysicalKeyInHand->ItemData);
				}
				Player->InteractionComp->StopInteraction();
				PhysicalKeyInHand->Destroy();
			}

#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Green, TEXT("🔓 [SUKCES] Zamek otwarty!"));
#endif
			return;
		}
		else
		{
			// Odmowa dostępu na [E] -> Odpala też efekt Rattle w BP jako informację!
			IPhysicalInteract::Execute_OnLockedInteraction(this, InstigatorActor);
			return;
		}
	}

	// ====================================================================
	// SCENARIUSZ 2: DRZWI SĄ ODBLOKOWANE -> PRÓBA ZARYGLOWANIA [E]:
	// ====================================================================
	if (!GateComp->bIsLocked)
	{
		bool bRequiresClosedPosition = (InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation);

		if (bRequiresClosedPosition && !bIsLatched)
		{
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("⚠️ [ZAMEK] Musisz najpierw domknąć drzwi [LPM], by je zaryglować [E]!"));
#endif
			return;
		}

		// 1. Zaryglowanie pasującym kluczem z ręki (Dopasowanie + Schowanie):
		if (GateComp->RequiredKeyTag.IsValid() && KeyTagFromHand.MatchesTagExact(GateComp->RequiredKeyTag))
		{
			GateComp->bIsLocked = true;
			GateComp->bKeyDiscovered = true;
			bIsLatched = true;

			if (PhysicalKeyInHand)
			{
				PhysicalKeyInHand->CaptureItemData();
				if (UInventoryComponent* InvComp = Player->FindComponentByClass<UInventoryComponent>())
				{
					InvComp->TryAddItem(PhysicalKeyInHand->ItemData);
				}
				Player->InteractionComp->StopInteraction();
				PhysicalKeyInHand->Destroy();
			}

#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, TEXT("🔒 [ZARYGLOWANO] Dopasowano i zaryglowano zamek kluczem z dłoni!"));
#endif
			return;
		}

		// 2. Zaryglowanie kluczem z plecaka (Dla już ODKRYTYCH zamków):
		if (GateComp->bKeyDiscovered && GateComp->RequiredKeyTag.IsValid())
		{
			if (UInventoryComponent* InvComp = Player->FindComponentByClass<UInventoryComponent>())
			{
				if (InvComp->HasItemWithTag(GateComp->RequiredKeyTag))
				{
					GateComp->bIsLocked = true;
					bIsLatched = true;

#if !UE_BUILD_SHIPPING
					if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, TEXT("🔒 [ZARYGLOWANO] Zaryglowano zamek kluczem z plecaka!"));
#endif
					return;
				}
			}
		}
	}
}

bool ABaseInteractable::IsSmallProp_Implementation()
{
	return PropSizeTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Prop.Size.Small")));
}