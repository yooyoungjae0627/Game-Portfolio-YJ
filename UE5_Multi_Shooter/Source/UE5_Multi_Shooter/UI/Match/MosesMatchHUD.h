// ============================================================================
// MosesMatchHUD.h (FULL)
// - HUD는 표시만 담당 (Tick/Binding 금지)
// - PlayerState(SSOT) + MatchGameState(복제) Delegate로만 갱신
// - [MOD] 클라2에서 GameState 바인딩 타이밍 이슈 -> Timer 기반 재시도
// - [MOD] PhaseText: Warmup="워밍업"(흰), Combat="매치"(빨강), Result="결과"(흰)
// - [MOD] Phase rollback(Combat인데 Warmup 늦게 도착 등) 방지
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h" // FMosesAnnouncementState, EMosesMatchPhase

#include "MosesMatchHUD.generated.h"

class UProgressBar;
class UTextBlock;
class UButton;

class UMosesMatchAnnouncementWidget;

class AMosesPlayerState;
class AMosesMatchGameState;

/**
 * Match HUD
 * - 서버 권위 구조에서 HUD는 "표시만" 담당한다.
 * - 상태 소스:
 *   - PlayerState(SSOT): HP/Shield/Score/Deaths/Ammo/Grenade
 *   - MatchGameState(복제): RemainingTime/Phase/Announcement
 */
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
	// Bind (Tick 금지 → Timer 재시도)
	// --------------------------------------------------------------------
	void BindToPlayerState();
	void BindToGameState_Match();
	void RefreshInitial();

	// [MOD] Bind 재시도 (클라2 타이밍 이슈)
	void ScheduleBindRetry();
	void TryBindRetry();
	bool IsBoundToPlayerState() const;
	bool IsBoundToMatchGameState() const;

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
	// Util
	// --------------------------------------------------------------------
	static FString ToMMSS(int32 TotalSeconds);

	// [MOD] PhaseText 표시 정책 (한글)
	static FText GetPhaseText_KR(EMosesMatchPhase Phase);

	// [MOD] PhaseText 색상 정책
	static FSlateColor GetPhaseColor(EMosesMatchPhase Phase);

	// [MOD] Phase rollback 방지용 우선순위
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

	// [MOD] HUD 표시용 마지막 Phase 캐시 (역행 이벤트 무시)
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	// [MOD] Bind 재시도 타이머 (Tick 금지)
	FTimerHandle BindRetryHandle;
	int32 BindRetryCount = 0;

	// [MOD] 재시도 정책
	static constexpr int32 BindRetryMaxCount = 30;   // 30 * 0.2 = 약 6초
	static constexpr float BindRetryInterval = 0.2f;
};
