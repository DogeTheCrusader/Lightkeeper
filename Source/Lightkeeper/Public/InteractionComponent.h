#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicalInteract.h"
#include "InteractionComponent.generated.h"

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

	// Ustawienia zasięgu i czułości
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float InteractionDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float BreakDistanceBuffer = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Interaction")
	AActor* GrabbedActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	bool bIsInspecting = false;

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
	void QuickInteraction(); // Klawisz E (Szybki klik)

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	bool ProcessMouseLook(float MouseX, float MouseY, float CameraSensitivity);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ZoomHoldSlot(float ScrollDelta);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	ECrosshairState GetCrosshairState() const { return CurrentCrosshairState; }

	// Zmienna przechowująca stan (ustawiana w Ticku):
	ECrosshairState CurrentCrosshairState = ECrosshairState::Default;

private:
	UPROPERTY()
	UPrimitiveComponent* GrabbedComponent = nullptr;
	FVector InitialHoldSlotLocation;
	FRotator InitialHoldSlotRotation;
	ECollisionResponse OriginalPawnResponse = ECR_Block;
};