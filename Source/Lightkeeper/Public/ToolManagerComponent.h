#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BaseTool.h"
#include "ToolManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolEquipped, ABaseTool*, NewTool);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToolHolstered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAimStateChanged, bool, bIsAiming);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UToolManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolManagerComponent();

	// ==========================================================
	// 1. KONFIGURACJA I AKTUALNA BROŃ
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Equipment")
	TSubclassOf<ABaseTool> DefaultToolClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Equipment")
	FName RightHandSocketName = TEXT("Hand_R_Socket");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Equipment")
	ABaseTool* CurrentEquippedTool = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Equipment")
	TSubclassOf<ABaseTool> SignatureToolClass;

	// Klawisz [1] -> Wyciąga Kij do dłoni / Chowa Kij na plecy (Toggle)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Equipment")
	void ToggleSignatureTool();

	// ==========================================================
	// 2. DELEGATY
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Equipment")
	FOnToolEquipped OnToolEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Equipment")
	FOnToolHolstered OnToolHolstered;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Equipment")
	FOnAimStateChanged OnAimStateChanged;

	// ==========================================================
	// 3. STEROWANIE WEJŚCIAMI (PPM / LPM / V / R)
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_Aim_Pressed();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_Aim_Released();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_PrimaryAction_Pressed();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_PrimaryAction_Released();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_QuickMelee();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_Reload();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Input")
	bool bIsAimInputHeld = false;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Input")
	bool IsAimInputHeld() const { return bIsAimInputHeld; }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_Guard_Pressed();

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Input")
	void Input_Guard_Released();

	// ==========================================================
	// 4. FUNKCJE EKWIPUNKU I WYCIĄGANIA BRONI
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Equipment")
	void EquipToolByClass(TSubclassOf<ABaseTool> ToolClass);

	// <--- O TĘ DEKLARACJĘ CHODZIŁO!
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Equipment")
	void EquipToolFromInventorySlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Equipment")
	void HolsterCurrentTool();

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Equipment")
	bool HasEquippedTool() const { return CurrentEquippedTool != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Equipment")
	bool IsAiming() const;

protected:
	virtual void BeginPlay() override;

private:
	void AttachToolToHand(ABaseTool* ToolToAttach);
	bool IsPlayerBusyWithInteraction() const;
};