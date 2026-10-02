#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameplayTagContainer.h"
#include "SafeLightComponent.generated.h"

class USanityComponent;
class UReactionReceiverComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API USafeLightComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	USafeLightComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Zmienna tylko do odczytu (stan logiki)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Safe Light")
	bool bIsLightActive = true;

	// ====================================================================
	// ZARZĄDZANIE W EDYTORZE I W ŚWIECIE:
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bStartsLit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bCanBeExtinguishedByWater = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bIgniteWhenOwnerIsBurning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bAutoSyncWithLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light",
		meta = (EditCondition = "!bAutoSyncWithLight", EditConditionHides))
	bool bIsSpotlightCone = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bExtinguishWhenBurningEnds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light",
		meta = (EditCondition = "!bAutoSyncWithLight && bIsSpotlightCone", EditConditionHides))
	float ConeAngle = 45.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Safe Light")
	float ActualLightRadius = 500.0f;

	// ====================================================================
	// INTERAKCJA RĘCZNA (KLAWISZ [E])
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light|Interaction")
	bool bIsToggleableLightSource = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light|Interaction")
	bool bCanBeBlownOut = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light|Interaction")
	bool bRequiresMatchesToIgnite = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light|Interaction", meta = (EditCondition = "bRequiresMatchesToIgnite"))
	FGameplayTag RequiredMatchItemTag;

	// ====================================================================
	// FUNKCJE PUBLICZNE
	// ====================================================================
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void SetLightActive(bool bNewActive);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void SetDynamicRadius(float NewRadius);

	// Tę funkcję wywołuje BaseInteractable, gdy gracz wciska klawisz E:
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void InteractWithLight(AActor* Instigator);

protected:
	virtual void BeginPlay() override;
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleOwnerStateApplied(FGameplayTag StateTag, float Intensity);

	UFUNCTION()
	void HandleOwnerStateRemoved(FGameplayTag StateTag);

private:
	bool bIsPlayerInside = false;
	bool bIsPlayerInCone = false;
	bool bHasContributedLight = false;
	TWeakObjectPtr<USanityComponent> CachedPlayerSanity;

	bool IsPlayerInsideLightCone(AActor* PlayerActor) const;
	void UpdatePlayerLightState();
	void SyncWithParentLight();
};