#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalInteract.h"
#include "GameplayTagContainer.h"
#include "InventoryTypes.h"
#include "Engine/EngineTypes.h"
#include "BaseInteractable.generated.h"

// Deklaracje wyprzedzające (Forward Declarations) dla szybszej kompilacji
class UHealthComponent;
class UReactionReceiverComponent;

// ====================================================================
// TYPY WYZWALACZY EMISJI ŻYWIOŁÓW (ImSim Emitter)
// ====================================================================
UENUM(BlueprintType)
enum class EEmissionTrigger : uint8
{
	OnDestroy		UMETA(DisplayName = "Przy Zniszczeniu (Kruche butelki, Mołotowy - wybucha gdy pęknie)"),
	OnImpact		UMETA(DisplayName = "Przy Uderzeniu (Wybuch/Fala przy kontakcie - obiekt może przetrwać!)"),
	TimedFuse		UMETA(DisplayName = "Zapalnik Czasowy (Granaty zegarowe)"),
	Proximity		UMETA(DisplayName = "Zbliżeniowy / Naciskowy (Miny)"),
	ContinuousZone	UMETA(DisplayName = "Ciągły / Strefowy (Pęknięte rury, Ognisko)"),
	Manual			UMETA(DisplayName = "Ręczny")
};

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

	// Oś myszki (Tylko dla Hinge i Translation)
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

	// Rozmiar Propa do systemu kopania (Tylko dla Grab_Free)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics",
		meta = (EditCondition = "InteractionType == EInteractionType::Grab_Free", EditConditionHides))
	FGameplayTag PropSizeTag;

	// Materiał obiektu (Drewno, Metal, Szkło, Ciało)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	FGameplayTag MaterialTag;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Physics")
	float CalculateMovementResistance(UPrimitiveComponent* MovingComponent);

	// ====================================================================
	// 2. ZMIENNE STANU I ZAMKÓW
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsHeld = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State",
		meta = (EditCondition = "InteractionType == EInteractionType::Hinge || InteractionType == EInteractionType::Bolt || EInteractionType::Translation", EditConditionHides))
	bool bIsLatched = true;

	// Czy zamek jest zamknięty na klucz?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State",
		meta = (EditCondition = "InteractionType != EInteractionType::Grab_Free", EditConditionHides))
	bool bIsLocked = false;

	// Jaki klucz otwiera ten zamek? (np. Item.Key.Brass.Basement)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State",
		meta = (EditCondition = "bIsLocked", EditConditionHides))
	FGameplayTag RequiredKeyTag;

	// Czy gracz dopasował już kiedyś właściwy klucz do tych drzwi? (Pamięć Zamka)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bKeyDiscovered = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsBroken = false;

	// Próba włożenia trzymanego klucza do zamka
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|State")
	bool TryUnlockWithKey(AActor* KeyActor, AActor* InstigatorActor);

	// ====================================================================
// 3. SYSTEM ZNISZCZEŃ I FIZYKI MATERIAŁÓW (ImSim Receiver)
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UReactionReceiverComponent* ReactionComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction")
	bool bCanBeDestroyed = false;

	// Jak twardy jest ten obiekt w ataku? (Szkło = 0.2, Drewno = 1.0, Metal = 2.5 [masakruje cel])
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction")
	float ImpactHardness = 1.0f;

	// Jak bardzo sam obrywa przy uderzeniu? (Szkło = 8.0 [kruche], Drewno = 1.0, Metal = 0.2 [odporny])
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction",
		meta = (EditCondition = "bCanBeDestroyed", EditConditionHides))
	float DamageSusceptibility = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Destruction",
		meta = (EditCondition = "bCanBeDestroyed", EditConditionHides))
	float CustomDamageMultiplier = 1.0f;

	// ====================================================================
	// 4. EKWIPUNEK I PODNOSZENIE [E] (Inventory Grid)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Inventory")
	bool bCanBePocketed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Inventory",
		meta = (EditCondition = "bCanBePocketed", EditConditionHides))
	FInventoryItemData ItemData;

	// Tagi, które blokują schowanie (Ogień, Kwas, Prąd) - dziedziczy State.Hazard
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lightkeeper|Inventory",
		meta = (EditCondition = "bCanBePocketed", EditConditionHides))
	FGameplayTagContainer BlockingHazardStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Inventory",
		meta = (EditCondition = "bCanBePocketed", EditConditionHides))
	bool bCanBeConsumed = false;

	// ====================================================================
	// 5. ROZPRZESTRZENIANIE ŻYWIOŁÓW (ImSim Emitter / Granaty / Rury)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter")
	bool bIsStateEmitter = false;

	// Kiedy żywioł ma wybuchnąć / się uaktywnić?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	EEmissionTrigger TriggerType = EEmissionTrigger::OnDestroy;

	// Czy obiekt niszczy się po emisji żywiołu? (Mołotow = TRUE, Dzwon/Rura = FALSE)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	bool bDestroyOnEmission = true;

	// Stan, który obiekt rozpyla (np. State.Element.Thermal.Fire lub State.Element.Pressure.Steam)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	FGameplayTag EmittedStateTag;

	// Zasięg wybuchu/rozlania w centymetrach
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	float SplashRadius = 150.0f;

	// Intensywność nałożonego stanu
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	float SplashIntensity = 1.0f;

	// Czas zapalnika w sekundach (Dla bomb zegarowych i granatów)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter && TriggerType == EEmissionTrigger::TimedFuse", EditConditionHides))
	float FuseTime = 3.5f;

	// Odstęp czasu między uderzeniami strefy (Dla rur z parą / ogniska)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|ImSim Emitter",
		meta = (EditCondition = "bIsStateEmitter && TriggerType == EEmissionTrigger::ContinuousZone", EditConditionHides))
	float EmissionInterval = 0.5f;

	// Główna funkcja wybuchu/emisji stanu (Można wywołać z kodu lub Blueprintu)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim Emitter")
	void TriggerStateEmission();

	// Funkcja do naprawy rur/wyłączenia pułapek (np. Perkiem Inżynierii)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|ImSim Emitter")
	void DeactivateEmitter();

protected:
	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	virtual void HandleStateApplied(FGameplayTag StateTag, float Intensity);

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	FTimerHandle FuseTimerHandle;
	FTimerHandle ContinuousTimerHandle;
	float LastHitTime = 0.0f;

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
};