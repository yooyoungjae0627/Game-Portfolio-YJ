// ============================================================================
// MosesFlagFeedbackData.h (FULL)
// ----------------------------------------------------------------------------
// 목적
// - 캡처 피드백(경고 임계치/루프 SFX/볼륨·피치 범위/방송 시간)을 데이터로 분리한다.
// - "방송 표시 시간 4초" 같은 기획값을 코드에서 하드코딩하지 않게 한다.
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
	/** 3초 캡처 기준, 2.5 이상 진행되면 경고(선택 기능) */
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float WarningThresholdSeconds = 2.5f;

	/** 로컬 루프 SFX (캡처 중인 플레이어만) */
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	TSoftObjectPtr<USoundBase> CaptureLoopSFX;

	/** 진행도(0..1)에 따른 볼륨/피치 범위 */
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float VolumeMin = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float VolumeMax = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float PitchMin = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float PitchMax = 1.2f;

	/** 방송 기본 표시 시간(기획: 4초) */
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	float BroadcastDefaultSeconds = 4.0f;
};
