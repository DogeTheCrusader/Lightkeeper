#include "BaseDoorInteractable.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

ABaseDoorInteractable::ABaseDoorInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Domyślny tryb dla drzwi
	InteractionType = EInteractionType::Hinge;
	PreferredMouseAxis = EMouseAxis::Auto_CameraRelative;
	BaseInteractionPower = 12.0f;
	MechanicalFriction = 1.0f;

	// 1. Budujemy żelazną hierarchię (Root -> Frame & Door)
	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);

	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	DoorFrame->SetupAttachment(DoorRoot);
	DoorFrame->SetCollisionProfileName(TEXT("BlockAll"));

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorRoot); // Skrzydło JEST ZAWSZE względem korzenia!
	DoorMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	DoorMesh->SetGenerateOverlapEvents(true);

	HingeConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("HingeConstraint"));
	HingeConstraint->SetupAttachment(DoorRoot);

	// Tłumienie drzwi (żeby nie latały jak z papieru)
	DoorMesh->SetLinearDamping(0.5f);
	DoorMesh->SetAngularDamping(4.5f);
}

void ABaseDoorInteractable::BeginPlay()
{
	Super::BeginPlay();

	// Masa i fizyka
	DoorMesh->SetMassOverrideInKg(NAME_None, DoorMass, true);
	DoorMesh->IgnoreComponentWhenMoving(DoorFrame, true); // Skrzydło NIGDY nie blokuje się o własną futrynę!

	SetupHinge();

	// Ustalenie stanu początkowego
	if (bIsLatched || bIsLocked)
	{
		SnapToClosedPosition();
	}
	else
	{
		DoorMesh->SetSimulatePhysics(true);
	}
}

void ABaseDoorInteractable::SetupHinge()
{
	if (!HingeConstraint) return;

	// Łączymy framugę ze skrzydłem
	HingeConstraint->SetConstrainedComponents(DoorFrame, NAME_None, DoorMesh, NAME_None);
	HingeConstraint->SetDisableCollision(true); // Zapobiega zacinaniu się w zawiasie

	// Konfiguracja osi (Z = obrót poziomy)
	HingeConstraint->SetAngularSwing1Limit(ACM_Locked, 0.0f);
	HingeConstraint->SetAngularSwing2Limit(ACM_Locked, 0.0f);
	HingeConstraint->SetAngularTwistLimit(ACM_Limited, MaxOpenAngle * 0.5f); // Połowa kąta

	if (bBidirectional)
	{
		HingeConstraint->ConstraintInstance.AngularRotationOffset = FRotator(0.0f, 0.0f, 0.0f);
	}
	else
	{
		// Twarda blokada przed wejściem w ścianę (od 0 do MaxOpenAngle)
		HingeConstraint->ConstraintInstance.AngularRotationOffset = FRotator(0.0f, 0.0f, -(MaxOpenAngle * 0.5f));
	}
	HingeConstraint->InitComponentConstraint();
}

void ABaseDoorInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ====================================================================
	// A. ANIMACJA "NUDGE" (Lekki odskok przy kliknięciu [LPM])
	// ====================================================================
	if (bIsAnimatingNudge)
	{
		NudgeTimer += DeltaTime;

		float Alpha = FMath::Clamp(NudgeTimer / 0.15f, 0.0f, 1.0f);
		float NudgeAngle = FMath::Lerp(0.0f, 8.0f, Alpha); // Uchylamy o 8 stopni

		// Zawsze operujemy na czystym zerze
		DoorMesh->SetRelativeRotation(FRotator(0.0f, NudgeAngle, 0.0f));

		if (Alpha >= 1.0f)
		{
			bIsAnimatingNudge = false;
			DoorMesh->SetSimulatePhysics(true);

			// Mikro-pchnięcie fizyczne po animacji, by wybudzić silnik Chaos:
			DoorMesh->SetPhysicsAngularVelocityInDegrees(FVector(0.0f, 0.0f, 15.0f));
		}
		return;
	}

	// ====================================================================
	// B. AUTO-LATCH (Domykanie drzwi w futrynie)
	// ====================================================================
	if (!bIsHeld && DoorMesh->IsSimulatingPhysics() && !bIsLocked)
	{
		// Czysty kąt zawsze odnosi się do DoorRoot (0,0,0)
		float CurrentYaw = DoorMesh->GetRelativeRotation().Yaw;

		if (FMath::Abs(CurrentYaw) <= 3.0f)
		{
			bIsLatched = true;
			SnapToClosedPosition();
		}
	}
}

void ABaseDoorInteractable::SnapToClosedPosition()
{
	if (!DoorMesh) return;

	// Perfekcyjne, twarde zamknięcie bez wystrzałów
	DoorMesh->SetSimulatePhysics(false);
	DoorMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	DoorMesh->SetRelativeRotation(FRotator::ZeroRotator); // ZAWSZE 0, bo rodzicem jest DoorRoot!
}

void ABaseDoorInteractable::GrabObject_Implementation(AActor* Grabber)
{
	Super::GrabObject_Implementation(Grabber);

	// Jeśli drzwi są zamknięte na zatrzask, ale zamek z kluczem puszcza:
	if (bIsLatched && !bIsLocked)
	{
		bIsLatched = false;
		bIsAnimatingNudge = true;
		NudgeTimer = 0.0f;
	}
}

void ABaseDoorInteractable::MoveObject_Implementation(float AxisDelta)
{
	Super::MoveObject_Implementation(AxisDelta);

	if (!DoorMesh || !DoorMesh->IsSimulatingPhysics()) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	// ====================================================================
	// 1. Z JAKIEJ STRONY STOIMY? (Odwracanie wektora pchania myszką)
	// ====================================================================
	FVector DirToPlayer = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	float Dot = FVector::DotProduct(GetActorForwardVector(), DirToPlayer);

	// Jeśli stoimy z tyłu, ruch myszką w górę musi otwierać w drugą stronę!
	float FacingSign = (Dot >= 0.0f) ? 1.0f : -1.0f;

	// ====================================================================
	// 2. POTĘŻNY, ZBALANSOWANY MOMENT OBROTOWY (Torque zamiast Impulsu!)
	// ====================================================================
	// Mnożnik BaseInteractionPower ustawiony w konstruktorze na 15.0
	float RawForce = AxisDelta * FacingSign * BaseInteractionPower;

	// Ogranicznik, żeby gracz nie wyrwał drzwi z zawiasów gwałtownym ruchem myszy:
	float ClampedForce = FMath::Clamp(RawForce, -25.0f, 25.0f);

	// Dodajemy ciągły pęd obrotowy (AddTorque) zamiast jednorazowego uderzenia (Impulse):
	// Mnożnik 500.0f gwarantuje potężny wpływ na masę 30kg!
	DoorMesh->AddTorqueInRadians(FVector(0.0f, 0.0f, ClampedForce * 500.0f), NAME_None, true);
}

void ABaseDoorInteractable::SetLocked_Implementation(bool bNewLocked)
{
	Super::SetLocked_Implementation(bNewLocked);

	if (bNewLocked)
	{
		bIsLatched = true;
		SnapToClosedPosition();
	}
}