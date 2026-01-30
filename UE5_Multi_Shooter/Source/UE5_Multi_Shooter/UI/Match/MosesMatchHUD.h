#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "MosesMatchHUD.generated.h"

class UProgressBar;
class UTextBlock;

class UMosesMatchAnnouncementWidget;
class UMosesMatchRulePopupWidget;

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
	//~UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

private:
	/** PlayerState Delegate 바인딩. */
	void BindToPlayerState();

	/** MatchGameState Delegate 바인딩. */
	void BindToGameState_Match();

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

	/** 초 -> "MM:SS" 변환 */
	static FString ToMMSS(int32 TotalSeconds);

private:
	// =====================================================================
	// BindWidgetOptional: BP 디자이너 위젯 이름과 동일해야 자동 바인딩된다.
	// (이름이 다르면 nullptr로 들어온다)
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

	// 중앙 남은 시간
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchCountdownText = nullptr;

	// 방송 / 룰팝업 자식 위젯
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesMatchAnnouncementWidget> AnnouncementWidget = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMosesMatchRulePopupWidget> RulePopupWidget = nullptr;

private:
	// =====================================================================
	// 캐시: Delegate 해제를 위해 보관
	// =====================================================================
	TWeakObjectPtr<AMosesPlayerState> CachedPlayerState;
	TWeakObjectPtr<AMosesMatchGameState> CachedMatchGameState;
};
