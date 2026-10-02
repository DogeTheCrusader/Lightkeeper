#include "SafeLightComponent.h"
#include "SanityComponent.h"
#include "InventoryComponent.h"
#include "ReactionReceiverComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
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

void USafeLightComponent::OnRegister()
{
	Super::OnRegister();

	// Aktualizuje żółtą sferę od razu w edytorze Blueprinta:
	if (bAutoSyncWithLight)
	{
		SyncWithParentLight();
	}
}

void USafeLightComponent::BeginPlay()
{
	Super::BeginPlay();

	SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Overlap);

	OnComponentBeginOverlap.AddDynamic(this, &USafeLightComponent::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &USafeLightComponent::OnOverlapEnd);

	// ====================================================================
	// JEDNO ŹRÓDŁO PRAWDY: Przekazujemy stan bStartsLit do systemu:
	// ====================================================================
	bIsLightActive = !bStartsLit; 
	SetLightActive(bStartsLit);

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

#if WITH_EDITOR
void USafeLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USafeLightComponent, bStartsLit))
	{
		SetLightActive(bStartsLit); // Kliknięcie w edytorze od razu zapala/gasi lampę!
	}
}
#endif

void USafeLightComponent::SetLightActive(bool bNewActive)
{
	if (bIsLightActive == bNewActive) return;

	bIsLightActive = bNewActive;
	UpdatePlayerLightState();

	if (AActor* Owner = GetOwner())
	{
		// 1. Gaszenie/Zapalanie fizycznych żarówek wizualnych (Działa w Edytorze i w Grze!):
		TArray<ULightComponent*> VisualLights;
		Owner->GetComponents<ULightComponent>(VisualLights);
		for (ULightComponent* Light : VisualLights)
		{
			if (Light)
			{
				// Zabezpieczenie: Ignoruj światła, które Level Designer oznaczył tagiem "AlwaysOn"
				if (Light->ComponentHasTag(FName("AlwaysOn")))
				{
					continue; // Zostaw to światło w spokoju!
				}

				Light->SetVisibility(bIsLightActive);
			}
		}

		// 2. Automatyczna synchronizacja z Chemią (URUCHAMIANA TYLKO PODCZAS GRY!)
		// HasAnyFlags(RF_ClassDefaultObject) zapobiega crashom przy edycji w Blueprintach!
		if (GetWorld() && GetWorld()->IsGameWorld() && !HasAnyFlags(RF_ClassDefaultObject))
		{
			if (UReactionReceiverComponent* ReactionComp = Owner->FindComponentByClass<UReactionReceiverComponent>())
			{
				static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);

				// Dodajemy lub usuwamy tag TYLKO jeśli stan faktycznie wymaga zmiany, 
				// zapobiegając w ten sposób nieskończonej pętli z HandleOwnerStateApplied!
				if (bIsLightActive && !ReactionComp->ActiveStates.HasTagExact(BurningStatus))
				{
					ReactionComp->ActiveStates.AddTag(BurningStatus);
				}
				else if (!bIsLightActive && ReactionComp->ActiveStates.HasTagExact(BurningStatus))
				{
					ReactionComp->ActiveStates.RemoveTag(BurningStatus);
				}
			}
		}
	}

	// 3. Włączanie/wyłączanie strefy Sanity (Działa w Edytorze i w Grze):
	SetCollisionEnabled(bIsLightActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (bIsLightActive && bAutoSyncWithLight)
	{
		SyncWithParentLight();
	}

	// ====================================================================
	// DIAGNOSTYKA EKRANOWA (Wyświetlana tylko w trakcie Play!)
	// ====================================================================
#if !UE_BUILD_SHIPPING
	if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
	{
		FString StateStr = bIsLightActive ? TEXT("💡 [ŚWIATŁO WŁĄCZONE]") : TEXT("🌑 [ŚWIATŁO ZGASZONE]");
		FColor LogColor = bIsLightActive ? FColor::Yellow : FColor::Silver;

		GEngine->AddOnScreenDebugMessage(
			(uint64)GetUniqueID() + 400,
			2.5f,
			LogColor,
			FString::Printf(TEXT("%s na obiekcie: %s"), *StateStr, *GetOwner()->GetName())
		);
	}
#endif
}

void USafeLightComponent::HandleOwnerStateApplied(FGameplayTag StateTag, float Intensity)
{
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);

	if (StateTag.MatchesTag(BurningStatus))
	{
		SetLightActive(true);
	}
}

void USafeLightComponent::HandleOwnerStateRemoved(FGameplayTag StateTag)
{
	static const FGameplayTag BurningStatus = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);

	if (StateTag.MatchesTag(BurningStatus))
	{
		if (bExtinguishWhenBurningEnds)
		{
			SetLightActive(false);
		}
	}
}

void USafeLightComponent::SyncWithParentLight()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	TArray<ULightComponent*> AllLights;
	Owner->GetComponents<ULightComponent>(AllLights);

	if (AllLights.IsEmpty()) return;

	ULightComponent* MasterLight = nullptr;
	float MaxRadius = 0.0f;

	// ZNAJDUJEMY GŁÓWNE ŚWIATŁO (największy zasięg w lampie):
	for (ULightComponent* Light : AllLights)
	{
		if (!Light) continue;

		float CurrentRadius = 0.0f;
		if (UPointLightComponent* Point = Cast<UPointLightComponent>(Light))
		{
			CurrentRadius = Point->AttenuationRadius;
		}
		else if (USpotLightComponent* Spot = Cast<USpotLightComponent>(Light))
		{
			CurrentRadius = Spot->AttenuationRadius;
		}

		if (CurrentRadius > MaxRadius)
		{
			MaxRadius = CurrentRadius;
			MasterLight = Light;
		}
	}

	if (!MasterLight || MaxRadius <= 0.0f) return;

	// 1. ZAPAMIĘTUJEMY FAKTYCZNY PROMIEŃ ŚWIATŁA DLA SANITY:
	ActualLightRadius = MaxRadius;

	// 2. SFERĘ KOLIZJI USTAWIAJĄCĄ DETEKCJĘ ROZSZERZAMY NA STREFĘ WZROKU (np. 2.5x lub min. 16 metrów):
	// Dzięki temu gracz z drugiego końca ulicy ma tę lampę na radarze i może na nią patrzeć!
	float RegistrationAuraRadius = FMath::Max(MaxRadius * 2.5f, 1600.0f);
	SetSphereRadius(RegistrationAuraRadius, true);

	if (USpotLightComponent* MasterSpot = Cast<USpotLightComponent>(MasterLight))
	{
		bool bHasAnyPointLight = Owner->FindComponentByClass<UPointLightComponent>() != nullptr;
		if (!bHasAnyPointLight)
		{
			bIsSpotlightCone = true;
			ConeAngle = MasterSpot->OuterConeAngle;
			SetRelativeRotation(MasterSpot->GetRelativeRotation());
		}
		else
		{
			bIsSpotlightCone = false;
		}
	}
	else
	{
		bIsSpotlightCone = false;
	}

	UpdateBounds();
	UpdateOverlaps();
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

	bool bShouldGiveLight = bIsPlayerInside && bIsLightActive && (!bIsSpotlightCone || bIsPlayerInCone);

	if (bShouldGiveLight && !bHasContributedLight)
	{
		CachedPlayerSanity->RegisterPotentialLight(this);
		bHasContributedLight = true;
	}
	else if (!bShouldGiveLight && bHasContributedLight)
	{
		CachedPlayerSanity->UnregisterPotentialLight(this);
		bHasContributedLight = false;
	}
}

void USafeLightComponent::SetDynamicRadius(float NewRadius)
{
	SetSphereRadius(NewRadius, true);
}

void USafeLightComponent::InteractWithLight(AActor* Instigator)
{
	if (!bIsToggleableLightSource) return;

	if (bIsLightActive)
	{
		// GASZENIE [E]
		if (bCanBeBlownOut)
		{
			SetLightActive(false);
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver, TEXT("💨 [ŚWIATŁO] Zgaszono płomień! Ukryto się w mroku."));
#endif
		}
		else
		{
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("❌ [ŚWIATŁO] Tego źródła nie da się zgasić ręcznie!"));
#endif
		}
	}
	else
	{
		// ZAPALANIE [E]
		UInventoryComponent* InvComp = Instigator ? Instigator->FindComponentByClass<UInventoryComponent>() : nullptr;
		bool bHasMatch = false;

		if (!bRequiresMatchesToIgnite)
		{
			bHasMatch = true; // Zapala się od razu (np. lampka na biurku)
		}
		else if (InvComp && RequiredMatchItemTag.IsValid())
		{
			// TUTAJ BYŁ BŁĄD - Używamy poprawnej metody z Twojego systemu:
			bHasMatch = InvComp->HasItemWithTag(RequiredMatchItemTag);
		}

		if (bHasMatch)
		{
			SetLightActive(true);

			if (bRequiresMatchesToIgnite && InvComp)
			{
				// Zabrano zapałkę:
				InvComp->ConsumeItemByTag(RequiredMatchItemTag, 1);
			}

#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("🔥 [ŚWIATŁO] Zapalono światło!"));
#endif
		}
		else
		{
#if !UE_BUILD_SHIPPING
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("❌ [BRAK PRZEDMIOTU] Potrzebujesz zapałek: %s"), *RequiredMatchItemTag.ToString()));
#endif
		}
	}
}