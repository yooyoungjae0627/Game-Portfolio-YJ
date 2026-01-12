#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesLobbyVoiceSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVoiceLevelChanged, float /*Level01*/);

/**
 * ULobbyVoiceSubsystem
 * - 로컬 전용: 10~20Hz 타이머로 VoiceLevel(0~1) 샘플링
 * - Day11 MVP: 더미 입력으로도 OK(파이프 증명)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyVoiceSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	void SetMicEnabled(bool bEnable);
	bool IsMicEnabled() const { return bMicEnabled; }

	float GetVoiceLevel01() const { return VoiceLevel01; }
	FOnVoiceLevelChanged& OnVoiceLevelChanged() { return VoiceLevelChanged; }

private:
	void StartSampler();
	void StopSampler();
	void SampleVoiceLevel();

	float ComputeVoiceLevel01_FallbackDummy() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Voice")
	float SampleIntervalSec = 0.05f; // 20Hz

	UPROPERTY(EditDefaultsOnly, Category = "Voice")
	bool bUseDummyInput = true;

	FTimerHandle SampleTimerHandle;
	bool bMicEnabled = false;
	float VoiceLevel01 = 0.f;

	FOnVoiceLevelChanged VoiceLevelChanged;
};
