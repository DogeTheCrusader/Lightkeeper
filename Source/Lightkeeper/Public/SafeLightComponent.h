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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bIsLightActive = true;

	// Czy to światło ma się automatycznie zapalać, gdy obiekt płonie, i gasnąć, gdy ogień zgaśnie?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bIgniteWhenOwnerIsBurning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bAutoSyncWithLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light",
		meta = (EditCondition = "!bAutoSyncWithLight", EditConditionHides))
	bool bIsSpotlightCone = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light",
		meta = (EditCondition = "!bAutoSyncWithLight && bIsSpotlightCone", EditConditionHides))
	float ConeAngle = 45.0f;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void SetLightActive(bool bNewActive);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void SetDynamicRadius(float NewRadius);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Autonomiczne nasłuchiwanie chemii:
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