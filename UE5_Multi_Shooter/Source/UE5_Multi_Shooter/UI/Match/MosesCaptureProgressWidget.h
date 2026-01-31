// ============================================================================
// MosesCaptureProgressWidget.h (FULL)
// ----------------------------------------------------------------------------
// - PlayerState(SSOT)의 MosesCaptureComponent RepNotify->Delegate를 받아
//   캡처 진행도(0..1)를 표시한다.
// - 2.5초 경고(기본값)는 MosesFlagFeedbackData의 WarningThresholdSeconds로 결정한다.
// - 루프 SFX는 "로컬 코스메틱"이며 캡처 진행도에 따라 Volume/Pitch를 보정한다.
//
// 주의
// - HUD는 절대 게임 상태를 바꾸지 않는다. (표시 전용)
// - Tick 금지 원칙 유지: Delegate 기반으로만 UI 갱신.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesCaptureProgressWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAudioComponent;

class UMosesCaptureComponent;
class UMosesFlagFeedbackData;
class AMosesFlagSpot;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCaptureProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleCaptureStateChanged(bool bCapturing, float ProgressAlpha, float HoldSeconds, TWeakObjectPtr<AMosesFlagSpot> Spot);

	UMosesCaptureComponent* ResolveMyCaptureComponent() const;

	void UpdateRootVisibility(bool bCapturing);
	void UpdateProgress(float ProgressAlpha);
	void UpdatePercentText(float ProgressAlpha);
	void UpdateWarning(bool bCapturing, float ProgressAlpha, float HoldSeconds);
	void UpdateLoopSFX(bool bCapturing, float ProgressAlpha);

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> Progress_Capture;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Percent;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Warning;

	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	TObjectPtr<UMosesFlagFeedbackData> FeedbackData;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudio;

	UPROPERTY(Transient)
	TObjectPtr<UMosesCaptureComponent> CachedCaptureComponent;
};
