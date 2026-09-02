#include "WeightPlateComponent.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"

UWeightPlateComponent::UWeightPlateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UPrimitiveComponent* UWeightPlateComponent::GetActivePrimitive() const
{
	if (CustomDetectionVolume) return CustomDetectionVolume;
	if (AActor* Owner = GetOwner())
	{
		return Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	}
	return nullptr;
}

void UWeightPlateComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPrimitiveComponent* Prim = GetActivePrimitive())
	{
		Prim->SetGenerateOverlapEvents(true);
		Prim->OnComponentBeginOverlap.AddDynamic(this, &UWeightPlateComponent::HandleBeginOverlap);
		Prim->OnComponentEndOverlap.AddDynamic(this, &UWeightPlateComponent::HandleEndOverlap);
	}

	RecalculateTotalWeight();
}

void UWeightPlateComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	RecalculateTotalWeight();
}

void UWeightPlateComponent::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	RecalculateTotalWeight();
}

void UWeightPlateComponent::RecalculateTotalWeight()
{
	UPrimitiveComponent* Prim = GetActivePrimitive();
	if (!Prim) return;

	TArray<UPrimitiveComponent*> OverlappingComps;
	Prim->GetOverlappingComponents(OverlappingComps);

	float TotalMass = 0.0f;
	TSet<AActor*> ProcessedActors;

	for (UPrimitiveComponent* Comp : OverlappingComps)
	{
		if (!Comp) continue;

		AActor* CompOwner = Comp->GetOwner();
		if (!CompOwner || ProcessedActors.Contains(CompOwner) || CompOwner == GetOwner()) continue;
		ProcessedActors.Add(CompOwner);

		// 1. Gracz / Postać:
		if (CompOwner->IsA<ACharacter>())
		{
			TotalMass += PlayerMassKg;
		}
		// 2. Fizyczny prop (skrzynia, kowadło, barykada):
		else if (Comp->IsSimulatingPhysics())
		{
			TotalMass += Comp->GetMass();
		}
	}

	CurrentWeightKg = TotalMass;
	float Ratio = FMath::Clamp(CurrentWeightKg / FMath::Max(1.0f, TargetWeightThresholdKg), 0.0f, 1.0f);

	OnWeightUpdated.Broadcast(CurrentWeightKg, Ratio);

	bool bNewThresholdState = (CurrentWeightKg >= TargetWeightThresholdKg);

	if (bNewThresholdState != bIsThresholdReached)
	{
		bIsThresholdReached = bNewThresholdState;
		OnWeightThresholdChanged.Broadcast(bIsThresholdReached);

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			FString StateStr = bIsThresholdReached ? TEXT("⚖️ [WAGA AKTYWNA]") : TEXT("⚖️ [WAGA ZWOLNIONA]");
			FColor MsgColor = bIsThresholdReached ? FColor::Green : FColor::Orange;
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, MsgColor,
				FString::Printf(TEXT("%s Ciężar: %.1f / %.1f kg (%.0f%%)"), *StateStr, CurrentWeightKg, TargetWeightThresholdKg, Ratio * 100.0f));
		}
#endif
	}
}