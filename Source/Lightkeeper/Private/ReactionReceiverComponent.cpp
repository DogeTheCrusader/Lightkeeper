#include "ReactionReceiverComponent.h"

UReactionReceiverComponent::UReactionReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UReactionReceiverComponent::ApplyStateImpact(FGameplayTag StateTag, float Intensity)
{
	// ====================================================================
	// SPRAWDZANIE HIERARCHII: Czy otrzymany tag pasuje do RODZICA w VulnerableStates?
	// ====================================================================
	bool bIsVulnerable = false;
	for (const FGameplayTag& VulnTag : VulnerableStates)
	{
		if (StateTag.MatchesTag(VulnTag)) // Jeśli StateTag to dziecko VulnTag -> TRUE!
		{
			bIsVulnerable = true;
			break;
		}
	}

	if (bIsVulnerable)
	{
		ActiveStates.AddTag(StateTag);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
				FString::Printf(TEXT("[ŻYWIOŁ] %s OTRZYMAŁ STAN: %s (Moc: %.1f)"),
					*GetOwner()->GetName(), *StateTag.ToString(), Intensity));
		}

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