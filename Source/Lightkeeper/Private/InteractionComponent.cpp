#include "InteractionComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner)
	{
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
				break;
			}
		}
	}
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CurrentCrosshairState = ECrosshairState::Default;

	// 2. Jeśli coś trzymamy w rękach, utrzymujemy odpowiedni kolor (np. żółty dla propów)
	if (GrabbedActor)
	{
		EInteractionType HeldType = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
		if (HeldType == EInteractionType::Grab_Free)
		{
			CurrentCrosshairState = ECrosshairState::Grabable;
		}
		else
		{
			CurrentCrosshairState = ECrosshairState::Interactive;
		}
	}
	// 3. Jeśli nic nie trzymamy, robimy Line Trace i sprawdzamy zasięgi
	else
	{
		FHitResult HitResult;
		// Wywołujemy nasz Line Trace
		if (PerformLineTrace(HitResult) && HitResult.GetActor())
		{
			AActor* HitActor = HitResult.GetActor();

			// SPRAWDZAMY CZY TRAFIONY OBIEKT MA INTERFEJS
			if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
			{
				// Opcjonalnie: upewniamy się, że odległość mieści się w naszym InteractionDistance
				// (PerformLineTrace już strzela na InteractionDistance, ale to dodatkowe zabezpieczenie)
				if (HitResult.Distance <= InteractionDistance)
				{
					EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(HitActor);

					if (Type == EInteractionType::Grab_Free)
					{
						CurrentCrosshairState = ECrosshairState::Grabable;     // Żółty
					}
					else
					{
						CurrentCrosshairState = ECrosshairState::Interactive; // Czerwony
					}
				}
			}
		}
	}

	if (GrabbedActor && HoldSlotComponent && PhysicsHandle)
	{
		EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

		if (Type == EInteractionType::Grab_Free)
		{
			if (UPrimitiveComponent* GrabbedComp = PhysicsHandle->GetGrabbedComponent())
			{
				AActor* Owner = GetOwner();
				FVector OwnerLoc = Owner->GetActorLocation();
				FVector DesiredHoldLoc = HoldSlotComponent->GetComponentLocation();

				// ====================================================================
				// 1. SPRAWDZANIE CZY OBIEKT NIE UTKNĄŁ ZA ŚCIANĄ
				// ====================================================================
				APlayerController* PC = GetWorld()->GetFirstPlayerController();
				FVector CameraLoc = (PC && PC->PlayerCameraManager) ? PC->PlayerCameraManager->GetCameraLocation() : OwnerLoc;

				float PropDistance = FVector::Dist(CameraLoc, GrabbedComp->GetComponentLocation());
				if (PropDistance > (InteractionDistance + BreakDistanceBuffer))
				{
					StopInteraction(); // Utknął -> puść
					return;
				}

				// ====================================================================
				// 2. STREFA BEZPIECZEŃSTWA NÓG (BLOKADA PRZECIĄGANIA POD SIEBIE!)
				// ====================================================================
				FVector FinalTargetLoc = DesiredHoldLoc;

				// Obliczamy dystans w poziomie (XY), ignorując wysokość (Z)
				FVector2D PlayerXY(OwnerLoc.X, OwnerLoc.Y);
				FVector2D HoldXY(DesiredHoldLoc.X, DesiredHoldLoc.Y);
				float HorizontalDist = FVector2D::Distance(PlayerXY, HoldXY);

				float MinKeepOutRadius = 30.0f; // Minimalny bezpieczny promień od środka gracza (w cm)

				bool bIsLookingDown = DesiredHoldLoc.Z < CameraLoc.Z;

				if (bIsLookingDown && HorizontalDist < MinKeepOutRadius)
				{
					// Wektor przodu gracza na płaszczyźnie poziomej
					FVector ForwardXY = Owner->GetActorForwardVector();
					ForwardXY.Z = 0.0f;
					ForwardXY = ForwardXY.GetSafeNormal();

					// Wypychamy cel dokładnie na krawędź 75 cm przed nasze buty!
					FVector SafeXY = OwnerLoc + (ForwardXY * MinKeepOutRadius);
					FinalTargetLoc.X = SafeXY.X;
					FinalTargetLoc.Y = SafeXY.Y;
				}

				// Blokada wbijania w podłogę pod stopy:
				float MinFloorZ = OwnerLoc.Z - 60.0f; // Poziom stóp
				if (FinalTargetLoc.Z < MinFloorZ)
				{
					FinalTargetLoc.Z = MinFloorZ;
				}

				// ====================================================================
				// 3. APILKUJEMY BEZPIECZNĄ POZYCJĘ DO PHYSICS HANDLE
				// ====================================================================
				FQuat TargetQuat = HoldSlotComponent->GetComponentTransform().GetRotation() * InitialGrabQuat;
				PhysicsHandle->SetTargetLocationAndRotation(FinalTargetLoc, TargetQuat.Rotator());
			}
		}
		else
		{
			// DLA MEBLI (Drzwi/Szuflady) -> Mierzymy dystans do ramy
			FVector TargetLocation = GrabbedComponent ? GrabbedComponent->GetComponentLocation() : GrabbedActor->GetActorLocation();
			float CurrentDistance = FVector::Dist(GetOwner()->GetActorLocation(), TargetLocation);

			if (CurrentDistance > (InteractionDistance + BreakDistanceBuffer))
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

	APlayerController* PC = Owner->GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return false;

	FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	FVector ForwardVector = PC->PlayerCameraManager->GetCameraRotation().Vector();
	FVector End = Start + (ForwardVector * InteractionDistance);

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	// ====================================================================
	// KROK 1: IDEALNIE PRECYZYJNA LINIA ZE ŚRODKA EKRANU
	// ====================================================================
	bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, TraceParams);

	if (bHit && OutHit.GetActor())
	{
		AActor* HitActor = OutHit.GetActor();
		if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			// Trafiłeś dokładnie tam, gdzie patrzysz (w środek celownika)! Zwracamy sukces.
			return true;
		}
	}

	// ====================================================================
	// KROK 2: MAŁY BOX SWEEP (Tylko jako pomocnik, gdy celujesz tuż obok / pod nogi)
	// ====================================================================
	// Używamy malutkiego pudła, żeby nie "łapało" obiektów z daleka w powietrzu:
	float SphereRadius = 3.0f; // Malutki promień (6 cm), żeby delikatnie pomagał, ale nie oszukiwał

	bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(SphereRadius),
		TraceParams
	);

	if (bHit && OutHit.GetActor())
	{
		AActor* HitActor = OutHit.GetActor();
		if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			return true;
		}
	}

	return false;
}

/* stary line trace
bool UInteractionComponent::PerformLineTrace(FHitResult& OutHit)
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	APlayerController* PC = Owner->GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return false;

	FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	FVector End = Start + (PC->PlayerCameraManager->GetCameraRotation().Vector() * InteractionDistance);

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, TraceParams);

}*/

void UInteractionComponent::StartInteraction()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FHitResult HitResult;
	if (PerformLineTrace(HitResult) && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();

		if (HitActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			if (IPhysicalInteract::Execute_IsLocked(HitActor))
			{
				// Obiekt odpowiada "Jestem zamknięty!" ➔ wywołujemy szarpanie
				IPhysicalInteract::Execute_OnLockedInteraction(HitActor, Owner);
				return; // Przerywamy chwytanie!
			}

			GrabbedActor = HitActor;

			GrabbedComponent = Cast<UPrimitiveComponent>(HitResult.GetComponent());

			// Odczytujemy typ obiektu z naszego C++ Enuma!
			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

			// ====================================================================
			// TWÓJ DOKŁADNY SWITCH Z KODU (1:1!)
			// ====================================================================
			switch (Type)
			{
			case EInteractionType::Hinge:
			case EInteractionType::Translation:
			case EInteractionType::Crank:
				// Drzwi, Szuflady, Zawory ➔ Zwykłe chwycenie klamki/uchwytu
				IPhysicalInteract::Execute_GrabObject(GrabbedActor, Owner);
				break;

			case EInteractionType::Grab_Free:
				IPhysicalInteract::Execute_GrabObject(GrabbedActor, Owner);

				if (PhysicsHandle)
				{
					UPrimitiveComponent* MeshToGrab = Cast<UPrimitiveComponent>(HitResult.GetComponent());
					if (MeshToGrab && HoldSlotComponent)
					{
						// ZAPAMIĘTUJEMY DOKŁADNY MESH, KTÓRY TRZYMAMY:
						GrabbedComponent = MeshToGrab;

						// Wyłączamy kolizję z graczem na czas trzymania
						OriginalPawnResponse = GrabbedComponent->GetCollisionResponseToChannel(ECC_Pawn);
						GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
						GrabbedComponent->IgnoreActorWhenMoving(Owner, true);

						// Zrzucamy z podłogi, jeśli stoisz na tym obiekcie
						ACharacter* CharOwner = Cast<ACharacter>(Owner);
						if (CharOwner && CharOwner->GetCharacterMovement())
						{
							if (CharOwner->GetCharacterMovement()->CurrentFloor.HitResult.GetActor() == GrabbedActor)
							{
								CharOwner->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
							}
						}

						FVector CenterOfMesh = GrabbedComponent->Bounds.Origin;
						FTransform HoldSlotTransform = HoldSlotComponent->GetComponentTransform();
						FTransform MeshTransform = GrabbedComponent->GetComponentTransform();

						InitialGrabQuat = HoldSlotTransform.GetRotation().Inverse() * MeshTransform.GetRotation();

						PhysicsHandle->GrabComponentAtLocationWithRotation(
							GrabbedComponent,
							NAME_None,
							CenterOfMesh,
							GrabbedComponent->GetComponentRotation()
						);
					}
				}
				break;

			case EInteractionType::Bolt:
				// Zasuwki, Włączniki ➔ Błyskawiczne kliknięcie i puszczenie!
				IPhysicalInteract::Execute_GrabObject(GrabbedActor, Owner);
				StopInteraction();
				break;
			}
		}
	}
}

void UInteractionComponent::StopInteraction()
{
	if (GrabbedActor)
	{
		// 1. NAJPIERW PRZYWRACAMY KOLIZJĘ (Zanim obiekt dostanie "Release" od interfejsu)
		if (GrabbedComponent)
		{
			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

			// ====================================================================
			// TŁUMIENIE PĘDU TYLKO DLA WOLNYCH PROPÓW (Puszki/Skrzynie)!
			// Dla drzwi/szuflad ten blok jest OMIJANY, więc drzwi zachowują swój płynny swing.
			// ====================================================================
			if (Type == EInteractionType::Grab_Free)
			{
				FVector SoftLinearVel = GrabbedComponent->GetPhysicsLinearVelocity() * 0.15f;
				FVector SoftAngularVel = GrabbedComponent->GetPhysicsAngularVelocityInDegrees() * 0.15f;

				SoftLinearVel = SoftLinearVel.GetClampedToMaxSize(120.0f);

				GrabbedComponent->SetPhysicsLinearVelocity(SoftLinearVel);
				GrabbedComponent->SetPhysicsAngularVelocityInDegrees(SoftAngularVel);
			}

			// PRZYWRACAMY DOKŁADNIE TAKĄ KOLIZJĘ, JAKA BYŁA W BLUEPRINTCIE:
			GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, OriginalPawnResponse);

			if (GetOwner())
			{
				GrabbedComponent->IgnoreActorWhenMoving(GetOwner(), false);
			}
		}

		// 2. Potem wywołujemy zwolnienie w obiekcie
		if (GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			IPhysicalInteract::Execute_ReleaseObject(GrabbedActor);
		}

		if (PhysicsHandle)
		{
			PhysicsHandle->ReleaseComponent();
		}

		// 3. Reset HoldSlota
		if (HoldSlotComponent)
		{
			HoldSlotComponent->SetRelativeLocation(InitialHoldSlotLocation);
			HoldSlotComponent->SetRelativeRotation(InitialHoldSlotRotation);
		}

		GrabbedComponent = nullptr;
		GrabbedActor = nullptr;
	}
}

void UInteractionComponent::SlamInteraction()
{
	AActor* Owner = GetOwner();

	if (GrabbedActor)
	{
		// 1. NAJPIERW PRZYWRACAMY KOLIZJĘ (Zanim obiekt dostanie "Slam" od interfejsu)
		if (GrabbedComponent)
		{
			// PRZYWRACAMY DOKŁADNIE TAKĄ KOLIZJĘ, JAKA BYŁA W BLUEPRINTCIE:
			GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, OriginalPawnResponse);

			if (GetOwner())
			{
				GrabbedComponent->IgnoreActorWhenMoving(GetOwner(), false);
			}
		}

		if (GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

			APlayerController* PC = Owner ? Owner->GetWorld()->GetFirstPlayerController() : nullptr;
			FVector Forward = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Vector() : Owner->GetActorForwardVector();

			if (Type == EInteractionType::Grab_Free)
			{
				if (PhysicsHandle) PhysicsHandle->ReleaseComponent();
				IPhysicalInteract::Execute_SlamObject(GrabbedActor, Forward);
			}
			else
			{
				IPhysicalInteract::Execute_SlamObject(GrabbedActor, Forward);
			}
		}

		// 2. Reset HoldSlota
		if (HoldSlotComponent)
		{
			HoldSlotComponent->SetRelativeLocation(InitialHoldSlotLocation);
			HoldSlotComponent->SetRelativeRotation(InitialHoldSlotRotation);
		}

		//GrabbedComponent = nullptr;
		GrabbedActor = nullptr;
	}
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

			// Klawisz 'E' działa TYLKO na szybkie akcje (Zasuwki, Przyciski, Zbieranie kluczy)
			if (Type == EInteractionType::Bolt)
			{
				// Wywołujemy błyskawiczne "kliknięcie"
				IPhysicalInteract::Execute_GrabObject(HitActor, GetOwner());
				IPhysicalInteract::Execute_ReleaseObject(HitActor);
			}
		}
	}
}

bool UInteractionComponent::ProcessMouseLook(float MouseX, float MouseY, float CameraSensitivity)
{
	if (bIsInspecting && IsValid(GrabbedActor) && HoldSlotComponent)
	{
		HoldSlotComponent->AddLocalRotation(FRotator(-MouseY * 3.0f, MouseX * 3.0f, 0.0f));
		return true; // Blokuj kamerę dla obracania w 3D
	}

	if (IsValid(GrabbedActor) && GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

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

			IPhysicalInteract::Execute_MoveObject(GrabbedActor, SelectedMouseDelta);
			return true;
		}

		if (Type == EInteractionType::Crank)
		{
			APlayerController* PC = GetOwner()->GetWorld()->GetFirstPlayerController();
			if (PC && GrabbedActor)
			{
				FVector2D WheelPos, MousePos;
				PC->ProjectWorldLocationToScreen(GrabbedActor->GetActorLocation(), WheelPos);
				PC->GetMousePosition(MousePos.X, MousePos.Y);
				FVector2D Dir = (MousePos - WheelPos).GetSafeNormal();

				float Torque = (Dir.X * MouseY) - (Dir.Y * MouseX);
				IPhysicalInteract::Execute_MoveObject(GrabbedActor, Torque);
				return true;
			}
		}
	}

	return false;
}

void UInteractionComponent::ZoomHoldSlot(float ScrollDelta)
{
	if (HoldSlotComponent && IsValid(GrabbedActor))
	{
		FVector Loc = HoldSlotComponent->GetRelativeLocation();
		Loc.X = FMath::Clamp(Loc.X + (ScrollDelta * 10.0f), 40.0f, 200.0f);
		HoldSlotComponent->SetRelativeLocation(Loc);
	}
}