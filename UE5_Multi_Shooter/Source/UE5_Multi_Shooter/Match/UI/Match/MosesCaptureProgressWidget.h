// ============================================================================
// MosesCaptureProgressWidget.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// - PlayerState(SSOT)의 MosesCaptureComponent RepNotify->Delegate를 받아
//   캡처 진행도(0..1)를 표시한다.
// - 2.5초 경고(기본값)는 MosesFlagFeedbackData의 WarningThresholdSeconds로 결정한다.
// - 루프 SFX는 "로컬 코스메틱"이며 캡처 진행도에 따라 Volume/Pitch를 보정한다.
//
// 주의
// - HUD는 절대 게임 상태를 바꾸지 않는다. (표시 전용)
// - Tick 금지 원칙 유지: Delegate 기반으로만 UI 갱신.
// - [MOD] 위젯 재생성/SeamlessTravel 타이밍 대응:
//   * NativeOnInitialized에서 1회 바인딩 시도
//   * BindRetry 타이머로 늦게 생성되는 PS/Component를 따라잡는다.
//   * NativeDestruct에서 Delegate Unbind + 타이머 정리 필수
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

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesCaptureProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesCaptureProgressWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	// ---------------------------------------------------------------------
	// Delegate handler (CaptureComponent)
	// ---------------------------------------------------------------------
	void HandleCaptureStateChanged(bool bCapturing, float ProgressAlpha, float HoldSeconds, TWeakObjectPtr<AMosesFlagSpot> Spot);

	// ---------------------------------------------------------------------
	// Resolve / Bind / Retry
	// ---------------------------------------------------------------------
	UMosesCaptureComponent* ResolveMyCaptureComponent() const;

	void BindToCaptureComponent();
	void UnbindFromCaptureComponent();

	bool IsBoundToCaptureComponent() const;

	void StartBindRetry();
	void StopBindRetry();
	void TryBindRetry();

private:
	// ---------------------------------------------------------------------
	// UI update helpers (Tick 금지: Delegate로만 호출)
	// ---------------------------------------------------------------------
	void UpdateRootVisibility(bool bCapturing);
	void UpdateProgress(float ProgressAlpha);
	void UpdatePercentText(float ProgressAlpha);
	void UpdateWarning(bool bCapturing, float ProgressAlpha, float HoldSeconds);
	void UpdateLoopSFX(bool bCapturing, float ProgressAlpha);

	void ResetVisuals();

private:
	// ---------------------------------------------------------------------
	// Widgets (BP 이름과 동일해야 BindWidgetOptional로 잡힘)
	// ---------------------------------------------------------------------
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Progress_Capture = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Percent = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Warning = nullptr;

private:
	// ---------------------------------------------------------------------
	// Data (체감 파라미터)
	// - Data-only GF에서 공급하는 DA_FlagFeedback_Default를 지정하는 용도
	// ---------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	TObjectPtr<UMosesFlagFeedbackData> FeedbackData = nullptr;

private:
	// ---------------------------------------------------------------------
	// Runtime
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudio = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMosesCaptureComponent> CachedCaptureComponent = nullptr;

private:
	// ---------------------------------------------------------------------
	// BindRetry (PS/Component 늦게 뜨는 케이스 대응)
	// ---------------------------------------------------------------------
	FTimerHandle BindRetryHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Capture")
	float BindRetryInterval = 0.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|HUD|Capture")
	int32 BindRetryMaxTry = 25;

	int32 BindRetryTryCount = 0;
};
