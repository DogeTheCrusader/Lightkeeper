#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProgressionTypes.h"
#include "GameplayTagContainer.h"
#include "ProgressionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressionComponent();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Progression")
	TMap<ECharacterStat, FStatData> Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Progression")
	FGameplayTagContainer UnlockedPerks;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression")
	int32 GetStatTier(ECharacterStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Progression")
	bool HasPerk(FGameplayTag PerkTag) const;

	// Inicjuje testowe Perki dla Dema (Wigor 1A, Precyzja 2A)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Debug")
	void Debug_UnlockDemoPerks();

protected:
	virtual void BeginPlay() override;
};