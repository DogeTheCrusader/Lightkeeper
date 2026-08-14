#include "StatusEffectComponent.h"

UStatusEffectComponent::UStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStatusEffectComponent::AddStatusEffect(FGameplayTag StatusTag)
{
	if (!StatusTag.IsValid() || ActiveStatusTags.HasTag(StatusTag)) return;

	ActiveStatusTags.AddTag(StatusTag);
	OnStatusEffectAdded.Broadcast(StatusTag);
}

void UStatusEffectComponent::RemoveStatusEffect(FGameplayTag StatusTag)
{
	if (!StatusTag.IsValid() || !ActiveStatusTags.HasTag(StatusTag)) return;

	ActiveStatusTags.RemoveTag(StatusTag);
	OnStatusEffectRemoved.Broadcast(StatusTag);
}

void UStatusEffectComponent::ClearAllStatusEffects()
{
	ActiveStatusTags.Reset();
}