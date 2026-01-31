// ============================================================================
// MosesMatchHUD.h (FULL)
// - PhaseText: Warmup -> "워밍업"(흰색), Combat -> "매치"(빨강), Result -> "결과"(흰색)
// - Tick/Binding 금지: Delegate 이벤트에서만 SetText/SetColor
// - [FIX] Phase 전환 시 HUD가 제거/재생성되어 이벤트를 "놓치는" 케이스 대응
//        -> BindRetry + GS Snapshot Apply 로 복구
// - [FIX] 클라2에서 Warmup 카운트가 가끔 멈추는 케이스 대응
//        -> GS delegate 바인딩이 늦게 되더라도 스냅샷 재적용 + 재바인딩
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "MosesMatchHUD.generated.h"

class UProgressBar;
class UTextBlock;

class AMosesPlayerState;
class AMosesMatchGameState;

class UMosesMatchAnnouncementWidget;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesMatchHUD(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// --------------------------------------------------------------------
	// Bind / Retry
	// --------------------------------------------------------------------
	void BindToPlayerState();
	void BindToGameState_Match();

	// [FIX] 바인딩이 늦게 되는(SeamlessTravel/PIE 타이밍) 케이스를 위한 재시도
	void StartBindRetry();
	void StopBindRetry();
	void TryBindRetry();

	bool IsPlayerStateBound() const;
	bool IsMatchGameStateBound() const;

	// [FIX] 현재 GS 스냅샷을 UI에 강제 적용 (이벤트 놓쳤을 때 복구용)
	void ApplySnapshotFromMatchGameState();

	/** 초기값 반영(틱/바인딩 금지). */
	void RefreshInitial();

	// --------------------------------------------------------------------
	// PlayerState Delegate Handlers
	// --------------------------------------------------------------------
	void HandleHealthChanged(float Current, float Max);
	void HandleShieldChanged(float Current, float Max);
	void HandleScoreChanged(int32 NewScore);
	void HandleDeathsChanged(int32 NewDeaths);
	void HandleAmmoChanged(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);

	// --------------------------------------------------------------------
	// MatchGameState Delegate Handlers
	// --------------------------------------------------------------------
	void HandleMatchTimeChanged(int32 RemainingSeconds);
	void HandleMatchPhaseChanged(EMosesMatchPhase NewPhase);
	void HandleAnnouncementChanged(const FMosesAnnouncementState& State);

	// --------------------------------------------------------------------
	// Helpers
	// --------------------------------------------------------------------
	static FString ToMMSS(int32 TotalSeconds);

	static FText GetPhaseText_KR(EMosesMatchPhase Phase);

	// [FIX] HUD 표시용 마지막 Phase 캐시 (역행 이벤트 무시)
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	// [FIX] Phase 우선순위
	static int32 GetPhasePriority(EMosesMatchPhase Phase);

private:
	// =====================================================================
	// BindWidgetOptional: BP 디자이너 위젯 이름과 동일해야 자동 바인딩된다.
	// =====================================================================

	// 체력
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText = nullptr;

	// 쉴드
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ShieldBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShieldText = nullptr;

	// 점수 / 데스
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DefeatsAmount = nullptr;

	// 탄약 / 수류탄
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponAmmoAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GrenadeAmount = nullptr;

	// [중요] PhaseText (BP 위젯 이름이 "PhaseText" 여야 자동 바인딩)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseText = nullptr;

	// 중앙 남은 시간
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchCountdownText = nullptr;

	// 방송 위젯
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesMatchAnnouncementWidget> AnnouncementWidget = nullptr;

private:
	// =====================================================================
	// 캐시: Delegate 해제를 위해 보관
	// =====================================================================
	TWeakObjectPtr<AMosesPlayerState> CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;

private:
	// =====================================================================
	// [FIX] Bind Retry Timer
	// =====================================================================
	FTimerHandle BindRetryHandle;
	int32 BindRetryTryCount = 0;
	int32 BindRetryMaxTry = 25;        // 필요하면 늘려도 됨
	float BindRetryInterval = 0.20f;   // GF_UI Retry랑 동일한 리듬
};
