#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "SafeLightComponent.generated.h"

class USanityComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API USafeLightComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	USafeLightComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Czy to Ÿród³o œwiat³a jest zapalone?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bIsLightActive = true;

	// AUTOMATYCZNA SYNCHRONIZACJA: Samodzielnie odczytuje zasiêg i k¹t z ¿arówki/reflektora na tym samym obiekcie!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light")
	bool bAutoSyncWithLight = true;

	// Czy to jest kierunkowy reflektor ze sto¿kiem (SpotLight), czy œwieci dooko³a 360 (PointLight)?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light",
		meta = (EditCondition = "!bAutoSyncWithLight", EditConditionHides))
	bool bIsSpotlightCone = false;

	// K¹t sto¿ka œwiat³a (dla reflektorów):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Safe Light",
		meta = (EditCondition = "!bAutoSyncWithLight && bIsSpotlightCone", EditConditionHides))
	float ConeAngle = 45.0f;

	// W³¹cza lub gasi strefê œwiat³a
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void SetLightActive(bool bNewActive);

	// Dynamicznie zmienia promieñ œwiat³a (dla rosn¹cych/gasn¹cych p³omieni)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Safe Light")
	void SetDynamicRadius(float NewRadius);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	bool bInitialCheckDone = false; // Flaga sprawdzania w klatce 1 po za³adowaniu gracza
	bool bIsPlayerInside = false;
	bool bIsPlayerInCone = false;
	bool bHasContributedLight = false;
	TWeakObjectPtr<USanityComponent> CachedPlayerSanity;

	bool IsPlayerInsideLightCone(AActor* PlayerActor) const;
	void UpdatePlayerLightState();
	void SyncWithParentLight();
};