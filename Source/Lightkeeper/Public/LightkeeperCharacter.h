#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InteractionComponent.h"
#include "StaminaComponent.h"
#include "LightkeeperCharacter.generated.h"

UCLASS()
class LIGHTKEEPER_API ALightkeeperCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALightkeeperCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;

	// Komponenty
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UInteractionComponent* InteractionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lightkeeper|Components")
	UStaminaComponent* StaminaComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USanityComponent* SanityComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStatusEffectComponent* StatusComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UInventoryComponent* InventoryComp;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

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
};