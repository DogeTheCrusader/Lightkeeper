#include "SafeLightComponent.h"
#include "SanityComponent.h"
#include "ReactionReceiverComponent.h"
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
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Overlap);
	SetGenerateOverlapEvents(true);
}

void USafeLightComponent::BeginPlay()
{
	Super::BeginPlay();

	SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Overlap);

	if (bAutoSyncWithLight)
	{
		SyncWithParentLight();
	}

	OnComponentBeginOverlap.AddDynamic(this, &USafeLightComponent::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &USafeLightComponent::OnOverlapEnd);

	// AUTONOMICZNA INTEGRACJA Z CHEMIA:
	if (bIgniteWhenOwnerIsBurning)
	{
		if (AActor* Owner = GetOwner())
		{
			if (UReactionReceiverComponent* ReactionComp = Owner->FindComponentByClass<UReactionReceiverComponent>())
			{
				ReactionComp->OnStateApplied.AddDynamic(this, &USafeLightComponent::HandleOwnerStateApplied);
				ReactionComp->OnStateRemoved.AddDynamic(this, &USafeLightComponent::HandleOwnerStateRemoved);
			}
		}
	}
}

void USafeLightComponent::HandleOwnerStateApplied(FGameplayTag StateTag, float Intensity)
{
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);

	// Gdy obiekt staje w płomieniach -> włączamy bezpieczne światło i ustawiamy zasięg!
	if (StateTag.MatchesTag(BurningStatus))
	{
		SetLightActive(true);
		SetDynamicRadius(350.0f * Intensity);
	}
}

void USafeLightComponent::HandleOwnerStateRemoved(FGameplayTag StateTag)
{
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);

	// Gdy ogień gaśnie -> wyłączamy bezpieczne światło:
	if (StateTag.MatchesTag(BurningStatus))
	{
		SetLightActive(false);
	}
}

void USafeLightComponent::SyncWithParentLight()
{
	if (AActor* Owner = GetOwner())
	{
		if (USpotLightComponent* SpotLight = Owner->FindComponentByClass<USpotLightComponent>())
		{
			SetSphereRadius(SpotLight->AttenuationRadius, true);
			bIsSpotlightCone = true;
			ConeAngle = SpotLight->OuterConeAngle;
			return;
		}

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

	if (bIsLightActive)
	{
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

		if (bIsCurrentlyHeld)
		{
			bIsPlayerInside = true;
			bIsPlayerInCone = true;
		}
		else
		{
			float Distance = FVector::Dist(GetComponentLocation(), PlayerPawn->GetActorLocation());
			bIsPlayerInside = (Distance <= SphereRadius);
			bIsPlayerInCone = bIsSpotlightCone ? IsPlayerInsideLightCone(PlayerPawn) : true;
		}

		UpdatePlayerLightState();
	}
	else
	{
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
		if (BaseProp->bIsHeld) return;
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

	// Twoja stara logika: sprawdza Cone i Overlap
	bool bShouldGiveLight = bIsPlayerInside && bIsLightActive && (!bIsSpotlightCone || bIsPlayerInCone);

	if (bShouldGiveLight && !bHasContributedLight)
	{
		// Zamiast AddLightSource() -> Rejestrujemy do sprawdzenia przez ścianę
		CachedPlayerSanity->RegisterPotentialLight(this);
		bHasContributedLight = true;
	}
	else if (!bShouldGiveLight && bHasContributedLight)
	{
		// Wyszliśmy ze światła lub stożka -> Wyrejestruj
		CachedPlayerSanity->UnregisterPotentialLight(this);
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