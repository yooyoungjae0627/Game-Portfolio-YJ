#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "MosesMatchHUD.generated.h"

/**
 * UMosesMatchHUD
 *
 * [역할]
 * - Dedicated Server / Server Authority 100% 구조에서 HUD는 "표시만" 담당한다.
 * - HUD 갱신 원칙:
 *   - Tick 금지
 *   - UMG Designer Binding 금지
 *   - RepNotify → Native Delegate → Widget Update ONLY
 *
 * [데이터 소스(SSOT)]
 * - PlayerState(SSOT) : HP/Shield/Score/Deaths/Captures/ZombieKills/Grenade
 * - PlayerState 소유 CombatComponent : Weapon/Ammo/Equip/Reload/Slots (서버 승인 결과)
 * - MatchGameState(복제) : Phase / RemainingSeconds / Announcement
 * - PlayerState 소유 CaptureComponent : 캡처 진행 상태(RepNotify→Delegate)
 *
 * [중요: 캡처 UI 연결]
 * - WBP_CombatHUD(Parent=UMosesMatchHUD) 안에 배치된 CaptureProgress 위젯(인스턴스 이름: "CaptureProgress")을
 *   BindWidgetOptional로 잡는다.
 * - 실제 진행률 값/경고 표시/OnOff는 UMosesCaptureComponent::OnCaptureStateChanged 델리게이트로만 갱신한다.
 * - HUD는 캡처 컴포넌트의 내부 상태를 "Getter"로 읽지 않는다(프로젝트 현재 API에 없음).
 *   → 즉, 이벤트 기반으로만 동작. (LateJoin은 CaptureComponent 쪽 RepNotify에서 1회 Broadcast 하면 100% 커버)
 */

class UProgressBar;
class UTextBlock;
class UImage;

class AMosesPlayerState;
class AMosesMatchGameState;

// [CAPTURE] 델리게이트 파라미터로 전달되는 Spot(약포인터)
class AMosesFlagSpot;

class UMosesMatchAnnouncementWidget;
class UMosesCrosshairWidget;
class UMosesScopeWidget;

class UMosesCombatComponent;
class UMosesWeaponData;

// [CAPTURE] PlayerState 소유 캡처 컴포넌트(진행도 RepNotify→Delegate)
class UMosesCaptureComponent;

// [CAPTURE] WBP_CaptureProgress의 Parent(플러그인/프로젝트 구조에 따라 위치는 다를 수 있음)
class UMosesCaptureProgressWidget;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesMatchHUD(const FObjectInitializer& ObjectInitializer);

public:
	/**
	 * SetScopeVisible_Local
	 * - 스코프 UI는 로컬 연출(UI)만 담당한다.
	 * - 판정/데미지/명중은 서버에서만 수행.
	 */
	void SetScopeVisible_Local(bool bVisible);

protected:
	//~UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// =========================================================================
	// Bind / Retry (Tick 금지: Timer 기반 재시도)
	// =========================================================================
	void BindToPlayerState();
	void BindToGameState_Match();

	void BindToCombatComponent_FromPlayerState();
	void UnbindCombatComponent();

	// [CAPTURE] PlayerState에서 CaptureComponent를 찾아 바인딩
	void BindToCaptureComponent_FromPlayerState();
	void UnbindCaptureComponent();

	void StartBindRetry();
	void StopBindRetry();
	void TryBindRetry();

	bool IsPlayerStateBound() const;
	bool IsMatchGameStateBound() const;
	bool IsCombatComponentBound() const;

	// [CAPTURE]
	bool IsCaptureComponentBound() const;

	void ApplySnapshotFromMatchGameState();
	void RefreshInitial();

private:
	// =========================================================================
	// PlayerState Delegate Handlers (SSOT)
	// =========================================================================
	void HandleHealthChanged(float Current, float Max);
	void HandleShieldChanged(float Current, float Max);
	void HandleScoreChanged(int32 NewScore);
	void HandleDeathsChanged(int32 NewDeaths);

	void HandleCapturesChanged(int32 NewCaptures);
	void HandleZombieKillsChanged(int32 NewZombieKills);

	void HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);

private:
	// =========================================================================
	// CombatComponent Delegate Handlers (SSOT on PlayerState)
	// =========================================================================
	void HandleAmmoChanged_FromCombat(int32 Mag, int32 Reserve);
	void HandleEquippedChanged_FromCombat(int32 SlotIndex, FGameplayTag WeaponId);
	void HandleReloadingChanged_FromCombat(bool bReloading);
	void HandleSlotsStateChanged_FromCombat(int32 ChangedSlotOr0ForAll);

private:
	// =========================================================================
	// [CAPTURE] CaptureComponent Delegate Handler
	// =========================================================================
	/**
	 * HandleCaptureStateChanged
	 *
	 * [시그니처]
	 * - 이 함수는 UMosesCaptureComponent::OnCaptureStateChanged 델리게이트 시그니처와 1:1로 일치해야 한다.
	 * - 현재 빌드 로그 기준:
	 *     void(bool bActive, float Progress01, float WarningFloat, TWeakObjectPtr<AMosesFlagSpot> Spot)
	 *
	 * [의미]
	 * - bActive      : 현재 캡처 진행 중이면 true (아니면 HUD는 숨김)
	 * - Progress01   : 0~1 진행률
	 * - WarningFloat : 프로젝트 정책에 따라 의미가 다를 수 있음(경고 시간/알파/남은시간 등)
	 *                 HUD에서는 "경고 ON/OFF"만 필요하므로 >0이면 Warning ON으로 처리한다.
	 * - Spot         : 현재 캡처 대상 Spot(약포인터). LateJoin에서 null 가능(안전 처리)
	 */
	void HandleCaptureStateChanged(bool bActive, float Progress01, float WarningFloat, TWeakObjectPtr<AMosesFlagSpot> Spot);

private:
	// =========================================================================
	// HUD Update Helpers
	// =========================================================================
	void UpdateCurrentWeaponHeader();
	void UpdateSlotPanels_All();

private:
	// =========================================================================
	// MatchGameState Delegate Handlers (복제 상태)
	// =========================================================================
	void HandleMatchTimeChanged(int32 RemainingSeconds);
	void HandleMatchPhaseChanged(EMosesMatchPhase NewPhase);
	void HandleAnnouncementChanged(const FMosesAnnouncementState& State);

private:
	// =========================================================================
	// Utils
	// =========================================================================
	static FString ToMMSS(int32 TotalSeconds);
	static FText GetPhaseText_KR(EMosesMatchPhase Phase);
	static int32 GetPhasePriority(EMosesMatchPhase Phase);

private:
	// =========================================================================
	// Crosshair Update (표시 전용)
	// - Tick 금지 → Timer(20Hz)로 로컬 표시만 업데이트
	// =========================================================================
	void StartCrosshairUpdate();
	void StopCrosshairUpdate();
	void TickCrosshairUpdate();

	float CalculateCrosshairSpreadFactor_Local() const;

private:
	// =========================================================================
	// [CAPTURE] CaptureProgress 내부 위젯 캐시
	// =========================================================================
	/**
	 * CacheCaptureProgress_InternalWidgets
	 * - WBP_CombatHUD에 배치된 CaptureProgress(UMosesCaptureProgressWidget) 내부의
	 *   ProgressBar/TextBlock을 GetWidgetFromName으로 찾아 캐시한다.
	 *
	 * [전제: 내부 위젯 이름 고정]
	 * - ProgressBar: "Progress_Capture"
	 * - TextBlock : "Text_Percent"
	 * - TextBlock : "Text_Warning"
	 */
	void CacheCaptureProgress_InternalWidgets();

private:
	// =========================================================================
	// BindWidgetOptional (WBP 이름이 다르면 nullptr로 남음 → 로그로 검증)
	// =========================================================================
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ShieldBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShieldText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DefeatsAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CapturesAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ZombieKillsAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponAmmoAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GrenadeAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchCountdownText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesMatchAnnouncementWidget> AnnouncementWidget = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesCrosshairWidget> CrosshairWidget = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesScopeWidget> ScopeWidget = nullptr;

	// 현재 무기 헤더
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CurrentWeaponName = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CurrentSlot = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Reloading = nullptr;

	// 슬롯 1~4 패널
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot1_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot1_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>    Img_Slot1_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot1_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot2_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot2_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>    Img_Slot2_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot2_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot3_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot3_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>    Img_Slot3_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot3_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot4_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot4_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>    Img_Slot4_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot4_Reloading = nullptr;

	// [CAPTURE] WBP_CombatHUD에 배치된 인스턴스 이름: "CaptureProgress"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesCaptureProgressWidget> CaptureProgress = nullptr;

private:
	// =========================================================================
	// Cached pointers (바인딩 대상)
	// =========================================================================
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	TWeakObjectPtr<AMosesPlayerState>   CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatComponent;

	// [CAPTURE] PlayerState 소유 캡처 컴포넌트 캐시
	TWeakObjectPtr<UMosesCaptureComponent> CachedCaptureComponent;

private:
	// =========================================================================
	// Bind retry timer (Tick 금지)
	// =========================================================================
	FTimerHandle BindRetryHandle;
	int32 BindRetryTryCount = 0;
	int32 BindRetryMaxTry = 25;
	float BindRetryInterval = 0.20f;

private:
	// =========================================================================
	// Crosshair update timer (표시 전용)
	// =========================================================================
	FTimerHandle CrosshairTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairUpdateInterval = 0.05f; // 20Hz

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairLogThreshold = 0.08f;

	float LastLoggedCrosshairSpread = -1.0f;

private:
	// =========================================================================
	// [CAPTURE] 내부 위젯 캐시 (WBP_CaptureProgress 내부)
	// =========================================================================
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Capture_ProgressBar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Capture_TextPercent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Capture_TextWarning = nullptr;

	UPROPERTY(Transient)
	bool bCaptureInternalCached = false;
};
