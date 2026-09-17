#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InteractionComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "LightkeeperCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterLandedEvent, const FHitResult&, Hit, float, FallSpeed);

UCLASS()
class LIGHTKEEPER_API ALightkeeperCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALightkeeperCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Komponenty
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UInteractionComponent* InteractionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UStaminaComponent* StaminaComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UProgressionComponent* ProgressionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class USanityComponent* SanityComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UStatusEffectComponent* StatusComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UInventoryComponent* InventoryComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UToolManagerComponent* ToolManagerComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UUtilityManagerComponent* UtilityManagerComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class ULanternComponent* LanternComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	class UReactionReceiverComponent* ReactionComp;

	UPROPERTY(BlueprintAssignable, Category = "Lightkeeper|Events")
	FOnCharacterLandedEvent OnCharacterLanded;

	UFUNCTION(BlueprintPure, Category = "Lightkeeper|Capabilities")
	bool CanPerformAction(EPlayerAction Action) const
	{
		return HealthComp ? HealthComp->IsActionAllowed(Action) : true;
	}

	// ==========================================================
	// 1. ZMIENNE DLA TWOICH 3 LINIJEK W KONSTRUKTORZE!
	// ==========================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float WalkSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float SprintSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float CrouchSpeed = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float JumpHeight = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float StandingCapsuleHalfHeight = 88.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float CrouchingCapsuleHalfHeight = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Movement")
	float CapsuleRadius = 32.0f;

	// ==========================================================
	// 2. PRZEKAŹNIKI DO KOMPONENTÓW
	// ==========================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Vitals")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Vitals")
	float Sanity = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Camera")
	float CameraSensitivity = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void StartInteraction() { if (InteractionComp) InteractionComp->StartInteraction(); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void StopInteraction() { if (InteractionComp) InteractionComp->StopInteraction(); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void SlamInteraction() { if (InteractionComp) InteractionComp->SlamInteraction(); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ForwardMouseLook(float MouseX, float MouseY);

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Interaction")
	void ZoomHoldSlot(float ScrollDelta) { if (InteractionComp) InteractionComp->ZoomHoldSlot(ScrollDelta); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Movement")
	void StartSprint() { if (StaminaComp) StaminaComp->StartSprint(); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Movement")
	void StopSprint() { if (StaminaComp) StaminaComp->StopSprint(); }

	UFUNCTION(BlueprintCallable, Category = "Lightkeeper|Movement")
	void UpdateMovementSpeed();

	// Opcja w Details: Postać automatycznie zaczyna grę z tym urazem:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightkeeper|Debug")
	FGameplayTag DebugStartInjury;

	// Komendy konsoli (~):
	UFUNCTION(Exec, Category = "Lightkeeper|Debug")
	void Debug_AddInjury(FString InjuryName);

	UFUNCTION(Exec, Category = "Lightkeeper|Debug")
	void Debug_ClearInjuries();

	UFUNCTION(Exec, Category = "Lightkeeper|Debug")
	void Debug_TestLaudanum();

	UFUNCTION(Exec, Category = "Lightkeeper|Debug")
	void Debug_TestBandage();

	void Debug_TestLegs(); void Debug_TestRightArm(); void Debug_TestChest(); void Debug_TestSprain();

protected:
	float LandingRecoveryTimer = 0.0f;
	virtual void BeginPlay() override;
	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;
};