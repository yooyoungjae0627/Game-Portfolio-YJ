// ============================================================================
// MosesFlagFeedbackData.h (FULL)
// - 캡처 체감(2.5초 경고 / 루프 SFX 볼륨·피치 스케일 / 방송 표시 시간)을 데이터로 분리한다.
// - UI 위젯은 이 DataAsset(또는 UObject)을 SoftRef로 받아 로컬 재생만 수행한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MosesFlagFeedbackData.generated.h"

class USoundBase;

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesFlagFeedbackData : public UObject
{
	GENERATED_BODY()

public:
	// 2.5초 경고(기본 3초 캡처에서 2.5 이상이면 경고)
	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	float WarningThresholdSeconds = 2.5f;

	// 로컬 루프 SFX (캡처 중인 플레이어만)
	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	TSoftObjectPtr<USoundBase> CaptureLoopSFX;

	// 진행도(0..1)에 따른 볼륨/피치 스케일 범위
	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	float VolumeMin = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	float VolumeMax = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	float PitchMin = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	float PitchMax = 1.2f;

	// 방송 기본 표시 시간
	UPROPERTY(EditDefaultsOnly, Category="Flag|Feedback")
	float BroadcastDefaultSeconds = 2.5f;
};
