// ============================================================================
// GameFeatureAction_MosesCombatUI.h (FULL)
// - GF_Combat_UIRuntime: GameFeature 활성/비활성 시 Combat HUD 주입/해제 액션
//
// 핵심 목표
// - MatchLevel(또는 매치 월드)에서만 HUD를 붙인다. Start/Lobby에서는 절대 붙이지 않는다.
// - SeamlessTravel/PIE 멀티에서 PlayerController 생성 타이밍이 늦어 HUD 부착이 스킵되는 문제를 방지한다.
//   -> PC가 없으면 타이머로 재시도한다.
// - 중복 방지: LocalPlayer별로 HUD 1개만 보장한다.
// - 월드 종료/전환 시 위젯/타이머를 완전히 정리한다.
//
// 엔진/버전 호환성
// - UGameFeatureAction_WorldActionBase를 사용하지 않는다.
// - UGameFeatureAction + WorldDelegates(OnPostWorldInitialization/OnWorldCleanup) 기반으로 처리.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFeatureAction_MosesCombatUI.generated.h"

class UUserWidget;
class UWorld;

UCLASS()
class GF_COMBAT_UIRUNTIME_API UGameFeatureAction_MosesCombatUI : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	UGameFeatureAction_MosesCombatUI();

protected:
	//~UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~End of UGameFeatureAction interface

private:
	// =========================================================================
	// 내부 자료구조
	// =========================================================================

	/** 월드 단위 관리 데이터 */
	struct FPerWorldData
	{
		/** LocalPlayer별 HUD 1개 보장 */
		TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<UUserWidget>> WidgetByLocalPlayer;

		/** PC 준비 타이밍 문제 대응: 월드별 재시도 횟수 */
		int32 RetryCount = 0;

		/** 월드별 재시도 타이머 핸들 */
		FTimerHandle RetryTimerHandle;
	};

private:
	// =========================================================================
	// World Delegate 핸들러
	// =========================================================================

	/** 월드 생성 직후(초기화 직후) 호출: 여기서 HUD 주입 시도 */
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	/** 월드 정리 시점 호출: 여기서 위젯/타이머 정리 */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

private:
	// =========================================================================
	// HUD 주입/해제
	// =========================================================================

	/** 현재 월드가 Combat HUD를 붙여야 하는 "매치 월드"인지 판정 */
	bool ShouldInjectForWorld(const UWorld* World) const;

	/** 월드에 HUD 주입 시도(조건 불충족이면 스킵/재시도 예약) */
	void InjectForWorld(UWorld* World);

	/** 월드에서 HUD 제거(정리) */
	void RemoveForWorld(UWorld* World);

private:
	// =========================================================================
	// 재시도(PC 타이밍 보정)
	// =========================================================================

	/** PC가 아직 없으면 일정 시간 후 재시도 예약 */
	void ScheduleRetryInject(UWorld* World);

	/** 타이머로 호출되는 재시도 함수 */
	void RetryInject(UWorld* World);

private:
	// =========================================================================
	// 상태/데이터
	// =========================================================================

	/** Feature 활성 상태 플래그 */
	bool bIsActive = false;

	/** 월드별 데이터(위젯/타이머/재시도 카운트) */
	TMap<TWeakObjectPtr<UWorld>, FPerWorldData> WorldDataByWorld;

	/** World Delegate 핸들 */
	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle WorldCleanupHandle;

private:
	// =========================================================================
	// 에디터 세팅(SSOT: 지금 단계에서는 GF Action이 SSOT)
	// =========================================================================

	/** 매치 HUD 위젯 클래스 (Widget Blueprint도 OK: WBP_XXX_C) */
	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	/** AddToViewport ZOrder */
	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	int32 ViewportZOrder = 10;

	/**
	 * [MOD] Match 월드 필터 키워드
	 * - "MatchLevel"이 포함된 월드에서만 HUD를 붙인다.
	 * - PIE에서는 "UEDPIE_1_MatchLevel"처럼 prefix가 붙으므로 Contains 검사로 처리한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	FString MatchWorldNameToken = TEXT("MatchLevel");

	/**
	 * [MOD] 재시도 정책
	 * - PC가 준비되지 않았을 때 몇 번까지 재시도할지
	 * - 재시도 간격(초)
	 */
	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	int32 MaxRetryCount = 20;

	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	float RetryIntervalSeconds = 0.2f;
};
