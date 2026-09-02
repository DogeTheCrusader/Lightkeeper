#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MetroidvaniaGateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGateUnlocked, FName, UnlockMethod);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGateStateChanged, bool, bIsLocked);

class ALightkeeperCharacter;
class UHealthComponent;
class UReactionReceiverComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UMetroidvaniaGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMetroidvaniaGateComponent();

	// Stan blokady:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate")
	bool bIsLocked = true;

	// Czy zamek został już kiedyś odkryty (do szybkiego ryglowania z plecaka):
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Gate")
	bool bKeyDiscovered = false;

	// Wymagany klucz (np. Item.Key.Brass):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate|Requirements")
	FGameplayTag RequiredKeyTag;

	// Wymagany Perk do cichego otwarcia (np. Perk.Precision.2A):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate|Requirements")
	FGameplayTag RequiredPerkTag;

	// Czy zamek można stopić kwasem:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate|Bypass")
	bool bCanBeMeltedByAcid = true;

	// Czy zamek można wyważyć siłą fizyczną (Młot / Łom / Taran):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate|Bypass")
	bool bCanBeForcedByDamage = true;

	// Delegaty:
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Gate")
	FOnGateUnlocked OnGateUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Gate")
	FOnGateStateChanged OnGateStateChanged;

	// Główna funkcja otwierania:
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Gate")
	bool TryUnlock(AActor* Instigator, FGameplayTag ToolOrKeyTag);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Gate")
	void UnlockGate(FName MethodName);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Gate")
	void SetGateLocked(bool bNewLocked);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Lightkeeper|Gate", meta = (ToolTip = "Wskaż pipetą drzwi lub mebel, który ta kłódka ma trzymać zablokowany."))
	TObjectPtr<AActor> TargetObjectToLock;

	// Czy po otwarciu kłódka ma z brzękiem odpaść i spaść na podłogę (fizyka):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate")
	bool bDropWithPhysicsOnUnlock = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gate")
	bool bCanBeRelocked = false;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Gate")
	void ToggleGate();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOwnerDeath();

	UFUNCTION()
	void HandleChemicalReaction(FGameplayTag StateTag, float Intensity);
};