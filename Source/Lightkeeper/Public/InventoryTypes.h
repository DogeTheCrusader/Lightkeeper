#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InventoryTypes.generated.h"

// 1. EQUIP TYPE:
UENUM(BlueprintType)
enum class EItemEquipType : uint8
{
	None			UMETA(DisplayName = "None / Inventory Only (Keys, Notes, Loot)"),
	MeleeWeapon		UMETA(DisplayName = "Melee Weapon (Knife, Crowbar, Club)"),
	Throwable		UMETA(DisplayName = "Throwable (Grenades, Molotovs, Bottles)"),
	Firearm			UMETA(DisplayName = "Firearm (Revolver, Shotgun)"),
	UtilityTool		UMETA(DisplayName = "Utility Tool (Burner, Tuning Fork, Siphon)")
};

// 2. COMBAT TRACE SHAPES:
UENUM(BlueprintType)
enum class EToolTraceShape : uint8
{
	SphereSweep     UMETA(DisplayName = "Sphere Sweep (Blunt Club, Hammer, Fist)"),
	BoxSweep        UMETA(DisplayName = "Box Sweep (Blade Slash, Cleaver, Axe)"),
	LineTrace       UMETA(DisplayName = "Line Trace (Pistol, Thrust)"),
	Cone            UMETA(DisplayName = "Cone AoE (Burner, Shotgun)")
};

UENUM(BlueprintType)
enum class EGuardBlockType : uint8
{
	FistGuard       UMETA(DisplayName = "Fist Guard (Domyślna garda rękami: -35% DMG)"),
	ToolGuard       UMETA(DisplayName = "Tool Guard (Garda narzędziem/łomem: -65% DMG)"),
	HeavyShield     UMETA(DisplayName = "Heavy Reinforced (Pancerna garda: -85% DMG)")
};

// 3. QUICK MELEE TYPES:
UENUM(BlueprintType)
enum class EMeleeAttackType : uint8
{
	FistPunch		UMETA(DisplayName = "Fist Punch / Push (Default Baseline)"),
	WeaponBash		UMETA(DisplayName = "Weapon Bash / Handle Strike (Club, Pistol)"),
	QuickSlash		UMETA(DisplayName = "Quick Slash (Knife, Dagger)")
};

// 4. EMISSION TRIGGERS:
UENUM(BlueprintType)
enum class EEmissionTrigger : uint8
{
	OnDestroy		UMETA(DisplayName = "On Destroy (Fragile Bottles, Molotovs)"),
	OnImpact		UMETA(DisplayName = "On Impact (Explodes on Surface Hit)"),
	TimedFuse		UMETA(DisplayName = "Timed Fuse (Clockwork Grenades)"),
	Proximity		UMETA(DisplayName = "Proximity / Pressure (Mines)"),
	ContinuousZone	UMETA(DisplayName = "Continuous Zone (Ruptured Pipes, Fire Pit)"),
	Manual			UMETA(DisplayName = "Manual")
};

// 5. EMISSION SHAPES:
UENUM(BlueprintType)
enum class EEmissionShape : uint8
{
	Sphere           UMETA(DisplayName = "Sphere 360 (Molotov, Grenade, Gas Cloud)"),
	DirectionalCone  UMETA(DisplayName = "Directional Cone (Steam Nozzle, Flame Jet)"),
	LineLaser        UMETA(DisplayName = "Line / Laser (Tripwire Beam)"),
	BoxVolume        UMETA(DisplayName = "Box Volume (Pressure Plate, Acid Pool)"),
	CylinderDisc     UMETA(DisplayName = "Flat Cylinder Disc (Oil Puddle, Acid Spill)")
};

USTRUCT(BlueprintType)
struct FInventoryItemData
{
	GENERATED_BODY()

	// ====================================================================
	// 1. ITEM IDENTITY
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Item Identity")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Item Identity")
	FText ItemName = FText::FromString(TEXT("New Item"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Item Identity", meta = (MultiLine = true))
	FText ItemDescription = FText::FromString(TEXT("Item description..."));

	// ====================================================================
	// 2. INVENTORY GRID
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2. Inventory Grid")
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2. Inventory Grid")
	class UTexture2D* ItemIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2. Inventory Grid")
	TSubclassOf<class ABaseInteractable> DropClass;

	// ====================================================================
	// 3. 3D PHYSICS & ECONOMY
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	class UStaticMesh* ItemMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	bool bSavedCanBeDestroyed = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	float SavedDamageThreshold = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	float SavedDamageSusceptibility = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	float SavedHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	float PrimaryValue = 30.0f; // Universal load: Fuel amount / Healing HP

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Physics & Economy")
	int32 ItemGoldValue = 10;

	// ====================================================================
	// 4. COMBAT & HAND TOOL (Visible when EquipType != None)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat")
	EItemEquipType EquipType = EItemEquipType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	TSubclassOf<class ABaseTool> EquipToolClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	FTransform HandGripOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	FGameplayTag AnimPoseTag;

	// Cooldown między atakami / rzutami (widoczny dla każdej broni, w tym miotanej i narzędzi!):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Timing",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	float ActionCooldown = 0.6f;

	// --- PRIMARY / LIGHT ATTACK ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	float BaseDamage = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	FGameplayTag PhysicalDamageTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	EToolTraceShape AttackShape = EToolTraceShape::SphereSweep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "(EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool) && AttackShape == EToolTraceShape::SphereSweep", EditConditionHides))
	float AttackRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "(EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool) && AttackShape == EToolTraceShape::BoxSweep", EditConditionHides, ToolTip = "Połowa wymiarów pola rażenia (Długość, szerokość, grubość)"))
	FVector BoxTraceHalfExtents = FVector(15.0f, 35.0f, 5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "(EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool) && AttackShape == EToolTraceShape::Cone", EditConditionHides))
	float AttackConeAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Primary Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	float StaminaCostPerAttack = 15.0f;

	// --- ELEMENTAL STRIKE ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Elemental",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	float ElementalDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Elemental",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	FGameplayTag AttackStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Elemental",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon || EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool", EditConditionHides))
	float StateIntensity = 1.0f;

	// --- HEAVY ATTACK (Tylko dla Broni Wręcz) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Heavy Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	float HeavyAttackDamage = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Heavy Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	float HeavyAttackRange = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Heavy Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	float HeavyAttackRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Heavy Attack",
		meta = (EditCondition = "EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	float HeavyAttackStaminaCost = 35.0f;

	// --- DEFENSE & GUARD ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Guard",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	EGuardBlockType GuardType = EGuardBlockType::FistGuard;

	// Jaki procent obrażeń pochłania to narzędzie (np. 0.65 = 65% redukcji):
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Guard",
		meta = (EditCondition = "EquipType != EItemEquipType::None && GuardType != EGuardBlockType::FistGuard", EditConditionHides))
	float GuardDamageAbsorption = 0.65f;

	// Mnożnik zużycia staminy przy zablokowaniu ciosu:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Guard",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	float GuardStaminaCostMultiplier = 1.0f;

	// --- QUICK MELEE [V] ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Quick Melee [V]",
		meta = (EditCondition = "EquipType != EItemEquipType::None", EditConditionHides))
	EMeleeAttackType QuickMeleeType = EMeleeAttackType::FistPunch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Quick Melee [V]",
		meta = (EditCondition = "EquipType != EItemEquipType::None && QuickMeleeType != EMeleeAttackType::FistPunch", EditConditionHides))
	float QuickMeleeDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Quick Melee [V]",
		meta = (EditCondition = "EquipType != EItemEquipType::None && QuickMeleeType != EMeleeAttackType::FistPunch", EditConditionHides))
	float QuickMeleeRange = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Combat|Quick Melee [V]",
		meta = (EditCondition = "EquipType != EItemEquipType::None && QuickMeleeType != EMeleeAttackType::FistPunch", EditConditionHides))
	float QuickMeleeCooldown = 0.5f;

	// ====================================================================
	// 5. FUEL & AMMUNITION
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Fuel & Ammo",
		meta = (EditCondition = "EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool || EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	bool bUsesSharedFuel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Fuel & Ammo",
		meta = (EditCondition = "EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool || EquipType == EItemEquipType::MeleeWeapon", EditConditionHides))
	float ResourceCostPerAction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Fuel & Ammo",
		meta = (EditCondition = "(EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool) && !bUsesSharedFuel", EditConditionHides))
	FGameplayTag RequiredAmmoTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Fuel & Ammo",
		meta = (EditCondition = "(EquipType == EItemEquipType::Firearm || EquipType == EItemEquipType::UtilityTool) && !bUsesSharedFuel", EditConditionHides))
	float MaxAmmoCapacity = 6.0f;

	// ====================================================================
	// 6. STATE EMITTER & EXPLOSIONS
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter")
	bool bIsStateEmitter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	EEmissionTrigger TriggerType = EEmissionTrigger::OnImpact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	float EmissionBurstDamage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	FGameplayTag EmittedStateTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	float EmissionStateIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	float SplashRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	bool bDestroyOnEmission = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	EEmissionShape EmissionShape = EEmissionShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	FName EmissionSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && EmissionShape == EEmissionShape::DirectionalCone", EditConditionHides))
	float StreamConeAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && EmissionShape == EEmissionShape::BoxVolume", EditConditionHides))
	FVector BoxEmissionExtents = FVector(100.0f, 100.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && TriggerType == EEmissionTrigger::TimedFuse", EditConditionHides))
	float FuseTime = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && bIsPersistentZone", EditConditionHides))
	bool bIsPersistentZone = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && bIsPersistentZone", EditConditionHides))
	float EmissionDuration = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && EmissionShape == EEmissionShape::CylinderDisc", EditConditionHides))
	float DiscHeight = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter && (EmissionShape == EEmissionShape::DirectionalCone || EmissionShape == EEmissionShape::LineLaser)", EditConditionHides))
	bool bInvertEmissionDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. State Emitter",
		meta = (EditCondition = "bIsStateEmitter", EditConditionHides))
	bool bAlignToSurfaceNormal = true;
};