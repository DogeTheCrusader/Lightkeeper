#include "SafeLightComponent.h"
#include "SanityComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BaseInteractable.h"

USafeLightComponent::USafeLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SphereRadius = 500.0f;
	SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetGenerateOverlapEvents(true);
}

void USafeLightComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1. Automatyczna synchronizacja ze Ÿród³em œwiat³a:
	if (bAutoSyncWithLight)
	{
		SyncWithParentLight();
	}

	OnComponentBeginOverlap.AddDynamic(this, &USafeLightComponent::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &USafeLightComponent::OnOverlapEnd);

	bInitialCheckDone = false; // Gotowy do sprawdzenia w 1. klatce po zakoñczeniu wszystkich BeginPlay
}

void USafeLightComponent::SyncWithParentLight()
{
	if (AActor* Owner = GetOwner())
	{
		// A. Jeœli to Reflektor ze sto¿kiem (SpotLight):
		if (USpotLightComponent* SpotLight = Owner->FindComponentByClass<USpotLightComponent>())
		{
			SetSphereRadius(SpotLight->AttenuationRadius, true);
			bIsSpotlightCone = true;
			ConeAngle = SpotLight->OuterConeAngle;
			return;
		}

		// B. Jeœli to Zwyk³a ¯arówka dookólna (PointLight):
		if (UPointLightComponent* PointLight = Owner->FindComponentByClass<UPointLightComponent>())
		{
			SetSphereRadius(PointLight->AttenuationRadius, true);
			bIsSpotlightCone = false;
			return;
		}
	}
}

void USafeLightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	// Jeœli to aktywne Ÿród³o œwiat³a:
	if (bIsLightActive)
	{
		// 1. Zawsze upewniamy siê, ¿e mamy wskaŸnik na Sanity gracza:
		if (!CachedPlayerSanity.IsValid())
		{
			if (USanityComponent* Sanity = PlayerPawn->FindComponentByClass<USanityComponent>())
			{
				CachedPlayerSanity = Sanity;
			}
		}

		bool bIsCurrentlyHeld = false;
		if (ABaseInteractable* BaseProp = Cast<ABaseInteractable>(GetOwner()))
		{
			bIsCurrentlyHeld = BaseProp->bIsHeld;
		}

		// ====================================================================
		// 2. DWUKIERUNKOWA KONTROLA ODLEGLOŒCI (WEJŒCIE I WYJŒCIE):
		// ====================================================================
		if (bIsCurrentlyHeld)
		{
			// Jeœli trzymamy w rêkach -> zawsze jesteœmy w œwietle:
			bIsPlayerInside = true;
			bIsPlayerInCone = true;
		}
		else
		{
			// Jeœli obiekt le¿y na ziemi (lub to latarnia miejska):
			float Distance = FVector::Dist(GetComponentLocation(), PlayerPawn->GetActorLocation());

			// PANCERNA MATEMATYKA: Wchodzisz w promieñ = PRAWDA, Wychodzisz = FA£SZ:
			bIsPlayerInside = (Distance <= SphereRadius);
			bIsPlayerInCone = bIsSpotlightCone ? IsPlayerInsideLightCone(PlayerPawn) : true;
		}

		UpdatePlayerLightState(); // Automatycznie dodaje lub odejmuje œwiat³o!
	}
	else
	{
		// Jeœli œwiat³o zosta³o zgaszone (np. zalane wod¹):
		bIsPlayerInside = false;
		bIsPlayerInCone = false;
		UpdatePlayerLightState();
	}
}

bool USafeLightComponent::IsPlayerInsideLightCone(AActor* PlayerActor) const
{
	if (!PlayerActor) return false;

	FVector LightForward = GetForwardVector();
	FVector DirToPlayer = (PlayerActor->GetActorLocation() - GetComponentLocation()).GetSafeNormal();

	float Dot = FVector::DotProduct(LightForward, DirToPlayer);
	float ConeLimit = FMath::Cos(FMath::DegreesToRadians(ConeAngle));

	return Dot >= ConeLimit;
}

void USafeLightComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->IsA<APawn>()) return;

	if (USanityComponent* Sanity = OtherActor->FindComponentByClass<USanityComponent>())
	{
		bIsPlayerInside = true;
		CachedPlayerSanity = Sanity;
		bIsPlayerInCone = bIsSpotlightCone ? IsPlayerInsideLightCone(OtherActor) : true;

		UpdatePlayerLightState();
	}
}

void USafeLightComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || !OtherActor->IsA<APawn>()) return;

	if (ABaseInteractable* BaseProp = Cast<ABaseInteractable>(GetOwner()))
	{
		if (BaseProp->bIsHeld)
		{
			return; // Niesiony obiekt nie gasi œwiat³a
		}
	}

	bIsPlayerInside = false;
	bIsPlayerInCone = false;
	UpdatePlayerLightState();
	CachedPlayerSanity.Reset();
}

void USafeLightComponent::UpdatePlayerLightState()
{
	if (!CachedPlayerSanity.IsValid())
	{
		bHasContributedLight = false;
		return;
	}

	bool bShouldGiveLight = bIsPlayerInside && bIsLightActive && (!bIsSpotlightCone || bIsPlayerInCone);

	// PANCERNA LOGIKA: Dodajemy œwiat³o TYLKO RAZ:
	if (bShouldGiveLight && !bHasContributedLight)
	{
		CachedPlayerSanity->AddLightSource();
		bHasContributedLight = true;
	}
	// Odejmujemy œwiat³o TYLKO jeœli wczeœniej je dodaliœmy:
	else if (!bShouldGiveLight && bHasContributedLight)
	{
		CachedPlayerSanity->RemoveLightSource();
		bHasContributedLight = false;
	}
}

void USafeLightComponent::SetLightActive(bool bNewActive)
{
	if (bIsLightActive == bNewActive) return;

	bIsLightActive = bNewActive;
	UpdatePlayerLightState();
}

void USafeLightComponent::SetDynamicRadius(float NewRadius)
{
	SetSphereRadius(NewRadius, true);
}