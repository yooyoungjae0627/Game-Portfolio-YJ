#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesMatchHUD.generated.h"

class UProgressBar;
class UTextBlock;

class AMosesPlayerState;
class AMosesMatchGameState;              // [MOD] Match GS만 사용
struct FMosesAnnouncementState;          // [MOD] forward
enum class EMosesMatchPhase : uint8;     // [MOD] forward

class UMosesMatchAnnouncementWidget;
class UMosesMatchRulePopupWidget;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesMatchHUD : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	void BindToPlayerState();
	void BindToGameState_Match();

	void RefreshInitial();
	static FString ToMMSS(int32 TotalSeconds);

private:
	// PlayerState handlers
	void HandleHealthChanged(float Current, float Max);
	void HandleShieldChanged(float Current, float Max);
	void HandleScoreChanged(int32 NewScore);
	void HandleDeathsChanged(int32 NewDeaths);
	void HandleAmmoChanged(int32 Mag, int32 Reserve);
	void HandleGrenadeChanged(int32 Grenade);

	// MatchGameState handlers
	void HandleMatchTimeChanged(int32 RemainingSeconds);
	void HandleMatchPhaseChanged(EMosesMatchPhase NewPhase);
	void HandleAnnouncementChanged(const FMosesAnnouncementState& State);

private:
	// -------------------------
	// BindWidget (Tick/Binding 금지 → 직접 Set)
	// -------------------------
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> HealthBar = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> ShieldBar = nullptr;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HealthText = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ShieldText = nullptr;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MatchCountdownText = nullptr;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ScoreAmount = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DefeatsAmount = nullptr;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> WeaponAmmoAmount = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> GrenadeAmount = nullptr;

	// -------------------------
	// Optional sub widgets
	// -------------------------
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UMosesMatchAnnouncementWidget> AnnouncementWidget = nullptr;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UMosesMatchRulePopupWidget> RulePopupWidget = nullptr;
};
