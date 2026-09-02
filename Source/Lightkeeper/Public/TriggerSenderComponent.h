#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TriggerSenderComponent.generated.h"

// Struktura połączonej zębatki (np. mała zębatka kręci się x-2.0 w lewo):
USTRUCT(BlueprintType)
struct FLinkedGearData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	TObjectPtr<class UMeshComponent> GearMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	float GearRatio = 1.0f; // 1.0 = zgodnie z ruchem, -2.0 = 2x szybciej w lewo!

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	FRotator RotationAxis = FRotator(0.0f, 0.0f, 1.0f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMechanismStateChanged, bool, bIsActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartInserted);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UTriggerSenderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTriggerSenderComponent();

	// ==========================================================
	// 1. GNIAZDO BRAKUJĄCEJ CZĘŚCI (ZĘBATKA / ZAWÓR / BEZPIECZNIK)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Socket")
	bool bRequiresMissingPart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Socket", meta = (EditCondition = "bRequiresMissingPart"))
	FGameplayTag RequiredPartTag; // Np. Item.Part.BrassGear

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Socket", meta = (EditCondition = "bRequiresMissingPart"))
	FName SocketAttachName = FName("PartSocket");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Socket")
	bool bIsPartInserted = false;

	// ==========================================================
	// 2. SPRZĘŻENIE ZĘBATE (MECHANICAL COUPLING)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Gears")
	TArray<FLinkedGearData> LinkedGears;

	// ==========================================================
	// 3. STAN AKTYWACJI MECHANIZMU
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Trigger")
	bool bIsActivated = false;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Trigger")
	FOnMechanismStateChanged OnMechanismStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Trigger")
	FOnPartInserted OnPartInserted;

	// Metoda wkładania części (z dłoni lub plecaka):
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Socket")
	bool TryInsertPart(AActor* InstigatorActor, FGameplayTag ItemTag, AActor* PhysicalPropActor = nullptr);

	// Obracanie zębatek z delty myszy gracza:
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Gears")
	void ProcessMechanicalDelta(float AxisDelta);

	// Przełączenie stanu (np. wajcha osiągnęła 90 stopni):
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Trigger")
	void SetMechanismActivated(bool bNewActivated);

protected:
	virtual void BeginPlay() override;
};