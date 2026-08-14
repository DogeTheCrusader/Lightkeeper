#include "BaseInteractable.h"

// Konstruktor - ustawienia startowe
ABaseInteractable::ABaseInteractable()
{
	// Pozwalamy temu aktorowi używać Event Tick
	PrimaryActorTick.bCanEverTick = true;
}

void ABaseInteractable::BeginPlay()
{
	Super::BeginPlay();
}

void ABaseInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Tutaj w Blueprintach dodamy nasz system Auto-Zatrzaskiwania drzwi (Auto-Latch)
}

// ==========================================================
// WYKONANIE FUNKCJI INTERFEJSU (Co się dzieje po kliknięciu?)
// ==========================================================
EInteractionType ABaseInteractable::GetInteractionType_Implementation()
{
	// Zamiast wpisywać "Hinge" na sztywno, zwracamy opcję wybraną w edytorze!
	return InteractionType;
}

EMouseAxis ABaseInteractable::GetPreferredMouseAxis_Implementation()
{
	return PreferredMouseAxis;
}

void ABaseInteractable::GrabObject_Implementation(AActor* Grabber)
{
	// Gdy gracz klika na klamkę
	bIsHeld = true;
}

void ABaseInteractable::ReleaseObject_Implementation()
{
	// Gdy gracz puszcza przycisk myszy lub odchodzi za daleko
	bIsHeld = false;
}

void ABaseInteractable::SlamObject_Implementation(FVector PushDirection)
{
	// Gdy gracz wciska Prawy Przycisk Myszy (RMB)
	bIsHeld = false; // Trzaśnięcie natychmiast wyrywa klamkę z rąk gracza
}

void ABaseInteractable::BreakObject_Implementation()
{
	bIsBroken = true; // Obiekt oznacza się w pamięci jako zniszczony!
}

void ABaseInteractable::MoveObject_Implementation(float AxisDelta)
{
	// Domyślnie puste. Dzieci (np. BP_BaseDoor) nadpiszą to w Blueprintach swoimi klockami!
}

bool ABaseInteractable::IsLocked_Implementation()
{
	return bIsLocked; // <-- TUTAJ DAJEMY 'return bIsLocked;' !
}

void ABaseInteractable::SetLocked_Implementation(bool bNewLocked)
{
	bIsLocked = bNewLocked; // Po prostu ustawiamy naszą zmienną!
}

void ABaseInteractable::OnLockedInteraction_Implementation(AActor* InstigatorActor)
{
	// Domyślnie nic tu nie robimy w C++.
	// Zostawiamy to puste, aby Blueprinty (jak BP_BaseDoor) mogły same zadecydować, co zrobić.
}

bool ABaseInteractable::IsLatched_Implementation()
{
	return bIsLatched; // Po prostu zwracamy naszą zmienną!
}

bool ABaseInteractable::IsSmallProp_Implementation()
{
	return bIsSmallProp;
}