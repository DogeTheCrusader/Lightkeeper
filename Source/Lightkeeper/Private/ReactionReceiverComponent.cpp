#include "ReactionReceiverComponent.h"

UReactionReceiverComponent::UReactionReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UReactionReceiverComponent::ApplyStateImpact(FGameplayTag StateTag, float Intensity)
{
	// Sprawdzamy, czy ten obiekt reaguje na ten Stan (np. czy reaguje na Ogień)
	if (VulnerableStates.HasTag(StateTag))
	{
		// Dodajemy ten Stan do aktywnych
		ActiveStates.AddTag(StateTag);

		// Wysyłamy informację do Blueprinta obiektu! (np. żeby włączył cząsteczki ognia i zabrał HP)
		OnStateApplied.Broadcast(StateTag, Intensity);
	}
}

void UReactionReceiverComponent::RemoveState(FGameplayTag StateTag)
{
	if (ActiveStates.HasTag(StateTag))
	{
		ActiveStates.RemoveTag(StateTag);
	}
}