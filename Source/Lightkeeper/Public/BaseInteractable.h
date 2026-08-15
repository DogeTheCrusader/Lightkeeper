#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalInteract.h"
#include "GameplayTagContainer.h"
#include "BaseInteractable.generated.h"

class UHealthComponent;
class UReactionReceiverComponent;

UCLASS(Blueprintable)
class LIGHTKEEPER_API ABaseInteractable : public AActor, public IPhysicalInteract
{
	GENERATED_BODY()

public:
	ABaseInteractable();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// ====================================================================
	// 1. TYP INTERAKCJI (Główny przełącznik)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	EInteractionType InteractionType = EInteractionType::Grab_Free;

	// Oś myszki wyświetla się TYLKO dla Drzwi (Hinge) i Szuflad (Translation):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation", EditConditionHides))
	EMouseAxis PreferredMouseAxis = EMouseAxis::MouseX;

	// Stałe siły i opory wyświetlają się TYLKO dla Mebli i Mechanizmów (Hinge, Translation, Crank):
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Crank", EditConditionHides))
	float BaseInteractionPower = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Crank", EditConditionHides))
	float ReferenceMass = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Crank", EditConditionHides))
	float MechanicalFriction = 1.0f;

	// Rozmiar Propa wyświetla się TYLKO dla wolnych przedmiotów (Grab_Free):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Grab_Free", EditConditionHides))
	FGameplayTag PropSizeTag;

	// Materiał (Drewno, Metal, Szkło) dotyczy KAŻDEGO obiektu w grze:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	FGameplayTag MaterialTag;

	// ====================================================================
	// 2. ZMIENNE STANU
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsHeld = false; // Widoczne tylko do podglądu (ustawiane przez C++)

	// Zatrzask w ramie wyświetla się TYLKO dla Drzwi (Hinge) i Zasuwek (Bolt):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Bolt", EditConditionHides))
	bool bIsLatched = true;

	// Zamek na klucz (Dla Drzwi, Szuflad i Zasuwek):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State",
		meta = (EditCondition = "InteractionType != EInteractionType::Grab_Free", EditConditionHides))
	bool bIsLocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsBroken = false;

	// ====================================================================
	// 3. SYSTEM ZNISZCZEŃ I ŻYWIOŁÓW (ImSim)
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UReactionReceiverComponent* ReactionComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction")
	bool bCanBeDestroyed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction",
		meta = (EditCondition = "bCanBeDestroyed", EditConditionHides))
	float MinImpactForceToDamage = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction",
		meta = (EditCondition = "bCanBeDestroyed", EditConditionHides))
	float CustomDamageMultiplier = 1.0f;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Physics")
	float CalculateMovementResistance(UPrimitiveComponent* MovingComponent);

protected:
	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	virtual void HandleStateApplied(FGameplayTag StateTag, float Intensity);

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:
	// ====================================================================
	// 4. FUNKCJE INTERFEJSU C++
	// ====================================================================
	virtual void GrabObject_Implementation(AActor* Grabber) override;
	virtual void ReleaseObject_Implementation() override;
	virtual void SlamObject_Implementation(FVector PushDirection, float PushForce) override;
	virtual void MoveObject_Implementation(float AxisDelta) override;
	virtual EInteractionType GetInteractionType_Implementation() override;
	virtual EMouseAxis GetPreferredMouseAxis_Implementation() override;
	virtual bool IsLocked_Implementation() override;
	virtual void OnLockedInteraction_Implementation(AActor* InstigatorActor) override;
	virtual void SetLocked_Implementation(bool bNewLocked) override;
	virtual bool IsLatched_Implementation() override;
	virtual FGameplayTag GetPropSizeTag_Implementation() override;
	virtual bool IsSmallProp_Implementation() override;
};