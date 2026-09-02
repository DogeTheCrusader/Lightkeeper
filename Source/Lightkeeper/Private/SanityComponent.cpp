#include "SanityComponent.h"
#include "SafeLightComponent.h"
#include "LightkeeperCharacter.h"
#include "LanternComponent.h"
#include "SensoryComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h" 

USanityComponent::USanityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USanityComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentSanity = GetMaxSanity();
	LowestSanityPercentInDarkness = 1.0f;
	MentalCollapseCount = 0;
	ActiveLightSourcesCount = 0;
	SanctuaryZonesCount = 0;
	bIsGracePeriodActive = false;
	bIsAdrenalineActive = false;
	bIsInDarkness = true;
	bIsLookingAtMonster = false;
}

void USanityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bIsPhysicallyIlluminated = false;

	// 1. BEZPIECZNY POKÓJ (Sanctuary):
	if (IsInSanctuary())
	{
		bIsPhysicallyIlluminated = true;
	}

	// 2. LATARNIA GRACZA:
	if (!bIsPhysicallyIlluminated)
	{
		if (ALightkeeperCharacter* Player = Cast<ALightkeeperCharacter>(GetOwner()))
		{
			if (Player->LanternComp && Player->LanternComp->bIsLit)
			{
				bIsPhysicallyIlluminated = true;
			}
		}
	}

	// 3. ŚWIATŁA ZEWNĘTRZNE (LineTrace):
	if (!bIsPhysicallyIlluminated)
	{
		bIsPhysicallyIlluminated = CheckLightLineOfSight();
	}

	bIsInDarkness = !bIsPhysicallyIlluminated;

	// ====================================================================
	// 4. KOSZMAR SPOJRZENIA (Gaze Dread - działa ZAWSZE, nawet w świetle!):
	// ====================================================================
	EvaluateMonsterGazeDread(DeltaTime);

	// ====================================================================
	// 5. OBSŁUGA MROKU I ŚWIATŁA:
	// ====================================================================
	if (bIsInDarkness)
	{
		TimeInDarkness += DeltaTime;

		float LightProximityPenalty = 1.0f;
		if (OverlappingLightSources.Num() > 0)
		{
			float HighestAlpha = 0.0f;
			for (USafeLightComponent* Light : OverlappingLightSources)
			{
				if (Light && Light->bIsLightActive)
				{
					float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Light->GetComponentLocation());
					float Alpha = 1.0f - FMath::Clamp(Dist / FMath::Max(1.0f, Light->GetScaledSphereRadius()), 0.0f, 1.0f);
					HighestAlpha = FMath::Max(HighestAlpha, Alpha);
				}
			}
			LightProximityPenalty = FMath::Clamp(1.0f - HighestAlpha, 0.15f, 1.0f);
		}

		float CurrentDrain = (BaseDarknessDrainRate * LightProximityPenalty) * (1.0f + (TimeInDarkness * DarknessAccelerationFactor));
		if (bHasMinorMadness) CurrentDrain *= MinorMadnessDrainMultiplier;

		float MinAllowedSanity = bIsGracePeriodActive ? (BaseMaxSanity * 0.25f) : 0.0f;

		CurrentSanity = FMath::Clamp(CurrentSanity - (CurrentDrain * DeltaTime), MinAllowedSanity, GetMaxSanity());
		OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());

		if (!bIsGracePeriodActive)
		{
			float CurrentSanityPercent = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
			LowestSanityPercentInDarkness = FMath::Min(LowestSanityPercentInDarkness, CurrentSanityPercent);
		}

		if (!bHasMinorMadness && (CurrentSanity / FMath::Max(1.0f, GetMaxSanity())) < 0.40f && FMath::FRandRange(0.0f, 100.0f) < 2.0f)
		{
			TriggerRandomMinorMadness();
		}

		if (CurrentSanity <= 0.0f && !bIsGracePeriodActive)
		{
			HandleSanityDepleted();
		}
	}
	else
	{
		TimeInDarkness = 0.0f;

		float DynamicCap = GetCurrentDynamicComfortCap();
		if (CurrentSanity < DynamicCap)
		{
			float SanityRatio = FMath::Clamp(CurrentSanity / FMath::Max(1.0f, GetMaxSanity()), 0.20f, 1.0f);
			float BaseRate = bIsAdrenalineActive ? AdrenalineRecoveryRate : BaseLightRecoveryRate;
			float EffectiveRecoverySpeed = BaseRate * SanityRatio;

			CurrentSanity = FMath::FInterpConstantTo(CurrentSanity, DynamicCap, DeltaTime, EffectiveRecoverySpeed);
			OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
		}
	}
}

void USanityComponent::EvaluateMonsterGazeDread(float DeltaTime)
{
	APawn* PlayerPawn = Cast<APawn>(GetOwner());
	if (!PlayerPawn) return;

	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CamMgr) return;

	FVector CameraLocation = CamMgr->GetCameraLocation();
	FVector CameraForward = CamMgr->GetCameraRotation().Vector();
	FVector TraceEnd = CameraLocation + (CameraForward * 2000.0f); // 20 metrów z celownika

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PlayerPawn);

	if (UInteractionComponent* InterComp = PlayerPawn->FindComponentByClass<UInteractionComponent>())
	{
		if (InterComp->GrabbedActor)
		{
			Params.AddIgnoredActor(InterComp->GrabbedActor);
		}
	}

	bIsLookingAtMonster = false;
	FHitResult Hit;

	// Sferyczny promień spojrzenia (grubość 25 cm) zapobiegający mruganiu:
	FCollisionShape EyeSphere = FCollisionShape::MakeSphere(25.0f);

	bool bHit = GetWorld()->SweepSingleByChannel(Hit, CameraLocation, TraceEnd, FQuat::Identity, ECC_Visibility, EyeSphere, Params);

	if (!bHit)
	{
		bHit = GetWorld()->SweepSingleByChannel(Hit, CameraLocation, TraceEnd, FQuat::Identity, ECC_Pawn, EyeSphere, Params);
	}

	if (bHit && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();

		if (USensoryComponent* MonsterSensory = HitActor->FindComponentByClass<USensoryComponent>())
		{
			if (MonsterSensory->SensoryProfile.bCausesGazeDread)
			{
				float DrainRate = MonsterSensory->SensoryProfile.GazeDreadDrainRate;
				if (DrainRate <= 0.0f) DrainRate = 4.0f;

				bIsLookingAtMonster = true;
				float GazeDamage = DrainRate * DeltaTime;
				TakeSanityDamage(GazeDamage);

#if !UE_BUILD_SHIPPING
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage((uint64)HitActor->GetUniqueID() + 500, 0.2f, FColor::Purple,
						FString::Printf(TEXT("👁️ [GROZA SPOJRZENIA] Wpatrujesz się w [%s]! -%.1f Sanity/s"), *HitActor->GetName(), DrainRate));
				}
#endif
			}
		}
	}
}

void USanityComponent::AddLightSource()
{
	ActiveLightSourcesCount++;
}

void USanityComponent::RemoveLightSource()
{
	ActiveLightSourcesCount = FMath::Max(0, ActiveLightSourcesCount - 1);
}

void USanityComponent::HandleSanityDepleted()
{
	if (MentalCollapseCount >= 3) return;

	MentalCollapseCount++;
	bIsGracePeriodActive = true;
	bIsAdrenalineActive = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GracePeriodTimerHandle, this, &USanityComponent::EndGracePeriod, GracePeriodDuration, false);
		World->GetTimerManager().SetTimer(AdrenalineTimerHandle, this, &USanityComponent::EndAdrenalineSurge, AdrenalineDuration, false);
	}

	if (MentalCollapseCount == 1)
	{
		MaxSanityCapMultiplier = 0.75f;
		LowestSanityPercentInDarkness = 1.0f;
		CurrentSanity = BaseMaxSanity * 0.25f;
		TriggerMajorBoutOfMadness();
		OnPsychologicalCollapse.Broadcast(1);
	}
	else if (MentalCollapseCount == 2)
	{
		MaxSanityCapMultiplier = 0.50f;
		LowestSanityPercentInDarkness = 1.0f;
		CurrentSanity = BaseMaxSanity * 0.25f;
		TriggerMajorBoutOfMadness();
		OnPsychologicalCollapse.Broadcast(2);
	}
	else if (MentalCollapseCount >= 3)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[KONIEC NOCY] Ostateczne Załamanie Psychiczne! Przebudzenie w Hubie rano."));
#endif
		OnTotalMentalBreakdown.Broadcast();
	}

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
}

void USanityComponent::EndGracePeriod()
{
	bIsGracePeriodActive = false;

	if (bIsInDarkness && CurrentSanity <= 0.0f)
	{
		HandleSanityDepleted();
	}
}

void USanityComponent::EndAdrenalineSurge()
{
	bIsAdrenalineActive = false;
}

float USanityComponent::GetCurrentDynamicComfortCap() const
{
	float CapPercent = FMath::Clamp(LowestSanityPercentInDarkness + PassiveRecoveryWindowPercent, 0.35f, 1.0f);
	return GetMaxSanity() * CapPercent;
}

void USanityComponent::TakeSanityDamage(float DamageAmount, FGameplayTag ShockTag)
{
	if (DamageAmount <= 0.0f) return;

	float FinalDamage = bHasMinorMadness ? (DamageAmount * MinorMadnessDrainMultiplier) : DamageAmount;
	CurrentSanity = FMath::Clamp(CurrentSanity - FinalDamage, 0.0f, GetMaxSanity());

	if (!bIsGracePeriodActive)
	{
		float CurrentSanityPercent = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
		LowestSanityPercentInDarkness = FMath::Min(LowestSanityPercentInDarkness, CurrentSanityPercent);
	}

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());

	if (CurrentSanity <= 0.0f && !bIsGracePeriodActive)
	{
		HandleSanityDepleted();
	}
}

void USanityComponent::RestoreSanity(float RestoreAmount)
{
	if (RestoreAmount <= 0.0f) return;

	CurrentSanity = FMath::Clamp(CurrentSanity + RestoreAmount, 0.0f, GetMaxSanity());
	LowestSanityPercentInDarkness = CurrentSanity / FMath::Max(1.0f, GetMaxSanity());
	bHasMinorMadness = false;

	OnSanityChanged.Broadcast(CurrentSanity, GetMaxSanity());
}

void USanityComponent::TriggerRandomMinorMadness()
{
	bHasMinorMadness = true;
	FGameplayTag MinorTag = FGameplayTag::RequestGameplayTag(FName("Status.Madness.Minor.Whispers"), false);
	OnMinorMadnessTriggered.Broadcast(MinorTag);
}

void USanityComponent::TriggerMajorBoutOfMadness()
{
	FGameplayTag BoutTag = FGameplayTag::RequestGameplayTag(FName("Status.Madness.Major.Scotophobia"), false);
	ActiveMadnessTags.AddTag(BoutTag);
	OnBoutOfMadnessTriggered.Broadcast(BoutTag);
}

void USanityComponent::RegisterPotentialLight(USafeLightComponent* LightComp)
{
	if (LightComp && !OverlappingLightSources.Contains(LightComp))
	{
		OverlappingLightSources.Add(LightComp);
	}
}

void USanityComponent::UnregisterPotentialLight(USafeLightComponent* LightComp)
{
	if (LightComp)
	{
		OverlappingLightSources.Remove(LightComp);
	}
}

bool USanityComponent::CheckLightLineOfSight()
{
	if (OverlappingLightSources.Num() == 0) return false;

	AActor* Player = GetOwner();
	if (!Player) return false;

	FVector PlayerLocation = Player->GetActorLocation();
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Player);

	for (USafeLightComponent* Light : OverlappingLightSources)
	{
		if (!Light || !Light->bIsLightActive) continue;

		FVector LightLocation = Light->GetComponentLocation();
		FHitResult Hit;

		bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			PlayerLocation,
			LightLocation,
			ECC_Visibility,
			TraceParams
		);

		if (!bHit || Hit.GetActor() == Light->GetOwner())
		{
			return true;
		}
	}

	return false;
}