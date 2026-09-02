#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicalInteract.h"
#include "InteractionComponent.generated.h"

class APlayerCameraManager;
class UPhysicsHandleComponent;
class USceneComponent;
class UPrimitiveComponent;

// Stany dynamicznego celownika
UENUM(BlueprintType)
enum class ECrosshairState : uint8
{
	Default		UMETA(DisplayName = "Default (Biały - Neutralny)"),
	Interactive	UMETA(DisplayName = "Interactive (Czerwony - Drzwi / Szuflady / Wajchy)"),
	Grabable	UMETA(DisplayName = "Grabable (Żółty - Wolne Rekwizyty / Butelki)")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ====================================================================
	// 1. SKANOWANIE WZROKIEM I ZASIĘG (Raycasting & Trace)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float InteractionDistance = 150.0f; // Maksymalny zasięg wzroku gracza (2.5 metra)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float BreakDistanceBuffer = 50.0f; // Bufor zerwania chwytu przy cofaniu

	// Hybrydowy skan wzroku (LineTrace + SphereSweep)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	bool PerformLineTrace(FHitResult& OutHit);

	// ====================================================================
	// 2. FIZYCZNY CHWYT I CIĄGNIĘCIE (Amnesia Hand Physics)
	// ====================================================================
	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Interaction")
	AActor* GrabbedActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float WeightSpeedReductionFactor = 0.025f; // Jak bardzo ciężkie obiekty spowalniają bieg

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float MinCarryingSpeedRatio = 0.15f; // Minimalna prędkość chodu z ciężarem (15%)

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void StartInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void StopInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void QuickInteraction();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ZoomHoldSlot(float ScrollDelta); // Przybliżanie/oddalanie kółkiem myszy

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	float CalculateMovementSpeed(float BaseSpeed, float MassInKg) const;

	//UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	//void OnGrabbedComponentHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// ====================================================================
	// 3. RZUCANIE I PĘD FIZYCZNY (Charged Throw & Slam)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float BaseThrowPower = 500.0f; // Bazowa siła rzutu gracza

	// Maksymalny mnożnik siły po pełnym naładowaniu (np. 1.8 = +80% siły i zasięgu)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float MaxChargedThrowMultiplier = 1.8f;

	// Ile sekund trzeba trzymać PPM, by osiągnąć 100% siły rzutu (np. 1.0 sekunda)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Physics")
	float MaxChargeTime = 1.0f;

	// Jedno Źródło Prawdy: Oblicza prędkość rzutu wg Twojego wzoru (Krzywa Sqrt + Bieg + Naładowanie)
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Physics")
	FVector CalculateThrowVelocity(const FVector& Direction, float ObjectMass, float ChargeMultiplier = 1.0f) const;

	// Wyrzut niesionego rekwizytu (Slam)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Physics")
	void SlamInteraction(float CustomMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Physics")
	void StartSlamCharge();

	// Wyrzuca niesiony obiekt z siłą proporcjonalną do czasu trzymania PPM
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Physics")
	void ReleaseSlamThrow();

	// ====================================================================
	// 4. INSPEKCJA I OBRACANIE PRZEDMIOTÓW W DŁONIACH
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	bool bIsInspecting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Interaction")
	float InspectWeightMultiplier = 0.35f;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ToggleInspectMode();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Interaction")
	bool IsInspecting() const { return bIsInspecting; }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	bool ProcessMouseLook(float MouseX, float MouseY, float CameraSensitivity);

	// ====================================================================
	// 5. EKWIPUNEK I ZUŻYWANIE POD KLAWISZEM [E]
	// ====================================================================
	// Szybkie podniesienie do plecaka (Tap [E])
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void TryPickupFocusedObject();

	// Szybkie wypicie/zużycie ze stołu bez plecaka (Hold [E])
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void TryQuickConsumeFocusedObject();

	// ====================================================================
	// 6. DYNAMICZNY CELOWNIK (Crosshair)
	// ====================================================================
	UPROPERTY(BlueprintReadOnly, Category = "Lightkeeper|Interaction")
	ECrosshairState CurrentCrosshairState = ECrosshairState::Default;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	ECrosshairState GetCrosshairState() const { return CurrentCrosshairState; }

	// ====================================================================
	// 7. GETTERY POMOCNICZE
	// ====================================================================
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Interaction")
	UPrimitiveComponent* GetGrabbedComponent() const { return GrabbedComponent; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Interaction")
	AActor* GetGrabbedActor() const { return GrabbedActor; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	UPhysicsHandleComponent* PhysicsHandle = nullptr;

	UPROPERTY()
	USceneComponent* HoldSlotComponent = nullptr;

private:
	UPROPERTY()
	UPrimitiveComponent* GrabbedComponent = nullptr;

	UPROPERTY()
	APlayerCameraManager* CachedCameraManager = nullptr;

	FVector InitialHoldSlotLocation;
	FRotator InitialHoldSlotRotation;
	FVector SmoothedHoldLocation;
	float SmoothedHoldDistance = 120.0f;
	float CurrentBaseHoldDistance = 120.0f;
	float CameraBlendAlpha = 1.0f;
	FQuat InitialGrabQuat = FQuat::Identity;
	ECollisionResponse OriginalPawnResponse = ECR_Block;
	float SlamChargeStartTime = 0.0f;

	float AccumulatedMechanismEffort = 0.0f;
	float LastMechanismPainTime = 0.0f;

	float ObstacleLagTimer = 0.0f;

	float LastManipulationNoiseTime = 0.0f;

	void CleanupInteraction();
	APlayerCameraManager* GetCameraManager();
	void UpdateCrosshairState(EInteractionType HeldType, bool bIsHoldingObject);

	float HeldPropBurnExposureTimer = 0.0f;
	void ProcessHeldObjectHazardConduction(float DeltaTime);
};