#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AISensoryTypes.generated.h"

// 4 Główne Stany Behawioralne Potwora (Dla Behavior Tree / Blueprinta)
UENUM(BlueprintType)
enum class EAIBehaviorState : uint8
{
	Idle_Patrol		UMETA(DisplayName = "Patrol / Spoczynek"),
	Suspicious		UMETA(DisplayName = "Zaniepokojony (Nasłuchiwanie)"), // <--- NOWY STAN!
	Investigate		UMETA(DisplayName = "Badanie Punktu (Hałas / Zapach)"),
	Chase_Attack	UMETA(DisplayName = "Pościg i Atak (Wykrycie)"),
	Flee_Panic		UMETA(DisplayName = "Panika i Ucieczka")
};

// Zdarzenie zmysłowe (Punkt Hałasu lub Plama Zapachu)
USTRUCT(BlueprintType)
struct FImSimStimulusEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag StimulusTag; // Np. State.Element.Acoustics.Noise lub Scent.Type.Blood

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float IntensityRadius = 300.0f; // Promień rozchodzenia w cm

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ExpirationTime = 0.0f; // Kiedy bodziec znika ze świata
};

// Pełna Matryca Zmysłów Potwora (Konfigurowalna w Details każdego Blueprinta!)
USTRUCT(BlueprintType)
struct FAISensoryProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight & Proximity")
	float ProximitySenseRadius = 140.0f; // Zmysł dotyku/obecności (odległość na wyciągnięcie ręki)

	// ==========================================================
	// 1. WZROK (Sight)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight")
	float SightRadius = 1500.0f; // 0 = Ślepy potwór (np. Słuchacz)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight")
	float PeripheralVisionAngle = 60.0f; // Kąt stożka widzenia (w stopniach)

	// ==========================================================
	// 2. SŁUCH (Hearing)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2. Hearing")
	float HearingSensitivity = 1.0f; // 0 = Głuchy, 2.0 = Wyostrzony słuch

	// ==========================================================
	// 3. WĘCH (Scent Matrix)
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Scent")
	FGameplayTagContainer TrackedScents; // Np. Scent.Type.Blood, Scent.Type.Bait

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Scent")
	float ScentTrackingRadius = 2500.0f; // Zasięg nosa w cm

	// ==========================================================
	// 4. ZMYSŁY LOVECRAFTOWSKIE / NADNATURALNE
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Senses")
	bool bHuntsLowSanity = false; // Wyczuwanie pękającego umysłu

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Senses", meta = (EditCondition = "bHuntsLowSanity"))
	float SanityHuntThreshold = 0.35f; // Poniżej 35% Sanity potwór wyczuwa gracza w mroku

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Senses")
	bool bIsPhototropic = false; // Łaknienie światła (Leci do zapalonej latarni!)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Senses")
	bool bFleesFromLight = false; // Strach przed światłem (Ucieka w mrok)
};