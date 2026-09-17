#pragma once

#include "CoreMinimal.h"
#include "BaseInteractable.h"
#include "BaseDoorInteractable.generated.h"

class UPhysicsConstraintComponent;

UCLASS(Blueprintable)
class LIGHTKEEPER_API ABaseDoorInteractable : public ABaseInteractable
{
	GENERATED_BODY()

public:
	ABaseDoorInteractable();

	virtual void Tick(float DeltaTime) override;

	// ====================================================================
	// 1. KOMPONENTY ARCHITEKTURY DRZWI
	// C++ samo buduje hierarchię, Blueprint będzie tylko "skórką"!
	// ====================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Door")
	USceneComponent* DoorRoot; // Nieruchomy punkt odniesienia

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Door")
	UStaticMeshComponent* DoorFrame; // Nieruchoma ościeżnica

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Door")
	UStaticMeshComponent* DoorMesh; // Ruchome skrzydło

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Door")
	UPhysicsConstraintComponent* HingeConstraint; // Zawias

	// ====================================================================
	// 2. PARAMETRY FIZYCZNE (Do ustawiania w Edytorze)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Hinge")
	float MaxOpenAngle = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Hinge")
	bool bBidirectional = false; // Czy mogą otwierać się w obie strony?

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Hinge")
	float DoorMass = 25.0f; // Masa skrzydła dla siły pchnięcia

	// ====================================================================
	// 3. OVERRIDY INTERFEJSU
	// ====================================================================
	virtual void GrabObject_Implementation(AActor* Grabber) override;
	virtual void MoveObject_Implementation(float AxisDelta) override;
	virtual void SetLocked_Implementation(bool bNewLocked) override;

protected:
	virtual void BeginPlay() override;

private:
	// Zmienne do efektu "Nudge" (odskok klamki)
	bool bIsAnimatingNudge = false;
	float NudgeTimer = 0.0f;

	void SetupHinge();
	void SnapToClosedPosition();
};