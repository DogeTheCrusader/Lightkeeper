// TA LINIJKA MUSI BYĆ PIERWSZA:
#include "MetroidvaniaGateComponent.h"

// Reszta includów:
#include "LightkeeperCharacter.h"
#include "ProgressionComponent.h"
// #include "ReactionReceiverComponent.h" // Odkomentuj, gdy podepniemy chemię

UMetroidvaniaGateComponent::UMetroidvaniaGateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMetroidvaniaGateComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UMetroidvaniaGateComponent::TryUnlock(ALightkeeperCharacter* Instigator, FGameplayTag ToolOrKeyTag)
{
	if (!bIsLocked) return true;
	if (!Instigator) return false;

	if (RequiredPerkTag.IsValid() && Instigator->ProgressionComp)
	{
		if (Instigator->ProgressionComp->HasPerk(RequiredPerkTag))
		{
			UnlockGate(FName("Perk"));
			return true;
		}
	}

	if (RequiredKeyTag.IsValid() && ToolOrKeyTag.IsValid())
	{
		if (ToolOrKeyTag.MatchesTagExact(RequiredKeyTag))
		{
			UnlockGate(FName("Key"));
			return true;
		}
	}

	return false;
}

void UMetroidvaniaGateComponent::UnlockGate(FName MethodName)
{
	if (bIsLocked)
	{
		bIsLocked = false;

		if (MethodName == FName("Key"))
		{
			bKeyDiscovered = true;
		}

		OnGateUnlocked.Broadcast(MethodName);

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			if (MethodName == FName("Perk"))
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, TEXT("🔓 [MISTRZ WŁAMAŃ] Ciche otwarcie zamka perkiem Precyzji (2A)!"));
			}
			else if (MethodName == FName("Key"))
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan, TEXT("🔑 [ZAMEK] Otwarto właściwym kluczem!"));
			}
			else if (MethodName == FName("AcidBypass"))
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Orange, TEXT("🧪 [KWAS] Kłódka została całkowicie stopiona kwasem!"));
			}
		}
#endif
	}
}

void UMetroidvaniaGateComponent::HandleChemicalReaction(FGameplayTag StateTag, float Intensity)
{
	if (!bIsLocked || !bCanBeMeltedByAcid) return;

	if (StateTag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Status.State.Hazard.Corroding")))
	{
		UnlockGate(FName("AcidBypass"));
	}
}