// ============================================================================
// MosesFlagFeedbackData.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// 목적
// - 캡처 체감(2.5초 경고 / 루프 SFX 볼륨·피치 스케일 / 방송 표시 시간)을 데이터로 분리.
// - "오늘 목표"인 Flag Capture 완결에서 BroadcastDuration(기본 4초)을 Data로 통일.
//
// [MOD]
// - UObject -> UDataAsset 로 변경하여 에디터에서 DataAsset 생성 가능하게 함.
// - GF_Combat_Data 아래에서 DA_FlagFeedback_Default 같은 에셋으로 관리한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MosesFlagFeedbackData.generated.h"

class USoundBase;

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesFlagFeedbackData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 2.5초 경고(기본 3초 캡처에서 2.5 이상이면 경고)
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float WarningThresholdSeconds = 2.5f;

	// 로컬 루프 SFX (캡처 중인 플레이어만)
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	TSoftObjectPtr<USoundBase> CaptureLoopSFX;

	// 진행도(0..1)에 따른 볼륨/피치 스케일 범위
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float VolumeMin = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float VolumeMax = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float PitchMin = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float PitchMax = 1.2f;

	// [MOD] 방송 기본 표시 시간 (오늘 정책: 4초)
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float BroadcastDefaultSeconds = 4.0f;
};
