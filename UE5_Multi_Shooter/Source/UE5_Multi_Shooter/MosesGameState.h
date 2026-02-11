// ============================================================================
// UE5_Multi_Shooter/MosesGameState.h  (CLEANED)
// - Base GameState for Lobby/Match
// - Owns ExperienceManagerComponent (must exist for seamless travel)
// - Replicates "Global Phase" (Lobby / Match) for UI and travel boundaries
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

#include "MosesGameState.generated.h"

class UMosesExperienceManagerComponent;

/**
 * Global server phase (큰 흐름만)
 * - Lobby/Match 경계 (UI 정책 / ServerTravel guard)
 * - Match 내부(Warmup/Combat/Result)는 AMosesMatchGameState의 EMosesMatchPhase로 분리
 */
UENUM(BlueprintType)
enum class EMosesServerPhase : uint8
{
	None  UMETA(DisplayName = "None"),
	Lobby UMETA(DisplayName = "Lobby"),
	Match UMETA(DisplayName = "Match"),
};

/**
 * AMosesGameState
 *
 * 한 줄:
 * - Lobby/Match 공통 "Experience 파이프라인"을 GameState에 고정해둔 베이스.
 *
 * 정책:
 * - ExperienceManagerComponent는 CreateDefaultSubobject로 "항상 존재"해야 한다.
 * - GameModeBase(서버)는 InitGameState()에서 GameState로부터 ExperienceManager를 얻는다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMosesGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// =========================================================================
	// Experience
	// =========================================================================
	UMosesExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

	// =========================================================================
	// Server-only API: Global Phase
	// =========================================================================
	void ServerSetPhase(EMosesServerPhase NewPhase);

	// =========================================================================
	// Read-only
	// =========================================================================
	EMosesServerPhase GetCurrentPhase() const { return CurrentPhase; }

private:
	// =========================================================================
	// RepNotify
	// =========================================================================
	UFUNCTION()
	void OnRep_CurrentPhase();

protected:
	// =========================================================================
	// Components (must exist in derived GS)
	// =========================================================================
	UPROPERTY(VisibleAnywhere, Category = "Moses|Experience")
	TObjectPtr<UMosesExperienceManagerComponent> ExperienceManagerComponent;

private:
	// =========================================================================
	// Replicated state
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	EMosesServerPhase CurrentPhase = EMosesServerPhase::None;
};
