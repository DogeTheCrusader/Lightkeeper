#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class ECharacterStat : uint8
{
	Vigor		UMETA(DisplayName = "Wigor (Ciało i Siła)"),
	Precision	UMETA(DisplayName = "Precyzja (Zwinność i Stealth)"),
	Engineering	UMETA(DisplayName = "Inżynieria (Maszyny)"),
	Deduction	UMETA(DisplayName = "Dedukcja (Sanity i Śledztwo)"),
	Occultism	UMETA(DisplayName = "Okultyzm (Sny)")
};

USTRUCT(BlueprintType)
struct FStatData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Progression")
	int32 CurrentTier = 0;
};