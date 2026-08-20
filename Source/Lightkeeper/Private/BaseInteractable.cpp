#include "BaseInteractable.h"
#include "LanternComponent.h"
#include "SanityComponent.h"
#include "HealthComponent.h"
#include "StaminaComponent.h"
#include "ReactionReceiverComponent.h"
#include "InventoryComponent.h"
#include "SafeLightComponent.h"
#include "Components/PrimitiveComponent.h"
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

	// Domyślny filtr zagrożeń blokujący podnoszenie [E]
	BlockingHazardStates.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard"), false));
}

void ABaseInteractable::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComp)
	{
		HealthComp->OnDeath.AddDynamic(this, &ABaseInteractable::HandleDeath);
	}

	if (ReactionComp)
	{
		ReactionComp->OnStateApplied.AddDynamic(this, &ABaseInteractable::HandleStateApplied);
	}

	// ====================================================================
	// 1. REJESTROWANIE ZDERZEŃ DLA BRYŁ
	// ====================================================================
	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Prim : PrimComps)
	{
		if (Prim)
		{
			Prim->SetNotifyRigidBodyCollision(true);
			Prim->OnComponentHit.AddDynamic(this, &ABaseInteractable::OnHit);
		}
	}

	// ====================================================================
	// 2. IMSIM: ODPAŁKA STREF CIĄGŁYCH (Pęknięte rury z parą, Ogniska)
	// ====================================================================
	if (bIsStateEmitter && TriggerType == EEmissionTrigger::ContinuousZone)
	{
		GetWorld()->GetTimerManager().SetTimer(ContinuousTimerHandle, this, &ABaseInteractable::TriggerStateEmission, EmissionInterval, true);
	}
}

void ABaseInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	/*
#if !UE_BUILD_SHIPPING
	// Wyświetlamy stan diagnostyczny dla Szuflad, Drzwi i trzymanych obiektów:
	if (bIsHeld || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Hinge)
	{
		if (GEngine)
		{
			FString LatchedStr = bIsLatched ? TEXT("TRUE (Zatrzasniete)") : TEXT("FALSE (Uchylone)");
			FString HeldStr = bIsHeld ? TEXT("TRUE (Trzymasz)") : TEXT("FALSE (Puszczone)");
			FString LockedStr = bIsLocked ? TEXT("TRUE (Zaryglowane)") : TEXT("FALSE");

			FColor StatusColor = bIsLatched ? FColor::Green : FColor::Orange;

			GEngine->AddOnScreenDebugMessage(
				(uint64)GetUniqueID(), // RZUTOWANIE NA uint64 ROZWIĄZUJE BŁĄD KOMPILACJI!
				0.0f,                  // 0.0s = odświeża się co klatkę bez spamu
				StatusColor,
				FString::Printf(TEXT("[%s] IsLatched: %s | IsHeld: %s | IsLocked: %s"),
					*GetName(), *LatchedStr, *HeldStr, *LockedStr)
			);
		}
	}
#endif*/
}

void ABaseInteractable::HandleStateApplied(FGameplayTag StateTag, float Intensity)
{
	// Szybkie pobranie tagu z pamięci (optymalizacja dla procesora):
	static const FGameplayTag ThermalTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal"), false);

	if (StateTag.MatchesTag(ThermalTag))
	{
		// 1. OBRAŻENIA: Zadajemy TYLKO jeśli ściana ma zaznaczone bCanBeDestroyed = true!
		// Ponieważ Twoja ściana ma bCanBeDestroyed = FALSE -> ta linijka zostanie zignorowana (ściana nigdy nie zniknie z mapy!)
		if (bCanBeDestroyed && HealthComp)
		{
			float FinalDamage = (25.0f * Intensity) * CustomDamageMultiplier;
			HealthComp->TakeDamage(FinalDamage, StateTag);
		}

		// 2. EFEKT WIZUALNY I ŚWIATŁO: Odpala się ZA KAŻDYM RAZEM, gdy podpalisz ścianę!
		if (USafeLightComponent* LightZone = FindComponentByClass<USafeLightComponent>())
		{
			LightZone->SetLightActive(true);
			LightZone->SetDynamicRadius(350.0f * Intensity);
		}
	}
}

void ABaseInteractable::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// ====================================================================
	// 1. ZABEZPIECZENIA PRZED EXPLOITAMI I IMMUNITETY
	// ====================================================================
	if (bIsHeld || bIsBroken) return;
	if (!OtherActor || OtherActor == this) return;
	if (OtherActor->IsA<APawn>()) return; // Gracz dotykający skrzynki jej nie niszczy

	// Obiekty przypięte do siebie (np. zasuwka na drzwiach) nie mogą się niszczyć:
	if (IsAttachedTo(OtherActor) || OtherActor->IsAttachedTo(this)) return;
	if (GetAttachParentActor() == OtherActor || OtherActor->GetAttachParentActor() == this) return;

	if (GetGameTimeSinceCreation() < 0.2f) return;

	if (ABaseInteractable* OtherInteractable = Cast<ABaseInteractable>(OtherActor))
	{
		if (OtherInteractable->bIsHeld || OtherInteractable->bIsBroken) return;
	}

	// ====================================================================
	// 2. IMSIM: PRZEKAZYWANIE STANU PRZEZ KONTAKT (Płonąca deska / Prąd)
	// ====================================================================
	if (ReactionComp)
	{
		static const FGameplayTag BurningTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Burning"), false);
		static const FGameplayTag FireElementTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal.Fire"), false);
		static const FGameplayTag MetalTag = FGameplayTag::RequestGameplayTag(FName("Material.Metal"), false);
		static const FGameplayTag ElectrocutedTag = FGameplayTag::RequestGameplayTag(FName("Status.State.Hazard.Electrocuted"), false);
		static const FGameplayTag CurrentElementTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Electricity.Current"), false);

		// A. Jeśli PŁONIEMY -> podpalamy cel:
		if (ReactionComp->HasState(BurningTag))
		{
			if (UReactionReceiverComponent* TargetReceiver = OtherActor->FindComponentByClass<UReactionReceiverComponent>())
			{
				TargetReceiver->ApplyStateImpact(FireElementTag, 1.0f);
			}
		}

		// B. Jeśli jesteśmy NAELEKTRYZOWANI i z METALU -> przewodzimy prąd:
		if (MaterialTag.MatchesTag(MetalTag) && ReactionComp->HasState(ElectrocutedTag))
		{
			if (UReactionReceiverComponent* TargetReceiver = OtherActor->FindComponentByClass<UReactionReceiverComponent>())
			{
				TargetReceiver->ApplyStateImpact(CurrentElementTag, 1.0f);
			}
		}
	}

	// ====================================================================
	// 3. FIZYKA OBRAŻEŃ (OPARTA NA RZECZYWISTEJ PRĘDKOŚCI LOTU)
	// ====================================================================
	FVector MyVel = HitComponent ? HitComponent->GetComponentVelocity() : FVector::ZeroVector;
	FVector OtherVel = OtherComp ? OtherComp->GetComponentVelocity() : FVector::ZeroVector;

	float ImpactSpeed = (MyVel - OtherVel).Size();

	// Jeśli zderzamy się ze statycznym otoczeniem (podłoga / ściana):
	if (OtherComp && OtherComp->IsWorldGeometry())
	{
		// Bierzemy TYLKO naszą prędkość uderzenia w płaszczyznę:
		ImpactSpeed = FMath::Abs(FVector::DotProduct(MyVel, Hit.ImpactNormal));
	}

	// ====================================================================
	// REALISTYCZNE PROGI PRĘDKOŚCI:
	// Szkło pęka przy locie 150 cm/s. 
	// Drewno/Metal wymaga prawdziwego uderzenia min. 500 cm/s (5 m/s - rzut lub upadek z wysokości!)
	// ====================================================================
	float MinSpeedToDamage = 500.0f;
	if (DamageSusceptibility > 2.0f) // Kruche rzeczy (Szkło)
	{
		MinSpeedToDamage = 150.0f;
	}

	// Jeśli prędkość jest poniżej progu (np. stawanie nogą = ~350 cm/s) -> ZERO OBRAŻEŃ!
	if (ImpactSpeed < MinSpeedToDamage) return;

	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastHitTime < 0.15f) return;
	LastHitTime = CurrentTime;

	// Emisja przy uderzeniu (np. dzwon):
	if (bIsStateEmitter && TriggerType == EEmissionTrigger::OnImpact)
	{
		TriggerStateEmission();
	}

	// Obliczamy energię kinetyczną:
	float ActualMass = (HitComponent && HitComponent->IsSimulatingPhysics()) ? HitComponent->GetMass() : ReferenceMass;
	float ExcessSpeed = ImpactSpeed - MinSpeedToDamage;
	float KineticEnergy = (ExcessSpeed * 0.1f) * (ActualMass / 10.0f);

	// A. Obrażenia własne (np. zrzucenie skrzynki z 2 piętra na beton):
	if (bCanBeDestroyed && HealthComp)
	{
		float FinalSelfDamage = KineticEnergy * DamageSusceptibility * CustomDamageMultiplier;
		if (FinalSelfDamage > 2.0f)
		{
			HealthComp->TakeDamage(FinalSelfDamage, FGameplayTag());
		}
	}

	// B. Obrażenia celu (np. rzucenie skrzynią w drzwi):
	if (ABaseInteractable* TargetInteractable = Cast<ABaseInteractable>(OtherActor))
	{
		if (TargetInteractable->bCanBeDestroyed && TargetInteractable->HealthComp)
		{
			float FinalTargetDamage = KineticEnergy * ImpactHardness * TargetInteractable->DamageSusceptibility * TargetInteractable->CustomDamageMultiplier;
			if (FinalTargetDamage > 2.0f)
			{
				TargetInteractable->HealthComp->TakeDamage(FinalTargetDamage, FGameplayTag());
			}
		}
	}
}

void ABaseInteractable::HandleDeath()
{
	if (bIsBroken) return;
	bIsBroken = true;

	// Jeśli to obiekt niszczący się z wybuchem (Mołotow / Butelka z kwasem):
	if (bIsStateEmitter && TriggerType == EEmissionTrigger::OnDestroy)
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

// ====================================================================
// GŁÓWNA EMISJA ŻYWIOŁÓW (ImSim Emitter)
// ====================================================================
void ABaseInteractable::TriggerStateEmission()
{
	if (!EmittedStateTag.IsValid()) return;

	FVector EmissionLocation = GetActorLocation();
	if (UPrimitiveComponent* PrimComp = FindComponentByClass<UPrimitiveComponent>())
	{
		EmissionLocation = PrimComp->GetComponentLocation();
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SplashRadius);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	if (GetWorld()->OverlapMultiByObjectType(Overlaps, EmissionLocation, FQuat::Identity, ObjectQueryParams, Sphere, Params))
	{
		for (const FOverlapResult& Hit : Overlaps)
		{
			if (AActor* HitActor = Hit.GetActor())
			{
				if (UReactionReceiverComponent* Receiver = HitActor->FindComponentByClass<UReactionReceiverComponent>())
				{
					Receiver->ApplyStateImpact(EmittedStateTag, SplashIntensity);
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	DrawDebugSphere(GetWorld(), EmissionLocation, SplashRadius, 16, FColor::Red, false, 2.0f);
#endif

	// Niszczymy obiekt TYLKO jeśli ma zaznaczone bDestroyOnEmission:
	if (bDestroyOnEmission && (TriggerType == EEmissionTrigger::OnDestroy || TriggerType == EEmissionTrigger::TimedFuse || TriggerType == EEmissionTrigger::Proximity))
	{
		bIsBroken = true;
		Destroy();
	}
}

void ABaseInteractable::DeactivateEmitter()
{
	bIsStateEmitter = false;
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

	float FinalMultiplier = MassMultiplier / FMath::Max(0.1f, MechanicalFriction);
	return FinalMultiplier;
}

// ====================================================================
// SYSTEM EKWIPUNKU [E]
// ====================================================================
bool ABaseInteractable::CanBePocketed_Implementation()
{
	if (!bCanBePocketed)
	{
		return false;
	}

	if (ReactionComp)
	{
		if (ReactionComp->ActiveStates.HasAny(BlockingHazardStates))
		{
			return false;
		}
	}

	return true;
}

// ====================================================================
// 1. UŻYCIE KLUCZA Z DŁONI (Otwieranie i Zamykanie z Ręki)
// ====================================================================
bool ABaseInteractable::TryUnlockWithKey(AActor* KeyActor, AActor* InstigatorActor)
{
	if (!RequiredKeyTag.IsValid()) return false;

	if (ABaseInteractable* KeyInteractable = Cast<ABaseInteractable>(KeyActor))
	{
		// Sprawdzamy czy tag trzymanego klucza pasuje do zamka:
		if (KeyInteractable->ItemData.ItemTag.MatchesTag(RequiredKeyTag))
		{
			// FIZYKA: Drzwi (Hinge) i Szuflady (Translation) muszą być domknięte, by je zaryglować!
			bool bRequiresClosedPosition = (InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation);

			if (bRequiresClosedPosition && !bIsLocked && !bIsLatched)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[ZAMEK] Musisz najpierw domknac drzwi/szuflade, aby przekrecic klucz!"));
				}
				return false;
			}

			// PRZEŁĄCZAMY STAN ZAMKA:
			bIsLocked = !bIsLocked;
			bKeyDiscovered = true; // ZAPAMIĘTUJEMY ZAMEK NA ZAWSZE!

			if (GEngine)
			{
				if (bIsLocked)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[ZAMEK] Zaryglowano na klucz!"));
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("[SUKCES] Odryglowano kluczem!"));
				}
			}

			// Automatycznie chowamy użyty klucz do plecaka, żeby nie zginął:
			if (InstigatorActor)
			{
				if (UInventoryComponent* InvComp = InstigatorActor->FindComponentByClass<UInventoryComponent>())
				{
					if (InvComp->TryAddItem(KeyInteractable->ItemData))
					{
						KeyActor->Destroy();
						return true;
					}
				}
			}

			return true;
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, TEXT("[BLAD] Ten klucz nie pasuje do tego zamka!"));
	}

	return false;
}

// ====================================================================
// 2. INTERAKCJA POD KLAWISZEM [E] (Podnoszenie lub Ryglowanie z Plecaka)
// ====================================================================
void ABaseInteractable::PickupObject_Implementation(AActor* InstigatorActor)
{
	if (!InstigatorActor) return;

	// --------------------------------------------------------------------
	// A. DLA DRZWI / SZUFLAD / ZAWORÓW (Obiekty nie do kieszeni)
	// --------------------------------------------------------------------
	if (!bCanBePocketed)
	{
		if (RequiredKeyTag.IsValid())
		{
			// Z plecaka możemy ryglować/odryglowywać TYLKO jeśli zamek został wcześniej odkryty:
			if (bKeyDiscovered && InstigatorActor)
			{
				if (UInventoryComponent* InvComp = InstigatorActor->FindComponentByClass<UInventoryComponent>())
				{
					if (InvComp->HasItemWithTag(RequiredKeyTag))
					{
						// FIZYKA: Blokujemy ryglowanie, jeśli drzwi/szuflada nie są domknięte:
						bool bRequiresClosedPosition = (InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation);

						if (bRequiresClosedPosition && !bIsLocked && !bIsLatched)
						{
							if (GEngine)
							{
								GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[ZAMEK] Musisz najpierw domknac drzwi/szuflade, aby przekrecic klucz!"));
							}
							return;
						}

						bIsLocked = !bIsLocked; // Przełącz stan zamka

						if (GEngine)
						{
							if (bIsLocked)
							{
								GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[ZAMEK] Zaryglowano zapamietanym kluczem z plecaka!"));
							}
							else
							{
								GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("[ZAMEK] Odryglowano zapamietanym kluczem z plecaka!"));
							}
						}
						return;
					}
				}
			}
			else
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[ZAMEK] Nie znasz jeszcze tego zamka. Musisz dopasowac klucz z dloni!"));
				}
				return;
			}
		}

		return;
	}

	// --------------------------------------------------------------------
	// B. DLA PRZEDMIOTÓW (Klucze, Butelki, Bandaże do plecaka)
	// --------------------------------------------------------------------
	if (!CanBePocketed_Implementation())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Nie mozesz tego podniesc! (Płonie lub jest niebezpieczne)"));
		}
		return;
	}

	UInventoryComponent* InvComp = InstigatorActor->FindComponentByClass<UInventoryComponent>();
	if (InvComp)
	{
		if (HealthComp)
		{
			ItemData.SavedHealth = HealthComp->CurrentHealth;
		}

		if (InvComp->TryAddItem(ItemData))
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("Schowano do plecaka: %s (HP: %.1f)"), *ItemData.ItemTag.ToString(), ItemData.SavedHealth));
			}

			Destroy();
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Brak miejsca w ekwipunku!"));
			}
		}
	}
}

bool ABaseInteractable::ConsumeObject_Implementation(AActor* InstigatorActor)
{
	if (!InstigatorActor || !bCanBePocketed || !bCanBeConsumed) return false;

	// OPTYMALIZACJA CPU:
	static const FGameplayTag OilTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Oil"), false);
	static const FGameplayTag BandageTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Bandage"), false);
	static const FGameplayTag WineTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Wine"), false);

	// 1. NAFTA -> Uzupełnia Latarnię
	if (ItemData.ItemTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Oil"), false)))
	{
		if (ULanternComponent* Lantern = InstigatorActor->FindComponentByClass<ULanternComponent>())
		{
			// ZABEZPIECZENIE: Jeśli brakuje mniej niż 1 jednostki paliwa -> BAK JEST PEŁNY!
			if (Lantern->CurrentFuel >= (Lantern->MaxFuel))
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[LATARNIA] Bak jest pełny! Nie marnujesz nafty."));
				}
				return false; // NIE NISZCZYMY BUTELKI!
			}

			Lantern->RefillFuel(ItemData.PrimaryValue);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
					FString::Printf(TEXT("[SZYBKIE UŻYCIE] Wlano +%.1f nafty!"), ItemData.PrimaryValue));
			}

			Destroy(); // Niszczymy butelkę TYLKO po faktycznym dolaniu
			return true;
		}
	}

	// 2. BANDAŻE -> Leczą Życie (HP)
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

	// 3. WINO MARIANI -> Przywraca Sanity
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

// ====================================================================
// IMPLEMENTACJA INTERFEJSU (IPhysicalInteract)
// ====================================================================
EInteractionType ABaseInteractable::GetInteractionType_Implementation()
{
	return InteractionType;
}

EMouseAxis ABaseInteractable::GetPreferredMouseAxis_Implementation()
{
	return PreferredMouseAxis;
}

void ABaseInteractable::GrabObject_Implementation(AActor* Grabber)
{
	bIsHeld = true;
}

void ABaseInteractable::ReleaseObject_Implementation()
{
	bIsHeld = false;
}

void ABaseInteractable::SlamObject_Implementation(FVector PushDirection, float PushForce)
{
	bIsHeld = false;

	// Jeśli rzucamy granat z zapalnikiem czasowym -> start zegara eksplozji!
	if (bIsStateEmitter && TriggerType == EEmissionTrigger::TimedFuse)
	{
		GetWorld()->GetTimerManager().SetTimer(FuseTimerHandle, this, &ABaseInteractable::TriggerStateEmission, FuseTime, false);
	}
}

void ABaseInteractable::MoveObject_Implementation(float AxisDelta)
{
}

bool ABaseInteractable::IsLocked_Implementation()
{
	return bIsLocked;
}

void ABaseInteractable::SetLocked_Implementation(bool bNewLocked)
{
	bIsLocked = bNewLocked;
}

void ABaseInteractable::OnLockedInteraction_Implementation(AActor* InstigatorActor)
{
	if (!bIsLocked) return;

	// 1. MECHANIKA PAMIĘCI: Sprawdzamy plecak TYLKO jeśli zamek został już wcześniej ODKRYTY Z RĘKI!
	if (bKeyDiscovered && InstigatorActor)
	{
		if (UInventoryComponent* InvComp = InstigatorActor->FindComponentByClass<UInventoryComponent>())
		{
			// Automatycznie otwieramy zapamiętanym kluczem z plecaka:
			if (InvComp->HasItemWithTag(RequiredKeyTag))
			{
				bIsLocked = false; // ZAMEK PUSZCZA!

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("[ZAMEK] Odryglowano automatycznie zapamietanym kluczem z plecaka!"));
				}
				return;
			}
		}
	}

	// 2. Jeśli zamek jest NIEODKRYTY (nawet jeśli masz klucz w plecaku, musisz go najpierw wyciągnąć do ręki!):
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[ZAMEK] Zamek jest zaryglowany. Musisz wyciagnac i recznie dopasowac klucz z dloni!"));
	}
}


bool ABaseInteractable::IsLatched_Implementation()
{
	return bIsLatched;
}

FGameplayTag ABaseInteractable::GetPropSizeTag_Implementation()
{
	return PropSizeTag;
}

bool ABaseInteractable::IsSmallProp_Implementation()
{
	return PropSizeTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Prop.Size.Small")));
}