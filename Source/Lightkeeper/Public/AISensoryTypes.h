#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AISensoryTypes.generated.h"

// ====================================================================
// 1. STANY BEHAWIORALNE POTWORA
// ====================================================================
UENUM(BlueprintType)
enum class EAIBehaviorState : uint8
{
	Idle_Patrol		UMETA(DisplayName = "Patrol / Spoczynek"),
	Suspicious		UMETA(DisplayName = "Zaniepokojony (Nasłuchiwanie / Ryk)"),
	Cautious		UMETA(DisplayName = "Czujny / Ostrożny (Przeczesywanie terenu)"),
	Investigate		UMETA(DisplayName = "Badanie Punktu (Hałas / Zapach)"),
	Chase_Attack	UMETA(DisplayName = "Pościg i Atak (Wykrycie)"),
	Flee_Panic		UMETA(DisplayName = "Panika i Ucieczka")
};

// ====================================================================
// 2. STIMULUS EVENT (Zdarzenie zmysłowe w świecie)
// ====================================================================
USTRUCT(BlueprintType)
struct FImSimStimulusEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag StimulusTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float IntensityRadius = 300.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ExpirationTime = 0.0f;
};

// ====================================================================
// 3. PEŁNY PROFIL ZMYSŁÓW POTWORA (Details Panel w Unreal)
// ====================================================================
USTRUCT(BlueprintType)
struct FAISensoryProfile
{
	GENERATED_BODY()

	// ==========================================================
	// 1. WZROK I BLISKOŚĆ
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight & Proximity", meta = (ToolTip = "Dystans (cm), z którego potwór zawsze wyczuwa gracza i natychmiast atakuje."))
	float ProximitySenseRadius = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight & Proximity", meta = (ToolTip = "Maksymalny zasięg wzroku w pełnym świetle (0 = ślepy)."))
	float SightRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight & Proximity", meta = (ToolTip = "Kąt stożka widzenia (np. 60 = 120 stopni przed potworem)."))
	float PeripheralVisionAngle = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1. Sight & Proximity", meta = (ToolTip = "Jeśli PRAWDA, potwór ignoruje ciemność i widzi gracza w 100% mroku."))
	bool bHasNightVision = false;

	// ==========================================================
	// 2. SŁUCH I AKUSTYKA
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2. Hearing & Acoustics", meta = (ToolTip = "Czułość słuchu. 1.0 = standard, 2.0 = słyszy 2x dalej."))
	float HearingSensitivity = 1.0f;

	// ==========================================================
	// 3. WĘCH I TROPIENIE
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Scent & Tracking", meta = (ToolTip = "Tagi zapachów, za którymi potwór podąża (np. Scent.Type.Blood)."))
	FGameplayTagContainer TrackedScents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3. Scent & Tracking", meta = (ToolTip = "Z jakiej odległości potwór potrafi wyczuć pojedynczą plamę zapachu."))
	float ScentTrackingRadius = 2500.0f;

	// ==========================================================
	// 4. KOSMICZNY HORROR I SANITY
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Horror & Sanity", meta = (ToolTip = "Jeśli PRAWDA, patrzenie na tego potwora drenuje Sanity gracza (Gaze Dread). Domyślnie FAŁSZ dla ludzi!"))
	bool bCausesGazeDread = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Horror & Sanity", meta = (EditCondition = "bCausesGazeDread", ToolTip = "Ile punktów Sanity na sekundę traci gracz, gdy wpatruje się w tego potwora."))
	float GazeDreadDrainRate = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Horror & Sanity", meta = (ToolTip = "Jeśli PRAWDA, potwór wyczuwa gracza w mroku, gdy ten traci poczytalność."))
	bool bHuntsLowSanity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Horror & Sanity", meta = (EditCondition = "bHuntsLowSanity", ToolTip = "Poniżej jakiego % Sanity (np. 0.35) potwór zaczyna wyczuwać gracza."))
	float SanityHuntThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4. Cosmic Horror & Sanity", meta = (ToolTip = "Jeśli PRAWDA, potwór wyczuwa zadyszkę i ciężki oddech gracza (bIsFatigued)."))
	bool bHearsExhaustedBreath = false;

	// ==========================================================
	// 5. MANEKINY I SPOJRZENIE
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Mannequin & Gaze", meta = (ToolTip = "Jeśli PRAWDA, potwór atakuje, gdy gracz spojrzy mu w twarz (Złoty Aktor)."))
	bool bTriggeredByDirectGaze = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Mannequin & Gaze", meta = (ToolTip = "Jeśli PRAWDA, potwór zastyga w bezruchu, gdy na niego patrzymy lub widzi swoje odbicie w lustrze."))
	bool bFreezesWhenObserved = false;

	// ==========================================================
	// 6. INTERAKCJE ZE ŚWIATŁEM
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. Light Interactions", meta = (ToolTip = "Jeśli PRAWDA, potwór jest przyciągany przez zapalona latarnię (Cień-Złodziej)."))
	bool bIsPhototropic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. Light Interactions", meta = (ToolTip = "Jeśli PRAWDA, potwór ucieka w mrok, gdy gracz włączy latarnię [F]."))
	bool bFleesFromLight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "6. Light Interactions", meta = (ToolTip = "Jeśli PRAWDA, potwór boi się wejść w oświetloną strefę (np. stoi na krawędzi cienia)."))
	bool bStopsAtLight = false;

	// ==========================================================
	// 7. ZACHOWANIE I WALKA
	// ==========================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "7. Behavior & Combat", meta = (ToolTip = "Jeśli PRAWDA, potwór NIGDY nie atakuje fizycznie (tylko np. pożera naftę albo ucieka)."))
	bool bIsPacifist = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "7. Behavior & Combat", meta = (ToolTip = "Czas zastygania/ryku potwora w ułamku sekundy przed rozpoczęciem sprintu."))
	float AggroTelegraphDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "7. Behavior & Combat", meta = (ToolTip = "Czas trwania stanu podwyższonej czujności po otrzymaniu ciosu lub wykryciu rzutu."))
	float AgitationDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "7. Behavior & Combat", meta = (ToolTip = "Jeśli PRAWDA, potwór panicznie ucieka, gdy zostanie podpalony (Burning)."))
	bool bFleesFromFire = true;
};