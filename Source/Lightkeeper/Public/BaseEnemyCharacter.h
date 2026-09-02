#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AISensoryTypes.h"
#include "InventoryTypes.h"
#include "BaseEnemyCharacter.generated.h"

class USensoryComponent;
class UHealthComponent;
class UReactionReceiverComponent;
class UStatusEffectComponent;
class ALightkeeperAIController;

UENUM(BlueprintType)
enum class EEnemyPatrolMode : uint8
{
	Stationary_Guard UMETA(DisplayName = "Stój w Miejscu (Strażnik)"),
	Waypoint_Route   UMETA(DisplayName = "Trasa po Punktach (Waypoints)"),
	Free_Roam        UMETA(DisplayName = "Swobodne Krążenie (Free Roam)")
};

USTRUCT(BlueprintType)
struct FEnemyMeleeAttackProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Damage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ToolTip = "Obrażenia psychiczne zadawane przy trafieniu w gracza."))
	float SanityDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackReach = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRadius = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	EToolTraceShape TraceShape = EToolTraceShape::SphereSweep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FGameplayTag DamageTypeTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Slashing"), false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackCooldown = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float StunDurationOnHit = 0.0f;
};

UCLASS(Blueprintable)
class LIGHTKEEPER_API ABaseEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseEnemyCharacter();

	virtual void Tick(float DeltaTime) override;

	// ==========================================================
	// KOMPONENTY
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	USensoryComponent* SensoryComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UReactionReceiverComponent* ReactionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UStatusEffectComponent* StatusComp;

	// ==========================================================
	// TRYBY PATROLU I RUCHU
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Patrol Setup")
	EEnemyPatrolMode PatrolMode = EEnemyPatrolMode::Free_Roam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Patrol Setup", meta = (EditCondition = "PatrolMode == EEnemyPatrolMode::Waypoint_Route"))
	TArray<AActor*> PatrolWaypoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Patrol Setup", meta = (EditCondition = "PatrolMode == EEnemyPatrolMode::Waypoint_Route"))
	bool bPatrolPingPong = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Patrol Setup")
	FVector HomeGuardLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Patrol Setup")
	FRotator HomeGuardRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float PatrolSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float ChaseSpeed = 460.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float FleeSpeed = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Combat")
	FEnemyMeleeAttackProfile MeleeAttackProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Combat")
	TSubclassOf<class ABaseTool> EquippedToolClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Combat")
	class ABaseTool* EquippedToolInstance;

	// ==========================================================
	// WZBURZENIE I REAKCJA NA ATAK Z UKRYCIA
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	bool bIsAgitated = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	int32 StealthHitStreak = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	FVector LastDamageOriginLocation = FVector::ZeroVector;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|AI")
	void TriggerAgitation(FVector SuspectedOriginLocation);

	// ==========================================================
	// PŁYNNY OBRÓT I ANTY-BAITING
	// ==========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	float DesiredTargetYaw = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	bool bIsInterpolatingRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float RotationInterpSpeed = 7.0f; // Płynność obrotu torsu

	// Licznik kolejnych hałasów odciągających (Baiting Streak):
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	int32 ConsecutiveNoiseDistractions = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|AI")
	float LastDistractionTimestamp = 0.0f;

	void SetDesiredLookAtLocation(const FVector& LookAtTarget);

	// ==========================================================
	// DEBUG VISUALIZATION SWITCH
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Debug")
	bool bShowAIDebugGizmos = true;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void HandleStateChanged(EAIBehaviorState NewState, FVector TargetLocation);

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	virtual void HandleDamageTaken(float CurrentHP, float MaxHP);

	virtual void ExecuteMeleeTraceAttack();
	void ResetAttackCooldown();

	FTimerHandle AttackTimerHandle;
	FTimerHandle InvestigateTimerHandle;
	bool bCanAttack = true;

	void FinishInvestigation();
	void DrawAIDebugGizmos();

	FTimerHandle AgitationTimerHandle;
	void EndAgitation();
};