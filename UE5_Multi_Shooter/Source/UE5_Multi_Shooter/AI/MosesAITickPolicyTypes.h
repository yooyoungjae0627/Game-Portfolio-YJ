// AI/MosesAITickPolicyTypes.h
#pragma once

#include "CoreMinimal.h"
#include "MosesAITickPolicyTypes.generated.h"

/**
 * DAY3: AI 업데이트 빈도 정책 Tier
 */
UENUM(BlueprintType)
enum class EMosesAITickTier : uint8
{
	S UMETA(DisplayName="S (Close / In Combat)"),
	A UMETA(DisplayName="A (Mid / Chase)"),
	B UMETA(DisplayName="B (Far / Roam)"),
	C UMETA(DisplayName="C (Very Far / Idle)")
};

USTRUCT(BlueprintType)
struct FMosesAITickTierParams
{
	GENERATED_BODY()

	/** AIController + BrainComponent TickInterval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DecisionIntervalSec = 0.2f;

	/**
	 * Perception 폴링 주기(엔진 Perception 내부 주기 직접 제어는 케이스가 많아서,
	 * DAY3에서는 "수동 폴링(간격마다 가장 가까운 타겟 확인)"으로 안정 구현)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PerceptionPollIntervalSec = 0.2f;

	/** MoveTo 재요청/재경로 최소 쿨다운 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NavRepathCooldownSec = 0.5f;
};

USTRUCT(BlueprintType)
struct FMosesAITickPolicyConfig
{
	GENERATED_BODY()

	// ----- Tier 판정 거리(미터가 아니라 UE 단위 cm) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistS = 800.0f;   // <= 8m
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistA = 1600.0f;  // <= 16m
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistB = 3000.0f;  // <= 30m
	// 그 밖은 C

	// ----- 히스테리시스 / 최소 유지 -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HysteresisMargin = 200.0f;  // 경계에서 왔다갔다 방지
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinTierHoldSec = 2.0f;      // Tier 변경 최소 유지 시간

	// ----- Tier별 파라미터 -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMosesAITickTierParams TierS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMosesAITickTierParams TierA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMosesAITickTierParams TierB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMosesAITickTierParams TierC;

	FMosesAITickPolicyConfig()
	{
		// 문서 기본값 범위 반영 (S 0.1~0.2 / A 0.2~0.3 / B 0.4~0.6 / C 0.8~1.2)
		TierS.DecisionIntervalSec = 0.15f; TierS.PerceptionPollIntervalSec = 0.15f; TierS.NavRepathCooldownSec = 0.25f;
		TierA.DecisionIntervalSec = 0.25f; TierA.PerceptionPollIntervalSec = 0.25f; TierA.NavRepathCooldownSec = 0.40f;
		TierB.DecisionIntervalSec = 0.50f; TierB.PerceptionPollIntervalSec = 0.50f; TierB.NavRepathCooldownSec = 0.75f;
		TierC.DecisionIntervalSec = 1.00f; TierC.PerceptionPollIntervalSec = 1.00f; TierC.NavRepathCooldownSec = 1.25f;
	}
};
