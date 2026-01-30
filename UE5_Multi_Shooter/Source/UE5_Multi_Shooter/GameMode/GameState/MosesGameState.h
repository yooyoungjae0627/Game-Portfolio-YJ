// ============================================================================
// MosesGameState.h (FULL)
// - Lobby/Match 공통 GameState 베이스
// - Lyra 원칙에 맞춰 ExperienceManagerComponent를 GameState에 고정한다.
// - Lobby/Match 파생 GameState는 이 클래스를 상속받는다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "MosesGameState.generated.h"

class UMosesExperienceManagerComponent;

/**
 * EMosesServerPhase
 *
 * 로비/매치의 "큰 단계"만 나타내는 전역 Phase.
 * - Lobby UI 및 ServerTravel 경계에서 사용
 * - Match의 Warmup/Combat/Result는 AMosesMatchGameState의 EMosesMatchPhase로 분리한다.
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
 * 한 문장 요약:
 * - Lobby/Match에서 공통으로 필요한 "Experience 기반 파이프라인"을 GameState에 고정하는 베이스.
 *
 * 핵심:
 * - UMosesExperienceManagerComponent는 반드시 여기에서 CreateDefaultSubobject로 생성되어야 한다.
 * - GameModeBase(서버)는 InitGameState() 시점에 GameState에서 ExperienceManagerComponent를 찾아야 한다.
 * - SeamlessTravel에서도 InitGameState는 BeginPlay 이전에 호출될 수 있으므로 "런타임 AddComponent"는 금지한다.
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
	// -------------------------------------------------------------------------
	// Experience
	// -------------------------------------------------------------------------
	/** GameMode/Subsystem이 Experience 흐름에 접근할 수 있도록 제공한다. */
	UMosesExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

public:
	// -------------------------------------------------------------------------
	// Server-only API (Global Phase)
	// -------------------------------------------------------------------------
	void ServerSetPhase(EMosesServerPhase NewPhase);

public:
	// -------------------------------------------------------------------------
	// Read-only accessors
	// -------------------------------------------------------------------------
	EMosesServerPhase GetCurrentPhase() const { return CurrentPhase; }

private:
	// -------------------------------------------------------------------------
	// RepNotifies
	// -------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_CurrentPhase();

protected:
	// -------------------------------------------------------------------------
	// Components (Must exist on every derived GameState)
	// -------------------------------------------------------------------------
	/** Lyra 스타일 Experience 로딩/Ready 매니저. 모든 파생 GameState에서 반드시 존재해야 한다. */
	UPROPERTY(VisibleAnywhere, Category = "Moses|Experience")
	TObjectPtr<UMosesExperienceManagerComponent> ExperienceManagerComponent;

private:
	// -------------------------------------------------------------------------
	// Replicated state
	// -------------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	EMosesServerPhase CurrentPhase = EMosesServerPhase::None;
};
