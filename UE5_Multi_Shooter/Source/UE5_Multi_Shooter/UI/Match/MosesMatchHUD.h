// ============================================================================
// UE5_Multi_Shooter/UI/Match/MosesMatchHUD.h  (FULL - UPDATED)  [STEP3]
// ============================================================================
//
// [STEP3 목표]
// - HUD는 "현재 장착(CurrentSlot) 무기 탄약"만 표시한다.
// - 스왑(1~4) 승인으로 CurrentSlot이 바뀌면, HUD가 즉시 그 무기의 탄약으로 전환된다.
// - Tick/Binding 금지: RepNotify -> Delegate 기반만 사용한다.
//
// [핵심 변경점]
// - 기존 HUD는 PlayerState의 OnAmmoChanged에 의존했는데,
//   현재 코드베이스에서는 CombatComponent(SSOT)가 OnAmmoChanged를 가장 정확하게/즉시 발행한다.
// - 따라서 HUD가 PlayerState를 바인딩한 뒤, PlayerState 소유 CombatComponent의 Delegate에도 바인딩한다.
//   -> CurrentSlot 변화(CombatComponent OnRep_CurrentSlot)에서 AmmoChanged가 즉시 Broadcast되므로
//      HUD는 "스왑 즉시 탄약 전환"을 보장한다.
//
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"  
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "MosesMatchHUD.generated.h"

class UProgressBar;
class UTextBlock;

class AMosesPlayerState;
class AMosesMatchGameState;

class UMosesMatchAnnouncementWidget;
class UMosesCrosshairWidget;
class UMosesScopeWidget;

class UMosesCombatComponent;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesMatchHUD(const FObjectInitializer& ObjectInitializer);

public:
	// --------------------------------------------------------------------
	// [DAY8] Scope UI 표시 토글 (로컬 연출)
	// - 서버 권위와 무관
	// --------------------------------------------------------------------
	void SetScopeVisible_Local(bool bVisible);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// --------------------------------------------------------------------
	// Bind / Retry
	// --------------------------------------------------------------------
	void BindToPlayerState();
	void BindToGameState_Match();

	// [STEP3][MOD] CombatComponent 바인딩 (Ammo/Equip 이벤트)
	void BindToCombatComponent_FromPlayerState();     // ✅ [MOD]
	void UnbindCombatComponent();                      // ✅ [MOD]
	bool IsCombatComponentBound() const;               // ✅ [MOD]

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
	void HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve);      // ✅ [MOD] 이름 명확화
	void HandleGrenadeChanged(int32 Grenade);

	// --------------------------------------------------------------------
	// [STEP3][MOD] CombatComponent Delegate Handlers
	// - CurrentSlot 변경 / Ammo 변경이 가장 즉시/정확하게 들어오는 경로
	// --------------------------------------------------------------------
	void HandleAmmoChanged_FromCombat(int32 Mag, int32 Reserve);  // ✅ [MOD]
	void HandleEquippedChanged_FromCombat(int32 SlotIndex, FGameplayTag WeaponId); // ✅ [MOD]

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
	// --------------------------------------------------------------------
	// [DAY7] Crosshair Update Loop (표시 전용, Tick 금지)
	// --------------------------------------------------------------------
	void StartCrosshairUpdate();
	void StopCrosshairUpdate();
	void TickCrosshairUpdate();

	float CalculateCrosshairSpreadFactor_Local() const;

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

	// PhaseText
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseText = nullptr;

	// 중앙 남은 시간
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchCountdownText = nullptr;

	// 방송 위젯
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesMatchAnnouncementWidget> AnnouncementWidget = nullptr;

	// Crosshair widget
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesCrosshairWidget> CrosshairWidget = nullptr;

	// Scope widget (Overlay)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesScopeWidget> ScopeWidget = nullptr;

private:
	// =====================================================================
	// 캐시: Delegate 해제를 위해 보관
	// =====================================================================
	TWeakObjectPtr<AMosesPlayerState> CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;

	// [STEP3][MOD] CombatComponent 캐시
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatComponent; // ✅ [MOD]

private:
	// =====================================================================
	// [FIX] Bind Retry Timer
	// =====================================================================
	FTimerHandle BindRetryHandle;
	int32 BindRetryTryCount = 0;
	int32 BindRetryMaxTry = 25;
	float BindRetryInterval = 0.20f;

private:
	// =====================================================================
	// [DAY7] Crosshair Timer (표시 전용)
	// =====================================================================
	FTimerHandle CrosshairTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairUpdateInterval = 0.05f; // 20Hz

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairLogThreshold = 0.08f;

	float LastLoggedCrosshairSpread = -1.0f;
};
