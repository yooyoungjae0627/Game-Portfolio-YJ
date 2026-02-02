#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "MosesMatchHUD.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;

class AMosesPlayerState;
class AMosesMatchGameState;

class UMosesMatchAnnouncementWidget;
class UMosesCrosshairWidget;
class UMosesScopeWidget;

class UMosesCombatComponent;
class UMosesWeaponData;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesMatchHUD(const FObjectInitializer& ObjectInitializer);

public:
	void SetScopeVisible_Local(bool bVisible);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// Bind / Retry
	void BindToPlayerState();
	void BindToGameState_Match();

	void BindToCombatComponent_FromPlayerState();
	void UnbindCombatComponent();

	void StartBindRetry();
	void StopBindRetry();
	void TryBindRetry();

	bool IsPlayerStateBound() const;
	bool IsMatchGameStateBound() const;
	bool IsCombatComponentBound() const;

	void ApplySnapshotFromMatchGameState();
	void RefreshInitial();

private:
	// PlayerState Delegate Handlers
	void HandleHealthChanged(float Current, float Max);
	void HandleShieldChanged(float Current, float Max);
	void HandleScoreChanged(int32 NewScore);
	void HandleDeathsChanged(int32 NewDeaths);

	// ✅ [FIX][MOD] PlayerState 전용 델리게이트 이름으로 변경
	void HandleCapturesChanged(int32 NewCaptures);
	void HandleZombieKillsChanged(int32 NewZombieKills);

	void HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);

private:
	// CombatComponent Delegate Handlers
	void HandleAmmoChanged_FromCombat(int32 Mag, int32 Reserve);
	void HandleEquippedChanged_FromCombat(int32 SlotIndex, FGameplayTag WeaponId);
	void HandleReloadingChanged_FromCombat(bool bReloading);
	void HandleSlotsStateChanged_FromCombat(int32 ChangedSlotOr0ForAll);

private:
	// HUD Update Helpers
	void UpdateCurrentWeaponHeader();
	void UpdateSlotPanels_All();

private:
	// MatchGameState Delegate Handlers
	void HandleMatchTimeChanged(int32 RemainingSeconds);
	void HandleMatchPhaseChanged(EMosesMatchPhase NewPhase);
	void HandleAnnouncementChanged(const FMosesAnnouncementState& State);

private:
	static FString ToMMSS(int32 TotalSeconds);
	static FText GetPhaseText_KR(EMosesMatchPhase Phase);
	static int32 GetPhasePriority(EMosesMatchPhase Phase);

private:
	// Crosshair Update (표시 전용)
	void StartCrosshairUpdate();
	void StopCrosshairUpdate();
	void TickCrosshairUpdate();

	float CalculateCrosshairSpreadFactor_Local() const;

private:
	// =====================================================================
	// BindWidgetOptional
	// =====================================================================
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

	// [MOD] 캡처/좀비킬 TextBlock (WBP 이름 정확히)
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

private:
	EMosesMatchPhase LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;

	TWeakObjectPtr<AMosesPlayerState> CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatComponent;

	FTimerHandle BindRetryHandle;
	int32 BindRetryTryCount = 0;
	int32 BindRetryMaxTry = 25;
	float BindRetryInterval = 0.20f;

	FTimerHandle CrosshairTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairUpdateInterval = 0.05f; // 20Hz

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Crosshair")
	float CrosshairLogThreshold = 0.08f;

	float LastLoggedCrosshairSpread = -1.0f;
};
