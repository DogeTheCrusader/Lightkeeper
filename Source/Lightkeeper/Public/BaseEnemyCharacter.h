#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AISensoryTypes.h"
#include "BaseEnemyCharacter.generated.h"

class USensoryComponent;
class UHealthComponent;
class UReactionReceiverComponent;
class UStatusEffectComponent;

UCLASS(Blueprintable)
class LIGHTKEEPER_API ABaseEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseEnemyCharacter();

	virtual void Tick(float DeltaTime) override;

	// ==========================================================
	// KOMPONENTY POTWORA (Dostępne publicznie dla systemów)
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	USensoryComponent* SensoryComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UReactionReceiverComponent* ReactionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UStatusEffectComponent* StatusComp;

	// Prędkości poruszania się w różnych stanach:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float PatrolSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float ChaseSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float FleeSpeed = 550.0f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void HandleStateChanged(EAIBehaviorState NewState, FVector TargetLocation);

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	virtual void HandleDamageTaken(float CurrentHP, float MaxHP);

	// Walka
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Combat")
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Combat")
	float AttackCooldown = 1.4f;

	FTimerHandle AttackTimerHandle;
	bool bCanAttack = true;

	// Zegary dla patrolu i badania terenu:
	FTimerHandle PatrolTimerHandle;
	FTimerHandle InvestigateTimerHandle;

	// Funkcje zachowania:
	void PerformRandomPatrol();
	void FinishInvestigation();
	void PerformMeleeAttackOnPlayer();
	void ResetAttackCooldown();
};