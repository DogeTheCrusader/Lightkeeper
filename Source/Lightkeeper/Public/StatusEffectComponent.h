#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "StatusEffectComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusEffectAdded, FGameplayTag, StatusTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusEffectRemoved, FGameplayTag, StatusTag);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatusEffectComponent();

	// Wszystkie aktywne Urazy Ciała i Bouts of Madness, które gracz obecnie posiada
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Status Effects")
	FGameplayTagContainer ActiveStatusTags;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Status Effects")
	FOnStatusEffectAdded OnStatusEffectAdded;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Status Effects")
	FOnStatusEffectRemoved OnStatusEffectRemoved;

	// Nakłada Uraz lub Szaleństwo (np. Status.Injury.Major.Legs)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void AddStatusEffect(FGameplayTag StatusTag);

	// Leczy Uraz (np. po użyciu bandaża lub laudanum)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void RemoveStatusEffect(FGameplayTag StatusTag);

	// Sprawdza czy gracz ma dany uraz (np. czy ma złamaną nogę)
	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Status Effects")
	bool HasStatusEffect(FGameplayTag StatusTag) const { return ActiveStatusTags.HasTag(StatusTag); }

	// Czyści wszystkie urazy (np. po przespaniu nocy w łóżku w Hubie)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Status Effects")
	void ClearAllStatusEffects();
};