#include "InteractionComponent.h"
#include "BaseInteractable.h"
#include "ProgressionComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LightkeeperCharacter.h"
#include "ToolManagerComponent.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bIsInspecting) return;

	AActor* Owner = GetOwner();
	if (Owner)
	{
		AddTickPrerequisiteActor(Owner);

		PhysicsHandle = Owner->FindComponentByClass<UPhysicsHandleComponent>();

		TArray<USceneComponent*> Comps;
		Owner->GetComponents<USceneComponent>(Comps);
		for (USceneComponent* Comp : Comps)
		{
			if (Comp && Comp->GetName().Contains(TEXT("HoldSlot")))
			{
				HoldSlotComponent = Comp;
				InitialHoldSlotLocation = HoldSlotComponent->GetRelativeLocation();
				InitialHoldSlotRotation = HoldSlotComponent->GetRelativeRotation();
				AddTickPrerequisiteComponent(HoldSlotComponent);
				break;
			}
		}
	}
}

APlayerCameraManager* UInteractionComponent::GetCameraManager()
{
	if (!CachedCameraManager)
	{
		if (AActor* Owner = GetOwner())
		{
			if (APlayerController* PC = Owner->GetWorld()->GetFirstPlayerController())
			{
				CachedCameraManager = PC->PlayerCameraManager;
			}
		}
	}
	return CachedCameraManager;
}

void UInteractionComponent::UpdateCrosshairState(EInteractionType HeldType, bool bIsHoldingObject)
{
	if (bIsHoldingObject)
	{
		CurrentCrosshairState = (HeldType == EInteractionType::Grab_Free) ? ECrosshairState::Grabable : ECrosshairState::Interactive;
		return;
	}

	FHitResult HitResult;
	if (PerformLineTrace(HitResult) && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			if (HitResult.Distance <= InteractionDistance)
			{
				EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(HitActor);
				CurrentCrosshairState = (Type == EInteractionType::Grab_Free) ? ECrosshairState::Grabable : ECrosshairState::Interactive;
				return;
			}
		}
	}

	CurrentCrosshairState = ECrosshairState::Default;
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner) return;

	const bool bHolding = (GrabbedActor != nullptr);
	const EInteractionType HeldType = bHolding ? IPhysicalInteract::Execute_GetInteractionType(GrabbedActor) : EInteractionType::Grab_Free;

	// 1. Aktualizacja celownika
	UpdateCrosshairState(HeldType, bHolding);

	// 2. Fizyka trzymania
	if (bHolding && HoldSlotComponent && PhysicsHandle)
	{
		APlayerCameraManager* CamMgr = GetCameraManager();
		const FVector OwnerLoc = Owner->GetActorLocation();
		const FVector CameraLoc = CamMgr ? CamMgr->GetCameraLocation() : OwnerLoc;
		const FVector CameraForward = CamMgr ? CamMgr->GetCameraRotation().Vector() : Owner->GetActorForwardVector();

		if (HeldType == EInteractionType::Grab_Free)
		{
			if (UPrimitiveComponent* GrabbedComp = PhysicsHandle->GetGrabbedComponent())
			{
				const float ObjectRadius = GrabbedComp->Bounds.SphereRadius;

				// Zwiększony bufor zerwania, aby gwałtowne ruchy myszą nie powodowały upuszczenia
				const float DynamicMaxLag = ObjectRadius + BreakDistanceBuffer + 80.0f;
				const float LagDistSq = FVector::DistSquared(GrabbedComp->GetComponentLocation(), SmoothedHoldLocation);

				if (LagDistSq > (DynamicMaxLag * DynamicMaxLag))
				{
					// Dłuższy czas (0.75s) – puszczamy tylko przy rzeczywistym zablokowaniu za przeszkodą
					ObstacleLagTimer += DeltaTime;
					if (ObstacleLagTimer > 0.75f)
					{
						StopInteraction();
						ObstacleLagTimer = 0.0f;
						return;
					}
				}
				else
				{
					ObstacleLagTimer = FMath::Max(0.0f, ObstacleLagTimer - (DeltaTime * 3.0f));
				}

				ACharacter* Char = Cast<ACharacter>(Owner);
				const float CapsuleRadius = Char && Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleRadius() : 45.0f;

				// Minimalny dystans trzymania, uniemożliwiający wejście obiektu w gracza
				float DesiredForwardDistance = HoldSlotComponent->GetRelativeLocation().X;
				const float MinSafeDist = CapsuleRadius + (ObjectRadius * 0.6f) + 15.0f;
				DesiredForwardDistance = FMath::Max(DesiredForwardDistance, MinSafeDist);

				// Płynna interpolacja dystansu i pozycji docelowej
				SmoothedHoldDistance = FMath::FInterpTo(SmoothedHoldDistance, DesiredForwardDistance, DeltaTime, 15.0f);
				FVector DesiredHoldLoc = CameraLoc + (CameraForward * SmoothedHoldDistance);

				SmoothedHoldLocation = FMath::VInterpTo(SmoothedHoldLocation, DesiredHoldLoc, DeltaTime, 25.0f);

				// ====================================================================
				// TŁUMIK WIROWANIA PRZY KOLIZJACH (BLOKADA SKRĘCANIA W RĘKACH):
				// ====================================================================
				FVector AngularVel = GrabbedComp->GetPhysicsAngularVelocityInDegrees();
				const float MaxAngularSpeedDeg = 60.0f;
				if (AngularVel.SizeSquared() > (MaxAngularSpeedDeg * MaxAngularSpeedDeg))
				{
					GrabbedComp->SetPhysicsAngularVelocityInDegrees(AngularVel.GetClampedToMaxSize(MaxAngularSpeedDeg));
				}

				FQuat TargetQuat = HoldSlotComponent->GetComponentTransform().GetRotation() * InitialGrabQuat;
				TargetQuat.Normalize();
				PhysicsHandle->SetTargetLocationAndRotation(SmoothedHoldLocation, TargetQuat.Rotator());
			}
		}
		else
		{
			// Drzwi / Mebel – sprawdzamy dystans względem kamery, nie stóp gracza
			const FVector TargetLocation = GrabbedComponent ? GrabbedComponent->GetComponentLocation() : GrabbedActor->GetActorLocation();
			const float MaxAllowedDistance = InteractionDistance + BreakDistanceBuffer + 30.0f;
			if (FVector::DistSquared(CameraLoc, TargetLocation) > (MaxAllowedDistance * MaxAllowedDistance))
			{
				StopInteraction();
			}
		}
	}
}

bool UInteractionComponent::PerformLineTrace(FHitResult& OutHit)
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	APlayerCameraManager* CamMgr = GetCameraManager();
	if (!CamMgr) return false;

	const FVector Start = CamMgr->GetCameraLocation();
	const FVector ForwardVector = CamMgr->GetCameraRotation().Vector();
	const FVector End = Start + (ForwardVector * InteractionDistance);

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	if (GrabbedActor)
	{
		TraceParams.AddIgnoredActor(GrabbedActor);
	}

	bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, TraceParams);
	if (bHit && OutHit.GetActor() && OutHit.GetActor()->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		return true;
	}

	const float SphereRadius = 3.0f;
	bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(SphereRadius),
		TraceParams
	);

	return (bHit && OutHit.GetActor() && OutHit.GetActor()->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()));
}

void UInteractionComponent::StartInteraction()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (UToolManagerComponent* ToolMgr = Owner->FindComponentByClass<UToolManagerComponent>())
	{
		if (ToolMgr->IsAiming())
		{
			return;
		}
	}

	FHitResult HitResult;
	if (PerformLineTrace(HitResult) && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();

		if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			if (IPhysicalInteract::Execute_IsLocked(HitActor))
			{
				// [LPM] Odpala Twój ORYGINALNY EFEKT w BP_BaseDoor:
				IPhysicalInteract::Execute_OnLockedInteraction(HitActor, Owner);
				return; // Nie chwyta klamki!
			}

			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(HitActor);

			switch (Type)
			{
			case EInteractionType::Hinge:
			case EInteractionType::Translation:
			case EInteractionType::Crank:
				GrabbedActor = HitActor;
				GrabbedComponent = Cast<UPrimitiveComponent>(HitResult.GetComponent());
				IPhysicalInteract::Execute_GrabObject(GrabbedActor, Owner);
				break;

			case EInteractionType::Grab_Free:
			{
				UPrimitiveComponent* MeshToGrab = Cast<UPrimitiveComponent>(HitResult.GetComponent());
				if (!MeshToGrab || !HoldSlotComponent) return;

				// ====================================================================
				// 1. CZYSTA WERYFIKACJA WIGORU (ZERO RZUTOWANIA NA GRACZA!):
				// Pobieramy Tag gabarytu przez Interfejs!
				// ====================================================================
				FGameplayTag PropSize = IPhysicalInteract::Execute_GetPropSizeTag(HitActor);
				static const FGameplayTag HeavyPropTag = FGameplayTag::RequestGameplayTag(FName("Prop.Size.Heavy"), false);

				if (PropSize.MatchesTag(HeavyPropTag))
				{
					bool bCanLiftHeavy = false;
					// Pytamy tylko, czy nasz Owner posiada komponent progresji:
					if (UProgressionComponent* ProgComp = Owner->FindComponentByClass<UProgressionComponent>())
					{
						static const FGameplayTag HeavyLifterTag = FGameplayTag::RequestGameplayTag(FName("Perk.Vigor.HeavyLifter"), false);
						bCanLiftHeavy = ProgComp->HasPerk(HeavyLifterTag);
					}

					// Jeśli nie ma Perka (lub w ogóle nie ma systemu progresji) -> ODRZUCAMY CHWYT!
					if (!bCanLiftHeavy)
					{
#if !UE_BUILD_SHIPPING
						if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("❌ [WIGOR] Barykada jest zbyt ciężka! Wymaga Perka: Dźwigar."));
#endif
						return; // CAŁKOWITE PRZERWANIE FUNKCJI! PhysicsHandle nie dotknie obiektu!
					}
				}

				GrabbedActor = HitActor;
				GrabbedComponent = MeshToGrab;

				IPhysicalInteract::Execute_GrabObject(GrabbedActor, Owner);

				if (PhysicsHandle)
				{
					if (!GrabbedComponent->IsSimulatingPhysics())
					{
						GrabbedComponent->SetSimulatePhysics(true);
					}

					GrabbedComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
					GrabbedComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

					OriginalPawnResponse = GrabbedComponent->GetCollisionResponseToChannel(ECC_Pawn);
					GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
					GrabbedComponent->IgnoreActorWhenMoving(Owner, true);

					// ====================================================================
					// 2. IGNOROWANIE KAPSUŁY (BEZ CASTOWANIA NA ACharacter!):
					// ====================================================================
					if (UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>())
					{
						Capsule->IgnoreActorWhenMoving(GrabbedActor, true);
						GrabbedComponent->IgnoreComponentWhenMoving(Capsule, true);
					}

					const float ObjectRadius = GrabbedComponent->Bounds.SphereRadius;
					CurrentBaseHoldDistance = FMath::Clamp(75.0f + ObjectRadius, 90.0f, 300.0f);
					SmoothedHoldDistance = CurrentBaseHoldDistance;

					FVector InitLoc = InitialHoldSlotLocation;
					InitLoc.X = CurrentBaseHoldDistance;
					HoldSlotComponent->SetRelativeLocation(InitLoc);

					SmoothedHoldLocation = GrabbedComponent->GetComponentLocation();

					// (Tutaj zostaje jedyny wymuszony cast na ALightkeeperCharacter dla UpdateMovementSpeed, 
					//  ponieważ to w 100% unikalna funkcja klasy gracza)
					if (ALightkeeperCharacter* Char = Cast<ALightkeeperCharacter>(Owner))
					{
						Char->UpdateMovementSpeed();
						if (Char->GetCharacterMovement() && Char->GetCharacterMovement()->CurrentFloor.HitResult.GetActor() == GrabbedActor)
						{
							Char->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
						}
					}

					FVector CenterOfMesh = GrabbedComponent->Bounds.Origin;
					FTransform HoldSlotTransform = HoldSlotComponent->GetComponentTransform();
					FTransform MeshTransform = GrabbedComponent->GetComponentTransform();

					InitialGrabQuat = HoldSlotTransform.GetRotation().Inverse() * MeshTransform.GetRotation();

					GrabbedComponent->SetLinearDamping(1.5f);
					GrabbedComponent->SetAngularDamping(25.0f);

					const float ActualMass = FMath::Max(1.0f, GrabbedComponent->GetMass());
					const float MassFactor = FMath::Clamp(1.0f - (ActualMass / 150.0f), 0.35f, 1.0f);

					PhysicsHandle->LinearStiffness = 1500.0f * MassFactor;
					PhysicsHandle->LinearDamping = 35.0f * MassFactor;

					PhysicsHandle->AngularStiffness = 2500.0f;
					PhysicsHandle->AngularDamping = 180.0f;
					PhysicsHandle->InterpolationSpeed = 50.0f;

					PhysicsHandle->GrabComponentAtLocationWithRotation(
						GrabbedComponent,
						NAME_None,
						CenterOfMesh,
						GrabbedComponent->GetComponentRotation()
					);
				}
				break;
			}

			case EInteractionType::Bolt:
				GrabbedActor = HitActor;
				GrabbedComponent = Cast<UPrimitiveComponent>(HitResult.GetComponent());
				IPhysicalInteract::Execute_GrabObject(GrabbedActor, Owner);
				StopInteraction();
				break;
			}
		}
	}
}

FVector UInteractionComponent::CalculateThrowVelocity(const FVector& Direction, float ObjectMass, float ChargeMultiplier) const
{
	float SafeMass = FMath::Max(1.0f, ObjectMass);

	// ====================================================================
	// UJEDNOLICONA PRĘDKOŚĆ RZUTU: 
	// Lekki przedmiot (1kg) = 950 cm/s, Średnia skrzynia (20kg) = 750 cm/s, Ciężka (100kg) = 500 cm/s
	// ====================================================================
	float MassFactor = FMath::Clamp(1.0f - (FMath::Sqrt(SafeMass) / 25.0f), 0.45f, 1.2f);

	float ThrowMod = 1.0f;
	if (AActor* Owner = GetOwner())
	{
		if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
		{
			ThrowMod = Health->GetThrowPowerMultiplier();
		}
	}

	// Bazowa siła rzutu = 850 cm/s:
	float FinalPower = (850.0f * ChargeMultiplier * ThrowMod) * MassFactor;

	FVector FinalThrowVelocity = Direction.GetSafeNormal() * FinalPower;
	FinalThrowVelocity.Z += 40.0f;

	return FinalThrowVelocity;
}

void UInteractionComponent::StopInteraction()
{
	if (GrabbedActor)
	{
		AActor* Owner = GetOwner();

		if (GrabbedComponent)
		{
			GrabbedComponent->SetLinearDamping(0.01f);
			GrabbedComponent->SetAngularDamping(0.0f);

			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
			if (Type == EInteractionType::Grab_Free)
			{
				GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				GrabbedComponent->IgnoreActorWhenMoving(Owner, false);

				// ====================================================================
				// CZYSTE PRZYWRÓCENIE KOLIZJI (BEZ CASTOWANIA):
				// ====================================================================
				if (UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>())
				{
					Capsule->IgnoreActorWhenMoving(GrabbedActor, false);
				}

				FVector DropNudge = Owner->GetActorForwardVector() * 35.0f;
				DropNudge.Z = -10.0f;
				GrabbedComponent->SetPhysicsLinearVelocity(DropNudge);
				GrabbedComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			}
			GrabbedComponent = nullptr;
		}

		if (GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			IPhysicalInteract::Execute_ReleaseObject(GrabbedActor);
		}

		if (PhysicsHandle) PhysicsHandle->ReleaseComponent();

		CleanupInteraction();
	}
}

void UInteractionComponent::SlamInteraction(float CustomMultiplier)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (!GrabbedActor)
	{
		FHitResult HitResult;
		if (PerformLineTrace(HitResult) && HitResult.GetActor())
		{
			if (HitResult.GetActor()->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
			{
				GrabbedActor = HitResult.GetActor();
				GrabbedComponent = Cast<UPrimitiveComponent>(HitResult.GetComponent());
			}
		}
	}

	if (GrabbedActor && GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		APlayerCameraManager* CamMgr = GetCameraManager();
		const FVector Forward = CamMgr ? CamMgr->GetCameraRotation().Vector() : Owner->GetActorForwardVector();
		EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

		if (Type == EInteractionType::Grab_Free)
		{
			if (PhysicsHandle)
			{
				PhysicsHandle->ReleaseComponent();
			}

			if (GrabbedComponent)
			{
				GrabbedComponent->SetLinearDamping(0.05f);
				GrabbedComponent->SetAngularDamping(0.1f);

				GrabbedComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
				GrabbedComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

				float ObjectMass = (GrabbedComponent && GrabbedComponent->IsSimulatingPhysics()) ? FMath::Max(1.0f, GrabbedComponent->GetMass()) : 15.0f;

				GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				GrabbedComponent->IgnoreActorWhenMoving(Owner, false);

				// ====================================================================
				// CZYSTE ODPYTYWANIE KOMPONENTÓW (BEZ CASTOWANIA):
				// ====================================================================
				if (UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>())
				{
					Capsule->IgnoreActorWhenMoving(GrabbedActor, false);
				}

				if (UStaminaComponent* StaminaComp = Owner->FindComponentByClass<UStaminaComponent>())
				{
					float StaminaCost = FMath::Clamp((5.0f + (ObjectMass * 0.9f)) * CustomMultiplier, 5.0f, 60.0f);
					StaminaComp->TryConsumeStamina(StaminaCost);
				}

				FVector FinalThrowVelocity = CalculateThrowVelocity(Forward, ObjectMass, CustomMultiplier);
				GrabbedComponent->AddImpulse(FinalThrowVelocity, NAME_None, true);

				GrabbedComponent = nullptr;
			}

			IPhysicalInteract::Execute_SlamObject(GrabbedActor, Forward, BaseThrowPower * CustomMultiplier);
		}
		else
		{
			GrabbedComponent = nullptr;
			IPhysicalInteract::Execute_SlamObject(GrabbedActor, Forward, BaseThrowPower * CustomMultiplier);
		}

		CleanupInteraction();
	}
}

void UInteractionComponent::StartSlamCharge()
{
	SlamChargeStartTime = GetWorld()->GetTimeSeconds();
}

void UInteractionComponent::ReleaseSlamThrow()
{
	if (SlamChargeStartTime <= 0.0f)
	{
		SlamInteraction(1.0f);
		return;
	}

	float ChargeDuration = FMath::Clamp(GetWorld()->GetTimeSeconds() - SlamChargeStartTime, 0.0f, MaxChargeTime);
	float ChargeAlpha = ChargeDuration / FMath::Max(0.1f, MaxChargeTime);
	float FinalMultiplier = FMath::Lerp(1.0f, MaxChargedThrowMultiplier, ChargeAlpha);

	SlamChargeStartTime = 0.0f;

	if (FinalMultiplier > 1.2f)
	{
		if (AActor* Owner = GetOwner())
		{
			if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
			{
				float Pain = Health->GetChargedThrowPainCost();
				if (Pain > 0.0f)
				{
					static const FGameplayTag BluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
					Health->TakeDamage(Pain, BluntTag);

#if !UE_BUILD_SHIPPING
					if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
						FString::Printf(TEXT("⚠️ [BÓL] Naładowany rzut uszkodził złamaną rękę! -%.1f HP"), Pain));
#endif
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (GEngine && FinalMultiplier > 1.2f)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Purple,
			FString::Printf(TEXT("[ŁADOWANY RZUT] Wyrzucono z siłą x%.2f!"), FinalMultiplier));
	}
#endif

	SlamInteraction(FinalMultiplier);
}

void UInteractionComponent::QuickInteraction()
{
	FHitResult HitResult;
	if (PerformLineTrace(HitResult) && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(HitActor);

			if (Type == EInteractionType::Bolt)
			{
				IPhysicalInteract::Execute_GrabObject(HitActor, GetOwner());
				IPhysicalInteract::Execute_ReleaseObject(HitActor);
			}
		}
	}
}

bool UInteractionComponent::ProcessMouseLook(float MouseX, float MouseY, float CameraSensitivity)
{
	float ArmResistance = 1.0f;
	if (AActor* Owner = GetOwner())
	{
		if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
		{
			ArmResistance = Health->GetMouseResistanceMultiplier();
		}
	}

	// 1. TRYB INSPEKCJI 3D
	if (bIsInspecting && IsValid(GrabbedActor) && HoldSlotComponent)
	{
		const float ObjectMass = (GrabbedComponent && GrabbedComponent->IsSimulatingPhysics())
			? GrabbedComponent->GetMass()
			: 1.0f;
		const float InspectSensitivity = FMath::Clamp(1.0f / FMath::Max(1.0f, ObjectMass * InspectWeightMultiplier * ArmResistance), 0.015f, 0.6f);

		const float PitchAngle = FMath::DegreesToRadians(-MouseY * InspectSensitivity * 2.5f);
		const float YawAngle = FMath::DegreesToRadians(-MouseX * InspectSensitivity * 2.5f);

		const FQuat PitchQuat(FVector::RightVector, PitchAngle);
		const FQuat YawQuat(FVector::UpVector, YawAngle);
		const FQuat DeltaQuat = PitchQuat * YawQuat;

		const FQuat CurrentQuat = HoldSlotComponent->GetRelativeRotation().Quaternion();
		FQuat TargetQuat = DeltaQuat * CurrentQuat;
		TargetQuat.Normalize();

		HoldSlotComponent->SetRelativeRotation(TargetQuat);
		return true;
	}

	// 2. FIZYCZNA MANIPULACJA MEBLAMI
	if (IsValid(GrabbedActor) && GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
		float CurrentEffortDelta = 0.0f;

		// A. DRZWI I SZUFLADY
		if (Type == EInteractionType::Hinge || Type == EInteractionType::Translation)
		{
			EMouseAxis PreferredAxis = IPhysicalInteract::Execute_GetPreferredMouseAxis(GrabbedActor);
			float SelectedMouseDelta = 0.0f;

			switch (PreferredAxis)
			{
			case EMouseAxis::MouseX:     SelectedMouseDelta = MouseX; break;
			case EMouseAxis::MouseY:     SelectedMouseDelta = MouseY; break;
			case EMouseAxis::InvertedX: SelectedMouseDelta = -MouseX; break;
			case EMouseAxis::InvertedY: SelectedMouseDelta = -MouseY; break;

			case EMouseAxis::Auto_CameraRelative:
			{
				APlayerController* PC = GetOwner()->GetWorld()->GetFirstPlayerController();
				FVector CamFwd = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Vector() : GetOwner()->GetActorForwardVector();

				FVector ObjFwd = GrabbedActor->GetActorForwardVector();
				FVector ObjRight = GrabbedActor->GetActorRightVector();

				float DotForward = FVector::DotProduct(CamFwd, ObjFwd);
				float DotRight = FVector::DotProduct(CamFwd, ObjRight);

				if (FMath::Abs(DotForward) > FMath::Abs(DotRight))
				{
					float Sign = (DotForward < 0.0f) ? -1.0f : 1.0f;
					SelectedMouseDelta = MouseX * Sign;
				}
				else
				{
					float Sign = (DotRight < 0.0f) ? 1.0f : -1.0f;
					SelectedMouseDelta = MouseY * Sign;
				}
				break;
			}
			}

			float FinalDelta = SelectedMouseDelta / ArmResistance;
			CurrentEffortDelta = FMath::Abs(SelectedMouseDelta);

			IPhysicalInteract::Execute_MoveObject(GrabbedActor, FinalDelta);
		}
		// B. KOŁA I ZAWORY
		else if (Type == EInteractionType::Crank)
		{
			APlayerController* PC = GetOwner()->GetWorld()->GetFirstPlayerController();
			if (PC)
			{
				FVector2D WheelPos, MousePos;
				PC->ProjectWorldLocationToScreen(GrabbedActor->GetActorLocation(), WheelPos);
				PC->GetMousePosition(MousePos.X, MousePos.Y);
				FVector2D Dir = (MousePos - WheelPos).GetSafeNormal();

				float Torque = (Dir.X * MouseY) - (Dir.Y * MouseX);
				float FinalTorque = Torque / ArmResistance;
				CurrentEffortDelta = FMath::Abs(Torque * 1.5f);

				IPhysicalInteract::Execute_MoveObject(GrabbedActor, FinalTorque);
			}
		}

		// NALICZANIE BÓLU PRZY WYSIŁKU
		if (CurrentEffortDelta > 0.0f)
		{
			AccumulatedMechanismEffort += CurrentEffortDelta;

			float CurrentTime = GetWorld()->GetTimeSeconds();
			bool bCooldownPassed = (CurrentTime - LastMechanismPainTime) >= 2.0f;
			const float PainEffortThreshold = (Type == EInteractionType::Crank) ? 650.0f : 450.0f;

			if (AccumulatedMechanismEffort >= PainEffortThreshold && bCooldownPassed)
			{
				AccumulatedMechanismEffort = 0.0f;
				LastMechanismPainTime = CurrentTime;

				if (AActor* Owner = GetOwner())
				{
					if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
					{
						float ArmPain = Health->GetActionPainCost(EAnatomicalLimb::LeftArm);
						if (ArmPain > 0.0f)
						{
							static const FGameplayTag BluntTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Blunt"), false);
							Health->TakeDamage(ArmPain, BluntTag);

#if !UE_BUILD_SHIPPING
							if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
								FString::Printf(TEXT("⚠️ [BÓL] Ciągły wysiłek przy meblu/zaworze! -%.1f HP"), ArmPain));
#endif
						}
					}
				}
			}

			return true;
		}
	}

	return false;
}

void UInteractionComponent::CleanupInteraction()
{
	bIsInspecting = false;
	AccumulatedMechanismEffort = 0.0f;
	LastMechanismPainTime = 0.0f;

	if (ALightkeeperCharacter* Char = Cast<ALightkeeperCharacter>(GetOwner()))
	{
		UStaminaComponent* Stamina = Char->FindComponentByClass<UStaminaComponent>();
		if (Stamina && Stamina->bWantsToSprint && !Char->bIsCrouched)
		{
			Stamina->StartSprint();
		}
		else
		{
			Char->UpdateMovementSpeed();
		}
	}

	if (HoldSlotComponent)
	{
		HoldSlotComponent->SetRelativeLocation(InitialHoldSlotLocation);
		HoldSlotComponent->SetRelativeRotation(InitialHoldSlotRotation);
	}

	GrabbedActor = nullptr;
}

void UInteractionComponent::ZoomHoldSlot(float ScrollDelta)
{
	if (HoldSlotComponent && IsValid(GrabbedActor) && GrabbedComponent)
	{
		FVector Loc = HoldSlotComponent->GetRelativeLocation();

		ACharacter* Char = Cast<ACharacter>(GetOwner());
		const float CapsuleRadius = Char && Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleRadius() : 45.0f;
		const float ObjectRadius = GrabbedComponent->Bounds.SphereRadius;

		const float MinZoom = CapsuleRadius + ObjectRadius + 8.0f;
		const float MaxZoom = FMath::Max(MinZoom + 70.0f, InteractionDistance + (ObjectRadius * 0.4f));

		Loc.X = FMath::Clamp(Loc.X + (ScrollDelta * 18.0f), MinZoom, MaxZoom);
		Loc.Y = 0.0f;

		float ZoomAlpha = FMath::GetRangePct(MaxZoom, MinZoom, Loc.X);
		Loc.Z = FMath::Lerp(0.0f, -22.0f, ZoomAlpha);

		HoldSlotComponent->SetRelativeLocation(Loc);
	}
}

float UInteractionComponent::CalculateMovementSpeed(float BaseSpeed, float MassInKg) const
{
	float EffectiveMass = MassInKg;
	float SpeedMultiplier = 1.0f - (EffectiveMass * WeightSpeedReductionFactor);
	SpeedMultiplier = FMath::Clamp(SpeedMultiplier, MinCarryingSpeedRatio, 1.0f);

	return BaseSpeed * SpeedMultiplier;
}

void UInteractionComponent::ToggleInspectMode()
{
	if (!GrabbedActor) return;

	EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
	if (Type != EInteractionType::Grab_Free) return;

	bIsInspecting = !bIsInspecting;
}

void UInteractionComponent::TryPickupFocusedObject()
{
	FHitResult Hit;
	bool bHit = PerformLineTrace(Hit);
	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;

	// ====================================================================
	// SCENARIUSZ A: CELUJEMY W DRZWI / ZAMEK [E] -> Otwieranie LUB Ryglowanie!
	// ====================================================================
	if (HitActor && HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		// Czyste zapytanie czy to zamek (jeśli nie, funkcja zostanie zignorowana w meblu):
		IPhysicalInteract::Execute_TryUnlockFromInput(HitActor, GetOwner());
		return;
	}

	// ====================================================================
	// SCENARIUSZ B: Trzymamy mały przedmiot w dłoni i wciskamy [E] w pustkę -> Chowamy do plecaka
	// ====================================================================
	if (GrabbedActor)
	{
		if (GrabbedActor->Implements<UPhysicalInteract>())
		{
			if (IPhysicalInteract::Execute_CanBePocketed(GrabbedActor))
			{
				AActor* ActorToPocket = GrabbedActor;
				StopInteraction();
				IPhysicalInteract::Execute_PickupObject(ActorToPocket, GetOwner());
				return;
			}
		}
		return;
	}

	// ====================================================================
	// SCENARIUSZ C: Puste ręce -> Podniesienie przedmiotu do plecaka
	// ====================================================================
	if (HitActor)
	{
		IPhysicalInteract::Execute_PickupObject(HitActor, GetOwner());
	}
}

void UInteractionComponent::TryQuickConsumeFocusedObject()
{
	FHitResult Hit;

	if (PerformLineTrace(Hit) && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor->Implements<UPhysicalInteract>())
		{
			IPhysicalInteract::Execute_ConsumeObject(HitActor, GetOwner());
		}
	}
}