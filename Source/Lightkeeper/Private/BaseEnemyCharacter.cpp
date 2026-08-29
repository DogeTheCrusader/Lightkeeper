#include "BaseEnemyCharacter.h"
#include "AIController.h"
#include "SensoryComponent.h"
#include "HealthComponent.h"
#include "ReactionReceiverComponent.h"
#include "StatusEffectComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseEnemyCharacter::ABaseEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SensoryComp = CreateDefaultSubobject<USensoryComponent>(TEXT("SensoryComponent"));
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	ReactionComp = CreateDefaultSubobject<UReactionReceiverComponent>(TEXT("ReactionReceiverComponent"));
	StatusComp = CreateDefaultSubobject<UStatusEffectComponent>(TEXT("StatusEffectComponent"));

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->PushForceFactor = 0.0f;
	GetCharacterMovement()->InitialPushForceFactor = 0.0f;

	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceWeight = 0.5f;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	if (GetMesh())
	{
		GetMesh()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		GetMesh()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}
}

void ABaseEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (SensoryComp)
	{
		SensoryComp->OnAIStateChanged.AddDynamic(this, &ABaseEnemyCharacter::HandleStateChanged);
	}

	if (HealthComp)
	{
		HealthComp->OnDeath.AddDynamic(this, &ABaseEnemyCharacter::HandleDeath);
		HealthComp->OnHealthChanged.AddDynamic(this, &ABaseEnemyCharacter::HandleDamageTaken);
	}

	GetWorld()->GetTimerManager().SetTimer(PatrolTimerHandle, this, &ABaseEnemyCharacter::PerformRandomPatrol, 4.5f, true);
}

void ABaseEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Ciągły pościg i atak:
	if (SensoryComp && SensoryComp->CurrentState == EAIBehaviorState::Chase_Attack)
	{
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				float DistToPlayer = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());

				if (DistToPlayer <= 140.0f)
				{
					PerformMeleeAttackOnPlayer();
				}
				else
				{
					AICon->MoveToActor(PlayerPawn, 70.0f);
				}
			}
		}
	}
}

void ABaseEnemyCharacter::HandleStateChanged(EAIBehaviorState NewState, FVector TargetLocation)
{
	AAIController* AICon = Cast<AAIController>(GetController());
	UWorld* World = GetWorld();
	if (!World) return;

	switch (NewState)
	{
	case EAIBehaviorState::Idle_Patrol:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);

		if (!World->GetTimerManager().IsTimerActive(PatrolTimerHandle))
		{
			World->GetTimerManager().SetTimer(PatrolTimerHandle, this, &ABaseEnemyCharacter::PerformRandomPatrol, 4.5f, true);
			PerformRandomPatrol();
		}
		break;

	case EAIBehaviorState::Suspicious:
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		World->GetTimerManager().ClearTimer(PatrolTimerHandle);
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);

		if (AICon)
		{
			AICon->StopMovement();
			FVector LookDir = (TargetLocation - GetActorLocation()).GetSafeNormal();
			SetActorRotation(FRotator(0.0f, LookDir.Rotation().Yaw, 0.0f));
		}

		World->GetTimerManager().SetTimer(InvestigateTimerHandle, this, &ABaseEnemyCharacter::FinishInvestigation, 2.5f, false);
		break;

	case EAIBehaviorState::Investigate:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed * 1.35f;
		World->GetTimerManager().ClearTimer(PatrolTimerHandle);

		if (AICon)
		{
			AICon->MoveToLocation(TargetLocation, 60.0f);
		}

		World->GetTimerManager().SetTimer(InvestigateTimerHandle, this, &ABaseEnemyCharacter::FinishInvestigation, 5.0f, false);
		break;

	case EAIBehaviorState::Chase_Attack:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		World->GetTimerManager().ClearTimer(PatrolTimerHandle);
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);

		if (AICon)
		{
			if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				AICon->MoveToActor(PlayerPawn, 80.0f);
			}
		}
		break;

	case EAIBehaviorState::Flee_Panic:
		GetCharacterMovement()->MaxWalkSpeed = FleeSpeed;
		World->GetTimerManager().ClearTimer(PatrolTimerHandle);
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);

		if (AICon)
		{
			FVector FleeDirection = (GetActorLocation() - TargetLocation).GetSafeNormal();
			FVector FleeDestination = GetActorLocation() + (FleeDirection * 1200.0f);
			AICon->MoveToLocation(FleeDestination, 50.0f);
		}
		break;
	}
}

void ABaseEnemyCharacter::HandleDamageTaken(float CurrentHP, float MaxHP)
{
	if (HealthComp && HealthComp->IsDead()) return;

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		AAIController* AICon = Cast<AAIController>(GetController());
		FVector PlayerLoc = PlayerPawn->GetActorLocation();

		// Przerywamy dotychczasowy spacer:
		GetWorld()->GetTimerManager().ClearTimer(PatrolTimerHandle);

		// Jeśli gracz jest w zasięgu wzroku -> natychmiastowy CHASE:
		if (SensoryComp && SensoryComp->CurrentState != EAIBehaviorState::Chase_Attack)
		{
			SensoryComp->SetAIState(EAIBehaviorState::Investigate, PlayerLoc);

			if (AICon)
			{
				// Wymuszamy pełny Pathfinding omijający skrzynie prosto do gracza:
				AICon->MoveToLocation(PlayerLoc, 60.0f, true, true, true, true);
			}

			// Dajemy potworowi 6 sekund na dobiegnięcie do miejsca ataku:
			GetWorld()->GetTimerManager().SetTimer(InvestigateTimerHandle, this, &ABaseEnemyCharacter::FinishInvestigation, 12.0f, false);
		}
	}
}

void ABaseEnemyCharacter::PerformMeleeAttackOnPlayer()
{
	if (!bCanAttack) return;

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UHealthComponent* PlayerHealth = PlayerPawn->FindComponentByClass<UHealthComponent>())
		{
			bCanAttack = false;

			static const FGameplayTag MonsterAttackTag = FGameplayTag::RequestGameplayTag(FName("Damage.Type.Slashing"), false);
			PlayerHealth->TakeDamage(AttackDamage, MonsterAttackTag);

#if !UE_BUILD_SHIPPING
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
					FString::Printf(TEXT("👾 [%s] WYPROWADZIŁ CIOS! (-%.0f HP)"), *GetName(), AttackDamage));
			}
#endif

			GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &ABaseEnemyCharacter::ResetAttackCooldown, AttackCooldown, false);
		}
	}
}

void ABaseEnemyCharacter::ResetAttackCooldown()
{
	bCanAttack = true;
}

void ABaseEnemyCharacter::PerformRandomPatrol()
{
	if (SensoryComp && SensoryComp->CurrentState != EAIBehaviorState::Idle_Patrol) return;

	AAIController* AICon = Cast<AAIController>(GetController());
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (AICon && NavSys)
	{
		FNavLocation RandomLocation;
		if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), 1000.0f, RandomLocation))
		{
			AICon->MoveToLocation(RandomLocation.Location, 50.0f);
		}
	}
}

void ABaseEnemyCharacter::FinishInvestigation()
{
	if (SensoryComp)
	{
		SensoryComp->SetAIState(EAIBehaviorState::Idle_Patrol);
	}
}

void ABaseEnemyCharacter::HandleDeath()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PatrolTimerHandle);
		World->GetTimerManager().ClearTimer(InvestigateTimerHandle);
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	if (SensoryComp) SensoryComp->Deactivate();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorEnableCollision(false);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
			FString::Printf(TEXT("💀 [%s] POTWÓR ZABITY!"), *GetName()));
	}
#endif

	Destroy();
}