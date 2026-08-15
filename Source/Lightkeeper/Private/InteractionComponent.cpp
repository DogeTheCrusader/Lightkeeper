#include "InteractionComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LightkeeperCharacter.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bIsInspecting) return;

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

				if (!bIsInspecting)
				{
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
				else
				{
					FQuat TargetQuat = HoldSlotComponent->GetComponentTransform().GetRotation() * InitialGrabQuat;
					PhysicsHandle->SetTargetLocationAndRotation(HoldSlotComponent->GetComponentLocation(), TargetQuat.Rotator());
				}
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

						float ObjectRadius = GrabbedComponent->Bounds.SphereRadius;
						CurrentBaseHoldDistance = FMath::Clamp(90.0f + ObjectRadius, 110.0f, 220.0f);

						FVector InitLoc = InitialHoldSlotLocation;
						InitLoc.X = CurrentBaseHoldDistance;
						HoldSlotComponent->SetRelativeLocation(InitLoc);

						// ====================================================================
						// 2. DYNAMICZNA PRĘDKOŚĆ CHODU Z WAGI PRZEDMIOTU:
						// ====================================================================

						float PropMass = GrabbedComponent->GetMass();

						if (ALightkeeperCharacter* Char = Cast<ALightkeeperCharacter>(Owner))
						{
							// Jeśli gracz właśnie podniósł coś cięższego niż 5 kg:
							if (PropMass > 5.0f)
							{
								// 1. Wyłączamy bieg w StaminaComponent (jeśli postać sprintowała):
								if (UStaminaComponent* Stamina = Char->FindComponentByClass<UStaminaComponent>())
								{
									Stamina->StopSprint(); // Zdejmuje flagę bIsSprinting!
								}
							}

							// 2. Przeliczamy prędkość (uwzględniając wagę)
							Char->UpdateMovementSpeed();

							// Zrzucenie z podłogi jeśli gracz stał na skrzyni
							if (Char->GetCharacterMovement() && Char->GetCharacterMovement()->CurrentFloor.HitResult.GetActor() == GrabbedActor)
							{
								Char->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
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
		if (GrabbedComponent)
		{
			EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
			if (Type == EInteractionType::Grab_Free)
			{
				FVector SoftLinearVel = GrabbedComponent->GetPhysicsLinearVelocity() * 0.15f;
				FVector SoftAngularVel = GrabbedComponent->GetPhysicsAngularVelocityInDegrees() * 0.15f;
				SoftLinearVel = SoftLinearVel.GetClampedToMaxSize(120.0f);
				GrabbedComponent->SetPhysicsLinearVelocity(SoftLinearVel);
				GrabbedComponent->SetPhysicsAngularVelocityInDegrees(SoftAngularVel);

				// Przywracamy kolizję TYLKO dla noszonego propa:
				GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, OriginalPawnResponse);
			}
			GrabbedComponent = nullptr;
		}

		if (GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
		{
			IPhysicalInteract::Execute_ReleaseObject(GrabbedActor);
		}

		if (PhysicsHandle) PhysicsHandle->ReleaseComponent();

		// ==========================================================
		// JEDNA CZYSTA LINIJKA CZYSZCZĄCA NA KOŃCU:
		// ==========================================================
		CleanupInteraction();
	}
}

void UInteractionComponent::SlamInteraction()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Jeśli nic nie trzymamy, robimy rzut/wyważenie celując w obiekt z odległości
	if (!GrabbedActor)
	{
		FHitResult HitResult;
		if (PerformLineTrace(HitResult) && HitResult.GetActor())
		{
			if (HitResult.GetActor()->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
			{
				GrabbedActor = HitResult.GetActor();
				GrabbedComponent = Cast<UPrimitiveComponent>(HitResult.GetComponent()); // Zapisujemy komponent!
			}
		}
	}

	if (GrabbedActor && GrabbedActor->GetClass()->ImplementsInterface(UPhysicalInteract::StaticClass()))
	{
		APlayerController* PC = Owner->GetWorld()->GetFirstPlayerController();
		FVector Forward = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Vector() : Owner->GetActorForwardVector();

		EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);

		// ====================================================================
		// 1. TYLKO DLA WOLNYCH PROPÓW (Grab_Free): Rzut z masą
		// ====================================================================
		if (Type == EInteractionType::Grab_Free)
		{
			if (PhysicsHandle)
			{
				PhysicsHandle->ReleaseComponent();
			}

			if (GrabbedComponent)
			{
				// 1. Kasujemy pęd od myszki
				GrabbedComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
				GrabbedComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

				// 2. Skalowanie prędkości przez masę (Krzywa pierwiastkowa):
				float ObjectMass = FMath::Max(1.0f, GrabbedComponent->GetMass());
				float MassSpeedMultiplier = FMath::Clamp(1.5f / FMath::Sqrt(ObjectMass), 0.1f, 1.4f);

				// 3. Wektor rzutu w celownik + BARDZO SUBTELNE PODBICIE W GÓRĘ (+60 cm/s):
				FVector FinalThrowVelocity = Forward * (BaseThrowPower * MassSpeedMultiplier);
				FinalThrowVelocity.Z += 60.0f; // Delikatny, naturalny łuk bez utraty celności!

				if (ACharacter* OwnerChar = Cast<ACharacter>(Owner))
				{
					// Dodajemy wektor pędu gracza do puszki (Biegniesz 900 -> rzut leci o 900 szybciej!):
					FinalThrowVelocity += OwnerChar->GetVelocity();
				}

				// ====================================================================
				// NAPIS DIAGNOSTYCZNY (Masa i Prędkość na ekranie):
				// ====================================================================
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
						FString::Printf(TEXT("MASA: %.1f kg | PRĘDKOŚĆ: %.0f cm/s | MNOŻNIK: x%.2f"), ObjectMass, FinalThrowVelocity.Size(), MassSpeedMultiplier));
				}

				GrabbedComponent->AddImpulse(FinalThrowVelocity, NAME_None, true); // Vel Change = TRUE

				GrabbedComponent->SetCollisionResponseToChannel(ECC_Pawn, OriginalPawnResponse);
				GrabbedComponent = nullptr;
			}

			IPhysicalInteract::Execute_SlamObject(GrabbedActor, Forward, BaseThrowPower);
		}
		// ====================================================================
		// 2. DLA DRZWI I MEBLI (Hinge, Translation, Crank): TYLKO WYWAŻENIE W BLUEPRINCIECIE
		// ====================================================================
		else
		{
			GrabbedComponent = nullptr;
			IPhysicalInteract::Execute_SlamObject(GrabbedActor, Forward, BaseThrowPower);
		}

		// ====================================================================
		// 3. CZYSTY RESET (Przywrócenie prędkości i HoldSlota bez blokowania sprintu):
		// ====================================================================
		CleanupInteraction();
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
		float ObjectMass = GrabbedComponent ? GrabbedComponent->GetMass() : 1.0f;

		// BARDZO CIĘŻKI OPOR: 
		// Mała puszka obraca się znośnie (0.4), ale 30kg skrzynia prawie w ogóle nie drgnie (0.02)!
		float InspectSensitivity = FMath::Clamp(1.0f / FMath::Max(1.0f, ObjectMass * 0.2f), 0.05f, 0.8f);

		APlayerController* PC = GetOwner()->GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();

			// Obliczamy rotację na podstawie ruchu myszki względem kamery gracza:
			FVector DeltaRotVector = (CamRot.Quaternion() * FVector(-MouseY * InspectSensitivity, MouseX * InspectSensitivity, 0.0f));
			FRotator DeltaRot(DeltaRotVector.X, DeltaRotVector.Y, DeltaRotVector.Z);

			// Płynne dojeżdżanie do obrotu (lag/wygładzenie, żeby nie było mikro-szarpnięć):
			FRotator CurrentRot = HoldSlotComponent->GetRelativeRotation();
			FRotator TargetRot = CurrentRot + DeltaRot;
			FRotator SmoothedRot = FMath::RInterpTo(CurrentRot, TargetRot, GetWorld()->GetDeltaSeconds(), 25.0f);

			HoldSlotComponent->SetRelativeRotation(SmoothedRot);
		}

		return true; // Blokujemy kamerę
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

void UInteractionComponent::CleanupInteraction()
{
	bIsInspecting = false;
	// 1. Przywrócenie prędkości gracza / Sprintu
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

	// 2. Reset HoldSlota
	if (HoldSlotComponent)
	{
		HoldSlotComponent->SetRelativeLocation(InitialHoldSlotLocation);
		HoldSlotComponent->SetRelativeRotation(InitialHoldSlotRotation);
	}

	GrabbedActor = nullptr;
}

void UInteractionComponent::ZoomHoldSlot(float ScrollDelta)
{
	if (HoldSlotComponent && IsValid(GrabbedActor))
	{
		FVector Loc = HoldSlotComponent->GetRelativeLocation();

		float MaxSafeDropLimit = (InteractionDistance + BreakDistanceBuffer) - 20.0f;
		float MinZoom = CurrentBaseHoldDistance - 40.0f;
		float MaxZoom = FMath::Min(CurrentBaseHoldDistance + 50.0f, MaxSafeDropLimit);

		// Zamiast natychmiastowego skoku, wyliczamy docelową pozycję X:
		float TargetX = FMath::Clamp(Loc.X + (ScrollDelta * 15.0f), MinZoom, MaxZoom);
		
		// Płynnie domykamy pozycję (Interpolacja), co eliminuje jakikolwiek "wybuch" prędkości!
		Loc.X = FMath::FInterpTo(Loc.X, TargetX, GetWorld()->GetDeltaSeconds(), 20.0f);

		HoldSlotComponent->SetRelativeLocation(Loc);
	}
}

float UInteractionComponent::CalculateMovementSpeed(float BaseSpeed, float MassInKg) const
{
	// W PRZYSZŁOŚCI (Miejsce na Perk Wigoru):
	// float EffectiveMass = MassInKg / (1.0f + VigorTier * 0.5f);
	float EffectiveMass = MassInKg;

	// Obliczamy mnożnik kary za wagę (np. dla 25 kg = 1.0 - (25 * 0.015) = 0.625)
	float SpeedMultiplier = 1.0f - (EffectiveMass * WeightSpeedReductionFactor);

	// Zabezpieczenie: prędkość nie może spaść poniżej MinCarryingSpeedRatio (np. 30%)
	SpeedMultiplier = FMath::Clamp(SpeedMultiplier, MinCarryingSpeedRatio, 1.0f);

	return BaseSpeed * SpeedMultiplier;
}

void UInteractionComponent::ToggleInspectMode()
{
	// Działamy TYLKO wtedy, gdy trzymamy wolny prop (Grab_Free)
	if (!GrabbedActor) return;

	EInteractionType Type = IPhysicalInteract::Execute_GetInteractionType(GrabbedActor);
	if (Type != EInteractionType::Grab_Free) return;

	// Przełączamy stan
	bIsInspecting = !bIsInspecting;

	// Jeśli wyłączamy inspekcję, resetujemy rotację, żeby przedmiot nie odskoczył
	if (!bIsInspecting && HoldSlotComponent)
	{
		HoldSlotComponent->SetRelativeRotation(InitialHoldSlotRotation);
	}
}