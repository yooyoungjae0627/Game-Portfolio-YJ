#include "UE5_Multi_Shooter/Dialogue/MosesDialogueVoicePlaybackComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"

UMosesDialogueVoicePlaybackComponent::UMosesDialogueVoicePlaybackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesDialogueVoicePlaybackComponent::BeginPlay()
{
	Super::BeginPlay();
	InitIfNeeded();
}

void UMosesDialogueVoicePlaybackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopVoice();
	Super::EndPlay(EndPlayReason);
}

void UMosesDialogueVoicePlaybackComponent::InitIfNeeded()
{
	EnsureAudioComponent();
}

void UMosesDialogueVoicePlaybackComponent::SetPausePolicy(EDialogueVoicePausePolicy InPolicy)
{
	PausePolicy = InPolicy;
}

void UMosesDialogueVoicePlaybackComponent::PlayLine(int32 LineIndex, const FMosesDialogueLine& Line)
{
	InitIfNeeded();

	CurrentLineIndex = LineIndex;
	CurrentVoiceSound = Line.VoiceSound;
	bIsPausedByDialogue = false;

	if (!CurrentVoiceSound)
	{
		StopVoice();
		LogNoVoice(LineIndex);
		return;
	}

	if (!IsAudioReady())
	{
		// Audio 컴포넌트 생성 실패 등
		UE_LOG(LogTemp, Warning, TEXT("[Audio] AudioComponent not ready. line=%d"), LineIndex);
		return;
	}

	VoiceAudioComp->Stop();
	VoiceAudioComp->SetSound(CurrentVoiceSound);
	VoiceAudioComp->Play(0.f);

	LogPlay(LineIndex);
}

void UMosesDialogueVoicePlaybackComponent::StopVoice()
{
	if (VoiceAudioComp)
	{
		VoiceAudioComp->Stop();
		VoiceAudioComp->SetSound(nullptr);
	}
	CurrentVoiceSound = nullptr;
	CurrentLineIndex = INDEX_NONE;
	bIsPausedByDialogue = false;
}

void UMosesDialogueVoicePlaybackComponent::PauseVoice()
{
	if (!IsAudioReady())
	{
		return;
	}

	// 이미 멈춰있어도 정책상 Pause 로그는 찍어도 됨(원하면 조건 추가)
	bIsPausedByDialogue = true;

	if (PausePolicy == EDialogueVoicePausePolicy::PauseResume)
	{
		VoiceAudioComp->SetPaused(true);
	}
	else
	{
		// MVP: Stop해서 Resume 때 재시작
		VoiceAudioComp->Stop();
	}

	LogPause();
}

void UMosesDialogueVoicePlaybackComponent::ResumeVoice()
{
	if (!IsAudioReady())
	{
		return;
	}

	bool bRestarted = false;

	if (!bIsPausedByDialogue)
	{
		// Dialogue pause가 아닌 경우(혹은 이미 resume된 경우) - 그래도 안전하게 처리
	}

	bIsPausedByDialogue = false;

	if (PausePolicy == EDialogueVoicePausePolicy::PauseResume)
	{
		VoiceAudioComp->SetPaused(false);

		// 폴백: SetPaused(false) 했는데도 재생이 안 되는 케이스를 대비(환경/사운드 타입에 따라)
		if (!VoiceAudioComp->IsPlaying())
		{
			RestartCurrentLineIfPossible();
			bRestarted = true;
		}
	}
	else
	{
		// MVP: 무조건 라인 재시작
		RestartCurrentLineIfPossible();
		bRestarted = true;
	}

	LogResume(bRestarted);
}

bool UMosesDialogueVoicePlaybackComponent::IsAudioReady() const
{
	return (VoiceAudioComp != nullptr);
}

void UMosesDialogueVoicePlaybackComponent::EnsureAudioComponent()
{
	if (VoiceAudioComp)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	VoiceAudioComp = NewObject<UAudioComponent>(OwnerActor, TEXT("DialogueVoiceAudioComp"));
	if (!VoiceAudioComp)
	{
		return;
	}

	VoiceAudioComp->bAutoActivate = false;
	VoiceAudioComp->bStopWhenOwnerDestroyed = true;

	// UI 사운드로 취급할지(3D 위치 음성이면 false로 바꾸고 Attach/Location 설정)
	VoiceAudioComp->bIsUISound = bTreatAsUISound;

	VoiceAudioComp->RegisterComponent();
	VoiceAudioComp->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
}

void UMosesDialogueVoicePlaybackComponent::RestartCurrentLineIfPossible()
{
	if (!CurrentVoiceSound || CurrentLineIndex == INDEX_NONE || !IsAudioReady())
	{
		return;
	}

	VoiceAudioComp->Stop();
	VoiceAudioComp->SetSound(CurrentVoiceSound);
	VoiceAudioComp->Play(0.f);
}

void UMosesDialogueVoicePlaybackComponent::LogPlay(int32 LineIndex) const
{
	UE_LOG(LogTemp, Log, TEXT("[Audio] Play voice line=%d"), LineIndex);
}

void UMosesDialogueVoicePlaybackComponent::LogPause() const
{
	UE_LOG(LogTemp, Log, TEXT("[Audio] Pause"));
}

void UMosesDialogueVoicePlaybackComponent::LogResume(bool bRestarted) const
{
	if (bRestarted)
	{
		UE_LOG(LogTemp, Log, TEXT("[Audio] Restart line"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Audio] Resume"));
	}
}

void UMosesDialogueVoicePlaybackComponent::LogNoVoice(int32 LineIndex) const
{
	UE_LOG(LogTemp, Log, TEXT("[Audio] No voice sound line=%d"), LineIndex);
}
