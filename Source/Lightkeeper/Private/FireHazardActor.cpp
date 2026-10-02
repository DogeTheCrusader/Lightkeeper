#include "FireHazardActor.h"
#include "Components/PointLightComponent.h"
#include "SafeLightComponent.h"
#include "Components/PrimitiveComponent.h"

AFireHazardActor::AFireHazardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	// 1. ŚWIATŁO GŁÓWNE (Rzuca cienie, ale miękkie dzięki Source Radius):
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(RootComponent);
	FireLight->bUseInverseSquaredFalloff = true; // Zgodnie ze screenem!
	FireLight->SetIntensity(2500.0f);            // Twoja sprawdzona baza
	FireLight->SetAttenuationRadius(800.0f);
	FireLight->SetLightColor(FLinearColor(1.0f, 0.361f, 0.04f)); // Twój kolor Hex FFA238FF
	FireLight->SetMobility(EComponentMobility::Movable);
	FireLight->SetSourceRadius(35.0f); // Kula ognia zamiast punktu -> zmiękcza krawędzie cieni!

	// 2. ŚWIATŁO WYPEŁNIAJĄCE BOKI (Fill Light - BEZ CIENI):
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(RootComponent);
	FillLight->SetCastShadows(false); // Kluczowe: przenika przez deski i rozświetla boki!
	FillLight->bUseInverseSquaredFalloff = true;
	FillLight->SetLightColor(FLinearColor(1.0f, 0.35f, 0.05f));
	FillLight->SetMobility(EComponentMobility::Movable);

	SafeLight = CreateDefaultSubobject<USafeLightComponent>(TEXT("SafeLight"));
	SafeLight->SetupAttachment(RootComponent);
	SafeLight->bAutoSyncWithLight = false;
	SafeLight->bIsSpotlightCone = false;
	SafeLight->bIsLightActive = true;
}

void AFireHazardActor::BeginPlay()
{
	Super::BeginPlay();
}

void AFireHazardActor::InitializeHazard(AActor* InTargetActor, float Radius, float Intensity, float Duration, bool bIsVerticalObject)
{
	TargetActor = InTargetActor;
	bIsVerticalSurface = bIsVerticalObject;

	if (FireLight)
	{
		FireLight->SetAttenuationRadius(Radius);
		FireLight->SetIntensity(Intensity);
		FireLight->SetVisibility(true);
	}

	if (SafeLight)
	{
		SafeLight->SetSphereRadius(Radius, true);
		SafeLight->UpdateOverlaps();
	}

	SetLifeSpan(Duration);

	if (InTargetActor)
	{
		TargetPhysMesh = Cast<UPrimitiveComponent>(InTargetActor->GetComponentByClass(UPrimitiveComponent::StaticClass()));

		// Logika pływania w Tick DZIAŁA TYLKO dla skrzynek i brył swobodnych.
		// Dla pionowych drzwi i okien (bIsVerticalSurface == true) wyłączamy Tick pływający:
		bFollowPhysicsTop = !bIsVerticalSurface && TargetPhysMesh.IsValid() && TargetPhysMesh->IsSimulatingPhysics();

		// Przypinamy ogień do siatki mebla (dla drzwi ogień obraca się razem ze skrzydłem):
		USceneComponent* AttachTarget = TargetPhysMesh.IsValid() ? TargetPhysMesh.Get() : InTargetActor->GetRootComponent();
		FAttachmentTransformRules AttachRules(
			EAttachmentRule::KeepWorld,
			EAttachmentRule::KeepWorld,
			EAttachmentRule::KeepWorld,
			false
		);
		AttachToComponent(AttachTarget, AttachRules);
	}
}

void AFireHazardActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Działa wyłącznie dla turlających się skrzynek:
	if (bFollowPhysicsTop && TargetPhysMesh.IsValid())
	{
		FBoxSphereBounds Bounds = TargetPhysMesh->Bounds;

		// Cel: Środek w osiach X i Y, a w Z bierzemy aktualny szczyt bryły + 40 cm w górę świata:
		FVector DesiredLocation = FVector(
			Bounds.Origin.X,
			Bounds.Origin.Y,
			Bounds.Origin.Z + Bounds.BoxExtent.Z + 40.0f
		);

		// Płynna interpolacja (ślizganie się płomienia po obracającej się skrzyni):
		FVector SmoothLocation = FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaTime, 15.0f);
		SetActorLocation(SmoothLocation);
	}
}