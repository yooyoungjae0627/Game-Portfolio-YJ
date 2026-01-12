#include "MosesLobbyVoiceSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMosesLobbyVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMosesLobbyVoiceSubsystem::Deinitialize()
{
	StopSampler();
	Super::Deinitialize();
}

void UMosesLobbyVoiceSubsystem::SetMicEnabled(bool bEnable)
{
	if (bMicEnabled == bEnable) return;
	bMicEnabled = bEnable;

	if (bMicEnabled) StartSampler();
	else
	{
		StopSampler();
		VoiceLevel01 = 0.f;
		VoiceLevelChanged.Broadcast(VoiceLevel01);
	}
}

void UMosesLobbyVoiceSubsystem::StartSampler()
{
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(
		SampleTimerHandle, this, &UMosesLobbyVoiceSubsystem::SampleVoiceLevel,
		SampleIntervalSec, true
	);
}

void UMosesLobbyVoiceSubsystem::StopSampler()
{
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().ClearTimer(SampleTimerHandle);
}

void UMosesLobbyVoiceSubsystem::SampleVoiceLevel()
{
	const float NewLevel = bUseDummyInput ? ComputeVoiceLevel01_FallbackDummy() : 0.f;
	VoiceLevel01 = FMath::Clamp(NewLevel, 0.f, 1.f);

	UE_LOG(LogTemp, Log, TEXT("[Voice] level=%.2f hz=%.0f"),
		VoiceLevel01, 1.f / FMath::Max(0.001f, SampleIntervalSec));

	VoiceLevelChanged.Broadcast(VoiceLevel01);
}

float UMosesLobbyVoiceSubsystem::ComputeVoiceLevel01_FallbackDummy() const
{
	const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return 0.5f + 0.5f * FMath::Sin(T * 7.f);
}
