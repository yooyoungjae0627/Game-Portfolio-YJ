// ============================================================================
// GameFeatureAction_MosesMatchRules.h (FULL)
// ----------------------------------------------------------------------------
// [역할]
// - GF_Match_Rules 활성화/비활성화 시점에, "서버 월드"에서 FlagSpot들의
//   SetFlagSystemEnabled(bool) 를 호출하여 Flag 시스템 Enable Gate를 토글한다.
//
// [중요: 모듈 의존성]
// - GF 플러그인은 프로젝트 메인 모듈(UE5_Multi_Shooter)에 직접 의존하지 않는다.
//   즉, 아래를 include 하지 않는다.
//     - UE5_Multi_Shooter/MosesLogChannels.h
//     - UE5_Multi_Shooter/Flag/MosesFlagSpot.h
// - 대신, 월드의 액터들을 순회하면서 리플렉션으로 함수 호출을 수행한다.
//   (UFunction FindFunction + ProcessEvent)
//
// [서버 권한]
// - Server Authority 100%: AuthGameMode가 존재하는 월드에서만 적용한다.
// - PIE/게임 월드만 적용하도록 필터링한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"       // UGameFeatureAction (확실히 존재)
#include "Engine/EngineTypes.h"      // FWorldContext
#include "GameFeatureAction_MosesMatchRules.generated.h"

class UWorld;

/**
 * Flag 룰 스위치 GameFeatureAction.
 *
 * 프로젝트 코드에 강결합하지 않고, 함수 이름 기반 리플렉션 호출로 동작한다.
 */
UCLASS()
class GF_MATCH_RULESRUNTIME_API UGameFeatureAction_MosesMatchRules : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	// =========================================================================
	// UGameFeatureAction Interface
	// =========================================================================

	/** GameFeature 활성화 시 호출 */
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;

	/** GameFeature 비활성화 시 호출 */
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	// =========================================================================
	// Internal Helpers
	// =========================================================================

	/** 현재 Action이 이 월드에 적용되어야 하는지 판단 */
	bool ShouldApplyToWorld(const UWorld* World) const;

	/** 모든 월드 컨텍스트를 순회하여 서버 월드에만 적용 */
	void ApplyToAllRelevantWorlds(bool bEnable) const;

	/** 월드 내 FlagSpot 후보 액터들에게 Enable 호출(리플렉션) */
	void ApplyFlagEnableToWorld(UWorld* World, bool bEnable) const;

	/** 액터가 FlagSpot 후보인지(함수 존재 여부로 판단) */
	bool IsFlagSpotCandidate(const AActor* Actor) const;

	/** Actor::SetFlagSystemEnabled(bool)을 리플렉션으로 호출 */
	void InvokeSetFlagSystemEnabled(AActor* Actor, bool bEnable) const;

private:
	// =========================================================================
	// Tuning
	// =========================================================================

	/** Game/PIE 월드에만 적용(기본 true) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Rules")
	bool bOnlyApplyInGameWorld = true;

	/** 활성화 시 Enable 값(기본 true) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Rules")
	bool bEnableOnActivate = true;

	/** 호출할 함수 이름(프로젝트 액터에 동일 시그니처 함수가 있어야 함) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Rules")
	FName FlagEnableFunctionName = TEXT("SetFlagSystemEnabled");
};
