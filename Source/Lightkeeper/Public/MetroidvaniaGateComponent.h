#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MetroidvaniaGateComponent.generated.h" // To ZAWSZE musi być ostatni include w pliku .h!

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGateUnlocked, FName, UnlockMethod);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UMetroidvaniaGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMetroidvaniaGateComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|State")
	bool bIsLocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|Requirements")
	FGameplayTag RequiredKeyTag;

	// Pamięć zamka (czy gracz zdążył już raz dopasować do niego klucz?):
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gate|State")
	bool bKeyDiscovered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|Requirements")
	FGameplayTag RequiredPerkTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|ImSim Bypass")
	bool bCanBeMeltedByAcid = false;

	UPROPERTY(BlueprintAssignable, Category = "Gate|Events")
	FOnGateUnlocked OnGateUnlocked;

	UFUNCTION(BlueprintCallable, Category = "Gate")
	bool TryUnlock(class ALightkeeperCharacter* Instigator, FGameplayTag ToolOrKeyTag);

	UFUNCTION(BlueprintCallable, Category = "Gate")
	void UnlockGate(FName MethodName);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleChemicalReaction(FGameplayTag StateTag, float Intensity);
};