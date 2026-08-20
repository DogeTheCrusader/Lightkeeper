#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "PhysicalInteract.generated.h"

// 1. NASZ SŁOWNIK (ENUM) - Dodajemy go tutaj, żeby cały projekt go widział
UENUM(BlueprintType)
enum class EInteractionType : uint8
{
	Grab_Free	UMETA(DisplayName = "Free Prop (Puszki/Skrzynki)"),
	Hinge		UMETA(DisplayName = "Hinge (Zawias/Drzwi)"),
	Translation UMETA(DisplayName = "Translation (Szyna/Szuflada)"),
	Crank		UMETA(DisplayName = "Crank (Korba/Zawór)"),
	Bolt		UMETA(DisplayName = "Bolt (Zasuwka)")
};

UENUM(BlueprintType)
enum class EMouseAxis : uint8
{
	MouseX		UMETA(DisplayName = "Mouse X (Lewo / Prawo)"),
	MouseY		UMETA(DisplayName = "Mouse Y (Gora / Dol)"),
	InvertedX	UMETA(DisplayName = "Inverted Mouse X"),
	InvertedY	UMETA(DisplayName = "Inverted Mouse Y (Do Siebie = Otwórz)"),
	Auto_CameraRelative		UMETA(DisplayName = "AUTO (Kamera sama wybiera os!)")
};

// 2. KLASA BAZOWA INTERFEJSU (Wymagane przez Unreala)
UINTERFACE(MinimalAPI)
class UPhysicalInteract : public UInterface
{
	GENERATED_BODY()
};

// 3. WŁAŚCIWY INTERFEJS Z FUNKCJAMI
class LIGHTKEEPER_API IPhysicalInteract
{
	GENERATED_BODY()

public:

	// Funkcja Łapania (Kto nas łapie?)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void GrabObject(AActor* Grabber);

	// Funkcja Puszczania
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void ReleaseObject();

	// Funkcja Trzaśnięcia/Wyważenia (Z jakiego kierunku?)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void SlamObject(FVector PushDirection, float PushForce);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void MoveObject(float AxisDelta);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	EMouseAxis GetPreferredMouseAxis();

	// Pytanie do obiektu: Czym jesteś? (Drzwiami, Szufladą?)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	EInteractionType GetInteractionType();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnLockedInteraction(AActor* InstigatorActor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsLocked();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void SetLocked(bool bNewLocked);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsLatched();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FGameplayTag GetPropSizeTag();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsSmallProp();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanBePocketed();

	// Szybka akcja pod klawiszem [E] (Podniesienie do kieszeni, wciśnięcie guzika)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void PickupObject(AActor* InstigatorActor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool ConsumeObject(AActor* InstigatorActor);
};