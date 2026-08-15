#include "BaseInteractable.h"
#include "HealthComponent.h"
#include "ReactionReceiverComponent.h"
#include "Components/PrimitiveComponent.h"

ABaseInteractable::ABaseInteractable()
{
	PrimaryActorTick.bCanEverTick = true;

	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	ReactionComp = CreateDefaultSubobject<UReactionReceiverComponent>(TEXT("ReactionReceiverComponent"));
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
	// WŁĄCZAMY REJESTROWANIE UDERZEŃ DLA WSZYSTKICH BRYŁ (Nawet nieruchomych drzwi!):
	// ====================================================================
	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Prim : PrimComps)
	{
		if (Prim)
		{
			Prim->SetNotifyRigidBodyCollision(true); // Włącza rejestrowanie zderzeń!
			Prim->OnComponentHit.AddDynamic(this, &ABaseInteractable::OnHit);
		}
	}
}

void ABaseInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABaseInteractable::HandleStateApplied(FGameplayTag StateTag, float Intensity)
{
	if (!bCanBeDestroyed) return;

	if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("State.Element.Thermal"))))
	{
		if (HealthComp)
		{
			// ====================================================================
			// DYNAMICZNE OBRAŻENIA ŻYWIOŁU: 25 HP * Moc Źródła * Mnożnik Obiektu!
			// ====================================================================
			float FinalDamage = (25.0f * Intensity) * CustomDamageMultiplier;
			HealthComp->TakeDamage(FinalDamage, StateTag);
		}
	}
}

void ABaseInteractable::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bIsHeld) return;
	if (OtherActor && OtherActor->IsA<APawn>()) return;
	if (ABaseInteractable* OtherInteractable = Cast<ABaseInteractable>(OtherActor))
	{
		if (OtherInteractable->bIsHeld) return;
	}

	float ImpactForce = NormalImpulse.Size();
	if (ImpactForce < MinImpactForceToDamage) return;

	float SelfMaterialVulnerability = 1.0f;
	if (MaterialTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Material.Glass"))))
	{
		SelfMaterialVulnerability = 5.0f;
	}
	else if (MaterialTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Material.Metal"))))
	{
		SelfMaterialVulnerability = 0.2f;
	}

	// 1. Obrażenia własne (uwzględniające CustomDamageMultiplier!):
	if (bCanBeDestroyed && HealthComp)
	{
		float BaseDamage = (ImpactForce - MinImpactForceToDamage) / 1000.0f;
		float FinalSelfDamage = FMath::Clamp(BaseDamage * SelfMaterialVulnerability * CustomDamageMultiplier, 0.0f, 80.0f);

		if (FinalSelfDamage > 2.0f)
		{
			HealthComp->TakeDamage(FinalSelfDamage, FGameplayTag());
		}
	}

	// 2. Obrażenia celu (np. rzucona skrzynia uderza w drzwi):
	if (OtherActor && OtherActor != this)
	{
		if (ABaseInteractable* TargetInteractable = Cast<ABaseInteractable>(OtherActor))
		{
			if (TargetInteractable->bCanBeDestroyed && TargetInteractable->HealthComp)
			{
				float TargetMatVulnerability = 1.0f;
				if (MaterialTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Material.Metal"))))
				{
					TargetMatVulnerability = 2.0f;
				}

				float BaseTargetDamage = (ImpactForce - MinImpactForceToDamage) / 800.0f;
				// Uwzględniamy CustomDamageMultiplier celu!
				float FinalTargetDamage = FMath::Clamp(BaseTargetDamage * TargetMatVulnerability * TargetInteractable->CustomDamageMultiplier, 5.0f, 100.0f);

				TargetInteractable->HealthComp->TakeDamage(FinalTargetDamage, FGameplayTag());
			}
		}
	}
}

void ABaseInteractable::HandleDeath()
{
	bIsBroken = true;
	Destroy(); // Czyste usunięcie ze świata
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

// ==========================================================
// FUNKCJE INTERFEJSU
// ==========================================================
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