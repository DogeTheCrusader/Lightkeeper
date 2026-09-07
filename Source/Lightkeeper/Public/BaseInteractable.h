#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalInteract.h"
#include "GameplayTagContainer.h"
#include "InventoryTypes.h"
#include "Engine/EngineTypes.h"
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
	// 1. FIZYKA I MECHANIKA MANIPULACJI (Amnesia Hand Physics)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	EInteractionType InteractionType = EInteractionType::Grab_Free;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation", EditConditionHides))
	EMouseAxis PreferredMouseAxis = EMouseAxis::MouseX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Crank", EditConditionHides))
	float BaseInteractionPower = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Crank", EditConditionHides))
	float ReferenceMass = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Translation || InteractionType == EInteractionType::Crank", EditConditionHides))
	float MechanicalFriction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Grab_Free", EditConditionHides))
	FGameplayTag PropSizeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	FGameplayTag MaterialTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics", meta = (ClampMin = "0.0", ToolTip = "Mnożnik hałasu: 1.0 = standard, 2.0 = skrzypiąca deska/pułapka, 0.2 = cichy mebel."))
	float AcousticNoiseMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaterialPurity = 1.0f;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Physics")
	float CalculateMovementResistance(UPrimitiveComponent* MovingComponent);

	// ====================================================================
	// 2. ZMIENNE STANU I ZAMKÓW (Metroidvania)
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsHeld = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Bolt || InteractionType == EInteractionType::Translation", EditConditionHides))
	bool bIsLatched = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsBroken = false;

	//UFUNCTION(BlueprintCallable, Category = "Lightkeeper|State")
	//void TryUnlockFromInput(AActor* InstigatorActor);

	// ====================================================================
	// 3. SYSTEM ZNISZCZEŃ I FIZYKI MATERIAŁÓW (ImSim Receiver)
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UReactionReceiverComponent* ReactionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UMetroidvaniaGateComponent* GateComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction")
	float ImpactHardness = 1.0f;

	// ====================================================================
	// 4. JEDNO ŹRÓDŁO PRAWDY: DANE PRZEDMIOTU I WYBUCHÓW (Master Struct)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Inventory")
	bool bCanBePocketed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Inventory",
		meta = (EditCondition = "bCanBePocketed", EditConditionHides))
	bool bCanBeConsumed = false;

	// Główna struktura danych – zawiera model, tagi, skalę, obrażenia oraz parametry wybuchów:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Inventory")
	FInventoryItemData ItemData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Inventory",
		meta = (EditCondition = "bCanBePocketed", EditConditionHides))
	FGameplayTagContainer BlockingHazardStates;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Inventory")
	virtual void CaptureItemData();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Inventory")
	virtual void ApplyItemData(const FInventoryItemData& InData);

	// ====================================================================
	// 5. FUNKCJE EMITERA I PUŁAPEK (Czytają bezpośrednio z ItemData!)
	// ====================================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim Emitter")
	void TriggerStateEmission();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim Emitter")
	void DeactivateEmitter();

	virtual void EndPersistentZone();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Chemistry")
	void FillContainerWithLiquid(FGameplayTag LiquidElementTag, float Purity = 1.0f);

protected:
	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	virtual void HandleStateApplied(FGameplayTag StateTag, float Intensity);

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	FTimerHandle ZoneExpiryTimerHandle;
	FTimerHandle ContinuousTimerHandle;
	FTimerHandle FuseTimerHandle;
	float LastReleaseTime = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Door")
	bool bIsLocked = false;

private:
	float LastHitTime = 0.0f;
	FVector LastImpactNormal = FVector::UpVector;
	float LastEmissionTime = 0.0f;
	float LastNoiseTime = 0.0f;

public:
	// ====================================================================
	// 6. IMPLEMENTACJA INTERFEJSU (IPhysicalInteract)
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
	virtual bool CanBePocketed_Implementation() override;
	virtual void PickupObject_Implementation(AActor* InstigatorActor) override;
	virtual bool ConsumeObject_Implementation(AActor* InstigatorActor) override;
	virtual void TryUnlockFromInput_Implementation(AActor* InstigatorActor) override;
	virtual FGameplayTag GetMaterialTag_Implementation() override { return MaterialTag; }
	virtual float GetMaterialPurity_Implementation() override { return MaterialPurity; }
	virtual float GetAcousticNoiseMultiplier_Implementation() override { return AcousticNoiseMultiplier; }
};