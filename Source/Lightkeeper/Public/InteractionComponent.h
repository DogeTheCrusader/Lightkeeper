#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicalInteract.h"
#include "InteractionComponent.generated.h"

class APlayerCameraManager;

UENUM(BlueprintType)
enum class ECrosshairState : uint8
{
	Default		UMETA(DisplayName = "Default (Biały)"),
	Interactive	UMETA(DisplayName = "Interactive (Czerwony - Drzwi/Wajchy)"),
	Grabable	UMETA(DisplayName = "Grabable (Żółty - Free Props)")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	bool bIsInspecting = false;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ToggleInspectMode();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void TryPickupFocusedObject();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void TryQuickConsumeFocusedObject();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Interaction")
	bool IsInspecting() const { return bIsInspecting; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float InspectWeightMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float InteractionDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float BreakDistanceBuffer = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float WeightSpeedReductionFactor = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float MinCarryingSpeedRatio = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float BaseThrowPower = 1500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Interaction")
	AActor* GrabbedActor = nullptr;

	float CameraBlendAlpha = 1.0f;
	FQuat InitialGrabQuat = FQuat::Identity;

	UPROPERTY()
	class UPhysicsHandleComponent* PhysicsHandle = nullptr;

	UPROPERTY()
	class USceneComponent* HoldSlotComponent = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	bool PerformLineTrace(FHitResult& OutHit);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void StartInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void StopInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void SlamInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void QuickInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	bool ProcessMouseLook(float MouseX, float MouseY, float CameraSensitivity);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ZoomHoldSlot(float ScrollDelta);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	ECrosshairState GetCrosshairState() const { return CurrentCrosshairState; }

	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Interaction")
	ECrosshairState CurrentCrosshairState = ECrosshairState::Default;

	float CalculateMovementSpeed(float BaseSpeed, float MassInKg) const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Interaction")
	UPrimitiveComponent* GetGrabbedComponent() const { return GrabbedComponent; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Interaction")
	AActor* GetGrabbedActor() const { return GrabbedActor; }

private:
UPROPERTY()
	UPrimitiveComponent* GrabbedComponent = nullptr;

	UPROPERTY()
	APlayerCameraManager* CachedCameraManager = nullptr;

	FVector InitialHoldSlotLocation;
	FRotator InitialHoldSlotRotation;
	FVector SmoothedHoldLocation;
	float SmoothedHoldDistance = 120.0f; // Płynny dystans Auto-Zoomu
	ECollisionResponse OriginalPawnResponse = ECR_Block;
	float CurrentBaseHoldDistance = 120.0f;

	void CleanupInteraction();
	APlayerCameraManager* GetCameraManager();
	void UpdateCrosshairState(EInteractionType HeldType, bool bIsHoldingObject);
};