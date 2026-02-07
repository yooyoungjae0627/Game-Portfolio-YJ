#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/Match/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "MosesMatchHUD.generated.h"

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

// [CAPTURE] WBP_CaptureProgress의 Parent
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

	// [ADD] 캡처 성공 UI 요청(로컬 1회)
	void TryBroadcastCaptureSuccess_Once();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// =========================================================================
	// [MOD] Aim UI immediate sync (Sniper/Crosshair/Scope)
	// =========================================================================
	void UpdateAimWidgets_Immediate(); // [MOD]
	bool IsSniperEquipped_Local() const; // [MOD]

private:
	// =========================================================================
	// Bind / Retry (Tick 금지: Timer 기반 재시도)
	// =========================================================================
	void BindToPlayerState();
	void BindToGameState_Match();

	void BindToCombatComponent_FromPlayerState();
	void UnbindCombatComponent();

	void BindToCaptureComponent_FromPlayerState();
	void UnbindCaptureComponent();

	void StartBindRetry();
	void StopBindRetry();
	void TryBindRetry();

	bool IsPlayerStateBound() const;
	bool IsMatchGameStateBound() const;
	bool IsCombatComponentBound() const;
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

	void HandleCapturesChanged(int32 NewCaptures);
	void HandleZombieKillsChanged(int32 NewZombieKills);
	// PvP Kills(내가 플레이어 죽인 수) -> DefeatsAmount 갱신용
	void HandlePvPKillsChanged(int32 NewPvPKills);
	void HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);
	//  ReserveMax 포함 버전
	void HandleAmmoChangedEx_FromCombat(int32 Mag, int32 ReserveCur, int32 ReserveMax);

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
	void CacheCaptureProgress_InternalWidgets();

private:
	// =========================================================================
	// BindWidgetOptional
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
	// Cached pointers
	// =========================================================================
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	TWeakObjectPtr<AMosesPlayerState>   CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatComponent;
	TWeakObjectPtr<UMosesCaptureComponent> CachedCaptureComponent;

private:
	// =========================================================================
	// Bind retry timer
	// =========================================================================
	FTimerHandle BindRetryHandle;
	int32 BindRetryTryCount = 0;
	int32 BindRetryMaxTry = 25;
	float BindRetryInterval = 0.20f;

private:
	// =========================================================================
	// Crosshair update timer
	// =========================================================================
	FTimerHandle CrosshairTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairUpdateInterval = 0.05f; // 20Hz

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairLogThreshold = 0.08f;

	float LastLoggedCrosshairSpread = -1.0f;

private:
	// =========================================================================
	// [CAPTURE] 내부 위젯 캐시
	// =========================================================================
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Capture_ProgressBar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Capture_TextPercent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Capture_TextWarning = nullptr;

	UPROPERTY(Transient)
	bool bCaptureInternalCached = false;

	bool bCaptureSuccessAnnounced_Local = false;

	bool bHUDVisibleActivated = false;

};
