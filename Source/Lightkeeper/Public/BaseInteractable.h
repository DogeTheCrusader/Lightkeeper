#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalInteract.h"
#include "BaseInteractable.generated.h"

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

	// ==========================================================
	// UNIWERSALNE ZMIENNE FIZYKI (Wszystko w jednej kategorii!)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	EInteractionType InteractionType = EInteractionType::Grab_Free; // Domyślnie Drzwi

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	EMouseAxis PreferredMouseAxis = EMouseAxis::MouseX; // Domyślnie Myszka X

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float InteractionForce = 50.0f; // Uniwersalna siła (Drzwi/Szuflada/Zawór)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float SlamForce = 1600.0f; // Uniwersalna siła trzaśnięcia/rzutu (RMB)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	bool bIsHeld = false; // Czy gracz obecnie to trzyma?

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State")
	bool bIsLatched = true; // Czy obiekt jest zatrzaśnięty w ramie?	

	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|State")
	bool bIsBroken = false; // Czy obiekt został zniszczony?

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|State")
	bool bIsLocked = false;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lightkeeper|Interaction")
	void BreakObject(); // Uniwersalna funkcja zniszczenia!

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	bool bIsSmallProp = false; // Domyślnie FALSE (duże obiekty). Dla puszek/butelek zaznaczasz TRUE!

	// ==========================================================
	// FUNKCJE INTERFEJSU C++
	// ==========================================================

	virtual void GrabObject_Implementation(AActor* Grabber) override;
	virtual void ReleaseObject_Implementation() override;
	virtual void SlamObject_Implementation(FVector PushDirection) override;
	virtual void MoveObject_Implementation(float AxisDelta) override;
	virtual EInteractionType GetInteractionType_Implementation() override;
	virtual EMouseAxis GetPreferredMouseAxis_Implementation() override;
	virtual bool IsLocked_Implementation() override;
	virtual void OnLockedInteraction_Implementation(AActor* InstigatorActor) override;
	virtual void SetLocked_Implementation(bool bNewLocked) override;
	virtual bool IsLatched_Implementation() override;
	virtual bool IsSmallProp_Implementation() override;
};
