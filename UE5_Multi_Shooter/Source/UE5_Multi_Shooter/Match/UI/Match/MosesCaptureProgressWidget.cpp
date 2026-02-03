// ============================================================================
// MosesCaptureProgressWidget.cpp (FULL)  [MOD]
// - CaptureComponent(PS SSOT) Delegate 기반 UI 갱신
// - Tick/Binding 금지: Delegate로만 갱신
// - [MOD] Delegate Unbind + BindRetry(늦은 PS/Component) 보강
// ============================================================================

#include "UE5_Multi_Shooter/Match/UI/Match/MosesCaptureProgressWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h" 

#include "UE5_Multi_Shooter/Match/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Match/Flag/MosesFlagFeedbackData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesCaptureProgressWidget::UMosesCaptureProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCaptureProgressWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Init CaptureProgressWidget=%s OwningPlayer=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwningPlayer()));

	// 초기 상태(숨김/0%) 보장
	ResetVisuals();

	// 1) 즉시 바인딩 시도
	BindToCaptureComponent();

	// 2) 아직 못 잡았으면 재시도(SeamlessTravel/PIE 타이밍)
	StartBindRetry();
}

void UMosesCaptureProgressWidget::NativeDestruct()
{
	StopBindRetry();

	// Delegate 해제(가장 중요)
	UnbindFromCaptureComponent();

	if (LoopAudio)
	{
		LoopAudio->Stop();
		LoopAudio = nullptr;
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------
// Resolve / Bind / Retry
// ---------------------------------------------------------------------

UMosesCaptureComponent* UMosesCaptureProgressWidget::ResolveMyCaptureComponent() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	if (!PS)
	{
		return nullptr;
	}

	return PS->FindComponentByClass<UMosesCaptureComponent>();
}

void UMosesCaptureProgressWidget::BindToCaptureComponent()
{
	if (CachedCaptureComponent)
	{
		return;
	}

	UMosesCaptureComponent* CC = ResolveMyCaptureComponent();
	if (!CC)
	{
		UE_LOG(LogMosesHUD, VeryVerbose, TEXT("[HUD][CL] CaptureComponent not found yet Widget=%s"), *GetNameSafe(this));
		return;
	}

	CachedCaptureComponent = CC;
	CachedCaptureComponent->OnCaptureStateChanged.AddUObject(this, &ThisClass::HandleCaptureStateChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound CaptureComponent Delegate CC=%s Widget=%s"),
		*GetNameSafe(CachedCaptureComponent),
		*GetNameSafe(this));

	// 바인딩 직후에도 안전하게 기본 숨김 유지(스냅샷 getter가 없는 구조에서도 안정)
	ResetVisuals();
}

void UMosesCaptureProgressWidget::UnbindFromCaptureComponent()
{
	if (!CachedCaptureComponent)
	{
		return;
	}

	CachedCaptureComponent->OnCaptureStateChanged.RemoveAll(this);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Unbound CaptureComponent Delegate CC=%s Widget=%s"),
		*GetNameSafe(CachedCaptureComponent),
		*GetNameSafe(this));

	CachedCaptureComponent = nullptr;
}

bool UMosesCaptureProgressWidget::IsBoundToCaptureComponent() const
{
	return (CachedCaptureComponent != nullptr);
}

void UMosesCaptureProgressWidget::StartBindRetry()
{
	if (IsBoundToCaptureComponent())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(BindRetryHandle))
	{
		return;
	}

	BindRetryTryCount = 0;

	World->GetTimerManager().SetTimer(
		BindRetryHandle,
		this,
		&ThisClass::TryBindRetry,
		BindRetryInterval,
		true);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Capture BindRetry START Interval=%.2f MaxTry=%d"),
		BindRetryInterval, BindRetryMaxTry);
}

void UMosesCaptureProgressWidget::StopBindRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(BindRetryHandle))
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Capture BindRetry STOP"));
	}
}

void UMosesCaptureProgressWidget::TryBindRetry()
{
	BindRetryTryCount++;

	if (!IsBoundToCaptureComponent())
	{
		BindToCaptureComponent();
	}

	if (IsBoundToCaptureComponent())
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Capture BindRetry DONE Try=%d"), BindRetryTryCount);
		StopBindRetry();
		return;
	}

	if (BindRetryTryCount >= BindRetryMaxTry)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Capture BindRetry GIVEUP Try=%d"), BindRetryTryCount);
		StopBindRetry();
	}
}

// ---------------------------------------------------------------------
// Delegate handler
// ---------------------------------------------------------------------

void UMosesCaptureProgressWidget::HandleCaptureStateChanged(bool bCapturing, float ProgressAlpha, float HoldSeconds, TWeakObjectPtr<AMosesFlagSpot> /*Spot*/)
{
	UpdateRootVisibility(bCapturing);

	if (!bCapturing)
	{
		// 취소/성공: 완전 리셋
		UpdateProgress(0.0f);
		UpdatePercentText(0.0f);

		if (Text_Warning)
		{
			Text_Warning->SetVisibility(ESlateVisibility::Collapsed);
		}

		UpdateLoopSFX(false, 0.0f);
		return;
	}

	UpdateProgress(ProgressAlpha);
	UpdatePercentText(ProgressAlpha);
	UpdateWarning(true, ProgressAlpha, HoldSeconds);
	UpdateLoopSFX(true, ProgressAlpha);
}

// ---------------------------------------------------------------------
// UI update helpers
// ---------------------------------------------------------------------

void UMosesCaptureProgressWidget::ResetVisuals()
{
	UpdateRootVisibility(false);
	UpdateProgress(0.0f);
	UpdatePercentText(0.0f);

	if (Text_Warning)
	{
		Text_Warning->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateLoopSFX(false, 0.0f);
}

void UMosesCaptureProgressWidget::UpdateRootVisibility(bool bCapturing)
{
	SetVisibility(bCapturing ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMosesCaptureProgressWidget::UpdateProgress(float ProgressAlpha)
{
	if (Progress_Capture)
	{
		Progress_Capture->SetPercent(FMath::Clamp(ProgressAlpha, 0.0f, 1.0f));
	}
}

void UMosesCaptureProgressWidget::UpdatePercentText(float ProgressAlpha)
{
	if (Text_Percent)
	{
		const int32 Percent = FMath::RoundToInt(FMath::Clamp(ProgressAlpha, 0.0f, 1.0f) * 100.0f);
		Text_Percent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
	}
}

void UMosesCaptureProgressWidget::UpdateWarning(bool bCapturing, float ProgressAlpha, float HoldSeconds)
{
	if (!Text_Warning)
	{
		return;
	}

	if (!bCapturing)
	{
		Text_Warning->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const float Threshold = FeedbackData ? FeedbackData->WarningThresholdSeconds : 2.5f;
	const float Elapsed = FMath::Clamp(ProgressAlpha, 0.0f, 1.0f) * HoldSeconds;

	const bool bWarn = (Elapsed >= Threshold);
	Text_Warning->SetVisibility(bWarn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bWarn)
	{
		UE_LOG(LogMosesFlag, VeryVerbose, TEXT("[FLAG][CL] CaptureWarning On (>=%.2f)"), Threshold);
	}
}

void UMosesCaptureProgressWidget::UpdateLoopSFX(bool bCapturing, float ProgressAlpha)
{
	// DataAsset 없으면 루프 SFX 기능은 비활성(표시 UI만 동작)
	if (!FeedbackData)
	{
		if (LoopAudio)
		{
			LoopAudio->Stop();
			LoopAudio = nullptr;
		}
		return;
	}

	USoundBase* Loop = FeedbackData->CaptureLoopSFX.LoadSynchronous();
	if (!Loop)
	{
		if (LoopAudio)
		{
			LoopAudio->Stop();
			LoopAudio = nullptr;
		}
		return;
	}

	if (!bCapturing)
	{
		if (LoopAudio)
		{
			LoopAudio->Stop();
			LoopAudio = nullptr;
		}
		return;
	}

	if (!LoopAudio)
	{
		LoopAudio = UGameplayStatics::SpawnSound2D(this, Loop);
	}

	const float Alpha = FMath::Clamp(ProgressAlpha, 0.0f, 1.0f);
	const float Volume = FMath::Lerp(FeedbackData->VolumeMin, FeedbackData->VolumeMax, Alpha);
	const float Pitch = FMath::Lerp(FeedbackData->PitchMin, FeedbackData->PitchMax, Alpha);

	if (LoopAudio)
	{
		LoopAudio->SetVolumeMultiplier(Volume);
		LoopAudio->SetPitchMultiplier(Pitch);
	}

	UE_LOG(LogMosesFlag, VeryVerbose, TEXT("[FLAG][CL] CaptureSFX Volume=%.2f Pitch=%.2f"), Volume, Pitch);
}
