#include "TriggerReceiverComponent.h"
#include "ImSimSensorySubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UTriggerReceiverComponent::UTriggerReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaterialTag = FGameplayTag::RequestGameplayTag(FName("Material.Metal"), false);
}

void UTriggerReceiverComponent::ReceiveTriggerSignal(bool bNewActivated)
{
	// ====================================================================
	// AUTOMATYCZNY HAŁAS URUCHOMIENIA MASZYNY W JEJ WŁASNYM PUNKCIE 3D:
	// ====================================================================
	if (bEmitsActivationNoise && bNewActivated)
	{
		if (UImSimSensorySubsystem* Sensory = GetWorld()->GetSubsystem<UImSimSensorySubsystem>())
		{
			static const FGameplayTag NoiseTag = FGameplayTag::RequestGameplayTag(FName("State.Element.Acoustics.Noise"), false);
			Sensory->RegisterNoise(GetOwner()->GetActorLocation(), ActivationNoiseRadius, NoiseTag, MaterialTag);
		}
	}

	OnTriggerReceived.Broadcast(bNewActivated);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		FString StateStr = bNewActivated ? TEXT("🟢 [ODEBRANO SYGNAŁ: WŁĄCZONO]") : TEXT("🔴 [ODEBRANO SYGNAŁ: WYŁĄCZONO]");
		FColor MsgColor = bNewActivated ? FColor::Green : FColor::Silver;
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, MsgColor,
			FString::Printf(TEXT("%s Urządzenie: %s (Hałas dla AI: %.0f cm)"), *StateStr, *GetOwner()->GetName(), ActivationNoiseRadius));
	}
#endif
}