#include "BaseEnemyCharacter.h"
#include "LightkeeperAIController.h"
#include "SensoryComponent.h"
#include "HealthComponent.h"
#include "ReactionReceiverComponent.h"
#include "StatusEffectComponent.h"
#include "SanityComponent.h"
#include "BaseTool.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ABaseEnemyCharacter::ABaseEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SensoryComp = CreateDefaultSubobject<USensoryComponent>(TEXT("SensoryComponent"));
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	ReactionComp = CreateDefaultSubobject<UReactionReceiverComponent>(TEXT("ReactionReceiverComponent"));
	StatusComp = CreateDefaultSubobject<UStatusEffectComponent>(TEXT("StatusEffectComponent"));

	AIControllerClass = ALightkeeperAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (GetMesh())
	{
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		GetMesh()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		GetMesh()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->PushForceFactor = 0.0f;
	GetCharacterMovement()->InitialPushForceFactor = 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceWeight = 0.5f;

	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 320.0f, 0.0f);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
}

void ABaseEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	HomeGuardLocation = GetActorLocation();
	HomeGuardRotation = GetActorRotation();

	if (SensoryComp)
	{
		SensoryComp->OnAIStateChanged.AddDynamic(this, &ABaseEnemyCharacter::HandleStateChanged);
	}

	if (HealthComp)
	{
		HealthComp->OnDeath.AddDynamic(this, &ABaseEnemyCharacter::HandleDeath);
		HealthComp->OnHealthChanged.AddDynamic(this, &ABaseEnemyCharacter::HandleDamageTaken);
	}

	if (EquippedToolClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		EquippedToolInstance = GetWorld()->SpawnActor<ABaseTool>(EquippedToolClass, GetActorTransform(), SpawnParams);
		if (EquippedToolInstance)
		{
			EquippedToolInstance->EquipTool(this);
		}
	}

	if (ALightkeeperAIController* AICon = Cast<ALightkeeperAIController>(GetController()))
	{
		AICon->StartPatrol();
	}
}

void ABaseEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (StatusComp && StatusComp->IsStunned())
	{
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			AICon->StopMovement();
		}
		return;
	}

	// 1. PŁYNNA INTERPOLACJA OBRÓTU W MIEJSCU (Gdy stoi / nasłuchuje):
	if (bIsInterpolatingRotation && SensoryComp && SensoryComp->CurrentState != EAIBehaviorState::Chase_Attack)
	{
		FRotator CurrentRot = GetActorRotation();
		FRotator TargetRot = FRotator(0.0f, DesiredTargetYaw, 0.0f);
		FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, RotationInterpSpeed);
		SetActorRotation(NewRot);

		// Gdy zbliży się na 2 stopnie -> kończymy interpolację:
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, DesiredTargetYaw)) < 2.0f)
		{
			bIsInterpolatingRotation = false;
		}
	}

	// 2. POŚCIG I ATAK:
	if (SensoryComp && SensoryComp->CurrentState == EAIBehaviorState::Chase_Attack)
	{
		if (ALightkeeperAIController* AICon = Cast<ALightkeeperAIController>(GetController()))
		{
			if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				FVector LookDir = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				FRotator TargetRot = FRotator(0.0f, LookDir.Rotation().Yaw, 0.0f);
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 10.0f));

				float DistToPlayer = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());

				if (DistToPlayer <= (MeleeAttackProfile.AttackReach + 30.0f))
				{
					if (bCanAttack)
					{
						ExecuteMeleeTraceAttack();
					}
					else
					{
						AICon->ChaseTargetActor(PlayerPawn, MeleeAttackProfile.AttackReach * 0.4f);
					}
				}
				else
				{
					AICon->ChaseTargetActor(PlayerPawn, MeleeAttackProfile.AttackReach * 0.4f);
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (bShowAIDebugGizmos)
	{
		DrawAIDebugGizmos();
	}
#endif
}

void ABaseEnemyCharacter::SetDesiredLookAtLocation(const FVector& LookAtTarget)
{
	FVector Dir = (LookAtTarget - GetActorLocation()).GetSafeNormal();
	if (!Dir.IsNearlyZero())
	{
		DesiredTargetYaw = Dir.Rotation().Yaw;
		bIsInterpolatingRotation = true;
	}
}

void ABaseEnemyCharacter::HandleStateChanged(EAIBehaviorState NewState, FVector TargetLocation)
{
	if (StatusComp && StatusComp->IsStunned()) return;

	ALightkeeperAIController* AICon = Cast<ALightkeeperAIController>(GetController());
	UWorld* World = GetWorld();
	if (!World || !AICon) return;

	switch (NewState)
	{
	case EAIBehaviorState::Idle_Patrol:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		bIsInterpolatingRotation = false;
		AICon->StartPatrol();
		break;

	case EAIBehaviorState::Suspicious:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed * 0.4f;
		AICon->StopMovement();
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);

		// PŁYNNY OBRÓT W STRONĘ DŹWIĘKU:
		if (FVector::DistSquared(TargetLocation, FVector::ZeroVector) > 10.0f)
		{
			SetDesiredLookAtLocation(TargetLocation);
		}

		World->GetTimerManager().SetTimer(InvestigateTimerHandle, [this, TargetLocation]()
			{
				if (SensoryComp && SensoryComp->CurrentState == EAIBehaviorState::Suspicious)
				{
					SensoryComp->SetAIState(EAIBehaviorState::Cautious, TargetLocation);
				}
			}, 1.5f, false);
		break;

	case EAIBehaviorState::Cautious:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed * 1.15f;
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		bIsInterpolatingRotation = false;

		if (FVector::DistSquared(TargetLocation, FVector::ZeroVector) > 10.0f)
		{
			AICon->SearchAreaAroundLocation(TargetLocation, 900.0f);
		}
		else
		{
			AICon->StartPatrol();
		}

		World->GetTimerManager().SetTimer(InvestigateTimerHandle, this, &ABaseEnemyCharacter::FinishInvestigation, 9.0f, false);
		break;

	case EAIBehaviorState::Investigate:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed * 1.45f;
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		bIsInterpolatingRotation = false;

		// Potwór po prostu idzie do punktu dźwięku/puszki (TargetLocation):
		AICon->MoveToInvestigationPoint(TargetLocation, 60.0f);

		// Po 5 sekundach badania przechodzi w czujny marsz (Cautious):
		World->GetTimerManager().SetTimer(InvestigateTimerHandle, [this, TargetLocation]()
			{
				if (SensoryComp && SensoryComp->CurrentState == EAIBehaviorState::Investigate)
				{
					SensoryComp->SetAIState(EAIBehaviorState::Cautious, TargetLocation);
				}
			}, 5.0f, false);
		break;

	case EAIBehaviorState::Chase_Attack:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		bIsInterpolatingRotation = false;
		ConsecutiveNoiseDistractions = 0; // Reset po rozpoczęciu walki
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			AICon->ChaseTargetActor(PlayerPawn, MeleeAttackProfile.AttackReach * 0.6f);
		}
		break;

	case EAIBehaviorState::Flee_Panic:
		GetCharacterMovement()->MaxWalkSpeed = FleeSpeed;
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		bIsInterpolatingRotation = false;
		AICon->FleeAwayFromThreat(TargetLocation);
		break;
	}
}

void ABaseEnemyCharacter::ExecuteMeleeTraceAttack()
{
	if (!bCanAttack || (SensoryComp && SensoryComp->SensoryProfile.bIsPacifist)) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	bCanAttack = false;

	FVector MonsterLoc = GetActorLocation();
	FVector PlayerLoc = PlayerPawn->GetActorLocation();

	FVector AttackDirection = (PlayerLoc - MonsterLoc).GetSafeNormal();
	FRotator TargetRot = FRotator(0.0f, AttackDirection.Rotation().Yaw, 0.0f);
	SetActorRotation(TargetRot);

	float DistToPlayer = FVector::Dist(MonsterLoc, PlayerLoc);
	float MaxReach = MeleeAttackProfile.AttackReach > 0.0f ? MeleeAttackProfile.AttackReach : 140.0f;

	bool bIsHitGuaranteed = (DistToPlayer <= (MaxReach + 40.0f));

	if (bIsHitGuaranteed)
	{
		if (UHealthComponent* TargetHealth = PlayerPawn->FindComponentByClass<UHealthComponent>())
		{
			FHitResult DirectHit;
			DirectHit.ImpactPoint = PlayerLoc;

			TargetHealth->TakeDamage(MeleeAttackProfile.Damage, MeleeAttackProfile.DamageTypeTag, DirectHit);

			if (MeleeAttackProfile.SanityDamage > 0.0f)
			{
				if (USanityComponent* TargetSanity = PlayerPawn->FindComponentByClass<USanityComponent>())
				{
					TargetSanity->TakeSanityDamage(MeleeAttackProfile.SanityDamage);
				}
			}

			if (MeleeAttackProfile.StunDurationOnHit > 0.0f)
			{
				if (UStatusEffectComponent* TargetStatus = PlayerPawn->FindComponentByClass<UStatusEffectComponent>())
				{
					TargetStatus->ApplyStun(MeleeAttackProfile.StunDurationOnHit);
				}
			}

#if !UE_BUILD_SHIPPING
			DrawDebugSphere(GetWorld(), PlayerLoc, 40.0f, 16, FColor::Red, false, 0.8f, 0, 2.0f);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
					FString::Printf(TEXT("⚔️ [%s] CIOS TRAFIŁ GRACZA! (-%.0f HP)"), *GetName(), MeleeAttackProfile.Damage));
			}
#endif
		}
	}

	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &ABaseEnemyCharacter::ResetAttackCooldown, MeleeAttackProfile.AttackCooldown, false);
}

void ABaseEnemyCharacter::ResetAttackCooldown()
{
	bCanAttack = true;
}

void ABaseEnemyCharacter::FinishInvestigation()
{
	if (SensoryComp && SensoryComp->CurrentState != EAIBehaviorState::Chase_Attack)
	{
		SensoryComp->SetAIState(EAIBehaviorState::Idle_Patrol);
		if (ALightkeeperAIController* AICon = Cast<ALightkeeperAIController>(GetController()))
		{
			AICon->StartPatrol();
		}
	}
}

void ABaseEnemyCharacter::HandleDamageTaken(float CurrentHP, float MaxHP)
{
	if (HealthComp && HealthComp->IsDead()) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	ALightkeeperAIController* AICon = Cast<ALightkeeperAIController>(GetController());
	FVector PlayerLoc = PlayerPawn->GetActorLocation();

	GetWorld()->GetTimerManager().ClearTimer(InvestigateTimerHandle);

	StealthHitStreak++;
	LastDamageOriginLocation = PlayerLoc;
	TriggerAgitation(PlayerLoc);

	// JEŚLI GRACZ ZADAŁ 2 CIOSY Z UKRYCIA Z RZĘDU -> NATYCHMIASTOWA SZARŻA POŚCIGU (CHASE):
	if (StealthHitStreak >= 2)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID() + 350, 3.0f, FColor::Red,
				FString::Printf(TEXT("🔥 [%s] DRUGIE TRAFIENIE Z UKRYCIA! WPADA W FURIĘ I SZARŻUJE!"), *GetName()));
		}
#endif
		if (SensoryComp && SensoryComp->CurrentState != EAIBehaviorState::Flee_Panic)
		{
			SensoryComp->SetAIState(EAIBehaviorState::Chase_Attack, PlayerLoc);
		}
		return;
	}

	// PIERWSZE TRAFIENIE: Wzburzony marsz prosto do pozycji, skąd padł strzał/rzut:
	if (SensoryComp && SensoryComp->CurrentState != EAIBehaviorState::Chase_Attack && SensoryComp->CurrentState != EAIBehaviorState::Flee_Panic)
	{
		SensoryComp->SetAIState(EAIBehaviorState::Investigate, PlayerLoc);
		if (AICon)
		{
			AICon->MoveToInvestigationPoint(PlayerLoc, 60.0f);
		}
		GetWorld()->GetTimerManager().SetTimer(InvestigateTimerHandle, this, &ABaseEnemyCharacter::FinishInvestigation, 8.0f, false);
	}
}

void ABaseEnemyCharacter::TriggerAgitation(FVector SuspectedOriginLocation)
{
	bIsAgitated = true;

	float Duration = SensoryComp ? SensoryComp->SensoryProfile.AgitationDuration : 6.0f;
	GetWorld()->GetTimerManager().SetTimer(AgitationTimerHandle, this, &ABaseEnemyCharacter::EndAgitation, Duration, false);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID() + 300, 2.5f, FColor::Orange,
			FString::Printf(TEXT("💢 [%s] WZBURZENIE (Agitated)! Ignoruje rykoszety i idzie do źródła ataku!"), *GetName()));
	}
#endif
}

void ABaseEnemyCharacter::EndAgitation()
{
	bIsAgitated = false;
	StealthHitStreak = 0; // Reset serii po uspokojeniu

	if (SensoryComp && SensoryComp->CurrentState == EAIBehaviorState::Idle_Patrol)
	{
		if (ALightkeeperAIController* AICon = Cast<ALightkeeperAIController>(GetController()))
		{
			AICon->StartPatrol();
		}
	}
}

void ABaseEnemyCharacter::HandleDeath()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
		World->GetTimerManager().ClearTimer(AgitationTimerHandle);
	}

	if (SensoryComp) SensoryComp->Deactivate();
	if (EquippedToolInstance) EquippedToolInstance->UnequipTool();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorEnableCollision(false);

	Destroy();
}

void ABaseEnemyCharacter::DrawAIDebugGizmos()
{
	if (!SensoryComp) return;

	FVector HeadLoc = GetActorLocation() + FVector(0.0f, 0.0f, 65.0f);
	FVector Forward = GetActorForwardVector();

	FColor ConeColor = (SensoryComp->CurrentState == EAIBehaviorState::Chase_Attack) ? FColor::Red :
		(SensoryComp->CurrentState == EAIBehaviorState::Investigate ? FColor::Yellow : FColor::Green);

	DrawDebugCone(
		GetWorld(),
		HeadLoc,
		Forward,
		SensoryComp->SensoryProfile.SightRadius,
		FMath::DegreesToRadians(SensoryComp->SensoryProfile.PeripheralVisionAngle),
		FMath::DegreesToRadians(SensoryComp->SensoryProfile.PeripheralVisionAngle),
		16,
		ConeColor,
		false,
		-1.0f,
		0,
		1.0f
	);

	if (SensoryComp->SensoryProfile.ProximitySenseRadius > 0.0f)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), SensoryComp->SensoryProfile.ProximitySenseRadius, 12, FColor::Cyan, false, -1.0f, 0, 0.8f);
	}
}