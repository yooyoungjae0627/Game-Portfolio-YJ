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

class AMosesFlagSpot;

class UMosesMatchAnnouncementWidget;
class UMosesCrosshairWidget;
class UMosesScopeWidget;

class UMosesCombatComponent;
class UMosesWeaponData;

class UMosesCaptureComponent;
class UMosesCaptureProgressWidget;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesMatchHUD(const FObjectInitializer& ObjectInitializer);

public:
	void SetScopeVisible_Local(bool bVisible);
	void TryBroadcastCaptureSuccess_Once();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// =========================================================================
	// [MOD] Aim UI immediate sync (Sniper/Crosshair/Scope)
	// =========================================================================
	void UpdateAimWidgets_Immediate();
	bool IsSniperEquipped_Local() const;

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
	void HandlePvPKillsChanged(int32 NewPvPKills);

	void HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);

	void HandleAmmoChangedEx_FromCombat(int32 Mag, int32 ReserveCur, int32 ReserveMax);

	// ✅ [MOD] Dead/Respawn (Victim only - Local)
	void HandleDeathStateChanged(bool bIsDead, float RespawnEndServerTime);
	void StartRespawnNotice_Local(float RespawnEndServerTime);
	void StopRespawnNotice_Local();
	void TickRespawnNotice_Local();

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
	// MatchGameState Delegate Handlers
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
	// Crosshair Update (표시 전용) - Timer(20Hz)
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
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock>   HealthText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ShieldBar = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock>   ShieldText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ScoreAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DefeatsAmount = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CapturesAmount = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ZombieKillsAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WeaponAmmoAmount = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GrenadeAmount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PhaseText = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MatchCountdownText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UMosesMatchAnnouncementWidget> AnnouncementWidget = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UMosesCrosshairWidget> CrosshairWidget = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UMosesScopeWidget> ScopeWidget = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_CurrentWeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_CurrentSlot = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot1_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot1_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_Slot1_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot1_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot2_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot2_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_Slot2_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot2_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot3_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot3_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_Slot3_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot3_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot4_WeaponName = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot4_Ammo = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_Slot4_Equipped = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Slot4_Reloading = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UMosesCaptureProgressWidget> CaptureProgress = nullptr;

private:
	// =========================================================================
	// Cached pointers
	// =========================================================================
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	TWeakObjectPtr<AMosesPlayerState>    CachedPlayerState;
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
	float CrosshairUpdateInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairLogThreshold = 0.08f;

	float LastLoggedCrosshairSpread = -1.0f;

private:
	// =========================================================================
	// [CAPTURE] internal cache
	// =========================================================================
	UPROPERTY(Transient) TObjectPtr<UProgressBar> Capture_ProgressBar = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   Capture_TextPercent = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   Capture_TextWarning = nullptr;

	UPROPERTY(Transient) bool bCaptureInternalCached = false;
	bool bCaptureSuccessAnnounced_Local = false;

private:
	// =========================================================================
	// [MOD] Local respawn notice (Victim only)
	// =========================================================================
	FTimerHandle RespawnNoticeTimerHandle;

	// 서버가 복제해준 “리스폰 종료 서버시간”
	float RespawnNoticeEndServerTime = 0.f;

	// 로컬 리스폰 공지 중엔 전원 공지(AnnouncementState)를 무시한다
	bool bLocalRespawnNoticeActive = false;
};
