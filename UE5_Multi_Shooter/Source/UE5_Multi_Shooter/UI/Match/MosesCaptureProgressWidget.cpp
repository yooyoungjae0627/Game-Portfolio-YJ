// ============================================================================
// MosesCaptureProgressWidget.cpp (FULL)
// - CaptureComponent(PS SSOT) Delegate 기반 UI 갱신
// - Tick/Binding 금지: Delegate로만 갱신
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesCaptureProgressWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h" // ✅ APlayerState 완전 타입

#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Flag/MosesFlagFeedbackData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

void UMosesCaptureProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedCaptureComponent = ResolveMyCaptureComponent();
	if (CachedCaptureComponent)
	{
		CachedCaptureComponent->OnCaptureStateChanged.AddUObject(this, &UMosesCaptureProgressWidget::HandleCaptureStateChanged);
	}

	// 초기 상태
	UpdateRootVisibility(false);
	UpdateProgress(0.0f);
	UpdatePercentText(0.0f);

	if (Text_Warning)
	{
		Text_Warning->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMosesCaptureProgressWidget::NativeDestruct()
{
	if (LoopAudio)
	{
		LoopAudio->Stop();
		LoopAudio = nullptr;
	}

	Super::NativeDestruct();
}

UMosesCaptureComponent* UMosesCaptureProgressWidget::ResolveMyCaptureComponent() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	// ✅ TObjectPtr/APlayerState 문제 회피: GetPlayerState 사용
	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	return PS ? PS->FindComponentByClass<UMosesCaptureComponent>() : nullptr;
}

void UMosesCaptureProgressWidget::HandleCaptureStateChanged(bool bCapturing, float ProgressAlpha, float HoldSeconds, TWeakObjectPtr<AMosesFlagSpot> /*Spot*/)
{
	UpdateRootVisibility(bCapturing);
	UpdateProgress(ProgressAlpha);
	UpdatePercentText(ProgressAlpha);
	UpdateWarning(bCapturing, ProgressAlpha, HoldSeconds);
	UpdateLoopSFX(bCapturing, ProgressAlpha);
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
	if (!FeedbackData)
	{
		return;
	}

	USoundBase* Loop = FeedbackData->CaptureLoopSFX.LoadSynchronous();
	if (!Loop)
	{
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
