// ============================================================================
// UE5_Multi_Shooter/Match/UI/Match/MosesMatchHUD.h (FULL)  [MOD]
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

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
	// Scope UI (Local cosmetic only)
	void SetScopeVisible_Local(bool bVisible);

	// ✅ Pickup toast (Owner only)
	void ShowPickupToast_Local(const FText& Text, float DurationSeconds);

	// ✅ Headshot toast (Owner only)  [MOD]
	void ShowHeadshotToast_Local(const FText& Text, float DurationSeconds);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// Aim UI
	void UpdateAimWidgets_Immediate();
	bool IsSniperEquipped_Local() const;

private:
	// Bind / Retry
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
	// PlayerState Handlers
	void HandleHealthChanged(float Current, float Max);
	void HandleShieldChanged(float Current, float Max);
	void HandleScoreChanged(int32 NewScore);
	void HandleCapturesChanged(int32 NewCaptures);
	void HandleZombieKillsChanged(int32 NewZombieKills);
	void HandlePvPKillsChanged(int32 NewPvPKills);
	void HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);

	// Combat snapshot
	void HandleAmmoChangedEx_FromCombat(int32 Mag, int32 ReserveCur, int32 ReserveMax);

	// ✅ Death notice (Victim only)
	void HandleDeathStateChanged(bool bIsDead, float RespawnEndServerTime);
	void StartRespawnNotice_Local(float RespawnEndServerTime);
	void StopRespawnNotice_Local();
	void TickRespawnNotice_Local();

private:
	// CombatComponent handlers
	void HandleAmmoChanged_FromCombat(int32 Mag, int32 Reserve);
	void HandleEquippedChanged_FromCombat(int32 SlotIndex, FGameplayTag WeaponId);
	void HandleReloadingChanged_FromCombat(bool bReloading);
	void HandleSlotsStateChanged_FromCombat(int32 ChangedSlotOr0ForAll);

private:
	// MatchGameState handlers
	void HandleMatchTimeChanged(int32 RemainingSeconds);
	void HandleMatchPhaseChanged(EMosesMatchPhase NewPhase);
	void HandleAnnouncementChanged(const FMosesAnnouncementState& State);

private:
	// Helpers
	static FString ToMMSS(int32 TotalSeconds);
	static FText GetPhaseText_KR(EMosesMatchPhase Phase);
	static int32 GetPhasePriority(EMosesMatchPhase Phase);

	void UpdateCurrentWeaponHeader();
	void UpdateSlotPanels_All();

private:
	// Crosshair Update
	void StartCrosshairUpdate();
	void StopCrosshairUpdate();
	void TickCrosshairUpdate();

	float CalculateCrosshairSpreadFactor_Local() const;

private:
	// [CAPTURE] internal cache
	void CacheCaptureProgress_InternalWidgets();
	void HandleCaptureStateChanged(bool bActive, float Progress01, float WarningFloat, TWeakObjectPtr<AMosesFlagSpot> Spot);
	void TryBroadcastCaptureSuccess_Once();

	// HUD: Result delegate handler
	void HandleResultStateChanged_Local(const FMosesMatchResultState& State);

protected:
	// BP 이벤트로 UI 업데이트 (바인딩 금지)
	UFUNCTION(BlueprintImplementableEvent)
	void BP_ShowResultPopup(
		bool bIsDraw,
		bool bIsWinner,
		const FString& MyId,
		int32 MyCaptures,
		int32 MyZombieKills,
		int32 MyPvPKills,
		int32 MyTotalScore,
		const FString& OpponentId,
		int32 OppCaptures,
		int32 OppZombieKills,
		int32 OppPvPKills,
		int32 OppTotalScore);

	// Confirm 버튼에서 PC 호출 (BP에서 이 함수를 호출하면 됨)
	UFUNCTION(BlueprintCallable)
	void UI_OnClickedConfirmReturnToLobby();

private:
	// Widgets
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ShieldBar = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ShieldText = nullptr;

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
	// Cached refs
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	TWeakObjectPtr<AMosesPlayerState> CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatComponent;
	TWeakObjectPtr<UMosesCaptureComponent> CachedCaptureComponent;

private:
	// Bind retry
	FTimerHandle BindRetryHandle;
	int32 BindRetryTryCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Bind")
	int32 BindRetryMaxTry = 25;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Bind")
	float BindRetryInterval = 0.20f;

private:
	// Crosshair timer
	FTimerHandle CrosshairTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairUpdateInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairLogThreshold = 0.08f;

	float LastLoggedCrosshairSpread = -1.0f;

private:
	// ✅ Respawn notice (Victim only)
	FTimerHandle RespawnNoticeTimerHandle;
	float RespawnNoticeEndServerTime = 0.f;
	bool bLocalRespawnNoticeActive = false;

private:
	// ✅ Pickup toast (Owner only) - block global announcements while active
	FTimerHandle LocalPickupToastTimerHandle;
	bool bLocalPickupToastActive = false;

	void StopLocalPickupToast_Internal();

private:
	// ✅ Headshot toast (Owner only) - block global announcements while active  [MOD]
	FTimerHandle LocalHeadshotToastTimerHandle;
	bool bLocalHeadshotToastActive = false;

	void StopLocalHeadshotToast_Internal();

private:
	// [CAPTURE] cached internal widgets
	UPROPERTY(Transient) TObjectPtr<UProgressBar> Capture_ProgressBar = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   Capture_TextPercent = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   Capture_TextWarning = nullptr;

	UPROPERTY(Transient) bool bCaptureInternalCached = false;
	bool bCaptureSuccessAnnounced_Local = false;
};
