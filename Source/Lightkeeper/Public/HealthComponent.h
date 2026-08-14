#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInjuryTriggered, FGameplayTag, InjuryTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LIGHTKEEPER_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	// Maksymalne HP bazowe (np. 100)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health")
	float BaseMaxHealth = 100.0f;

	// Limit HP po Omdleniach (1.0 = 100%, 0.75 = 75%, 0.50 = 50%)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Health")
	float MaxHealthCapMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Health")
	float CurrentHealth = 100.0f;

	// ==========================================================
	// PASEK PĘKNIĘCIA (FRACTURE METER - System Szansy Urazu)
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Health")
	float FractureMeter = 0.0f; // Szansa na uraz (0.0 - 1.0, gdzie 0.05 = +5% po ciosie)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Health")
	float FractureIncreasePerHit = 0.05f; // +5% przy każdym otrzymanym ciosie

	// ==========================================================
	// DELEGATY (EVENTY)
	// ==========================================================
	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Health")
	FOnInjuryTriggered OnInjuryTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Health")
	FOnDeath OnDeath;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Health")
	void TakeDamage(float DamageAmount, FGameplayTag DamageTypeTag);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Health")
	void Heal(float HealAmount);

	// Aplikuje karę po Omdleniu (np. 0.75 dla 75% max HP)
	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Health")
	void ApplyFaintCap(float CapMultiplier);

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Health")
	float GetMaxHealth() const { return BaseMaxHealth * MaxHealthCapMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Health")
	bool IsDead() const { return CurrentHealth <= 0.0f; }

protected:
	virtual void BeginPlay() override;

private:
	void TestFractureInjury(FGameplayTag DamageTypeTag);
};