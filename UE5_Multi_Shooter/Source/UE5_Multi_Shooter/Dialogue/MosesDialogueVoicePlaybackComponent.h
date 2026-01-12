#pragma once

#include "Components/ActorComponent.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
#include "MosesDialogueVoicePlaybackComponent.generated.h"

class UAudioComponent;

UENUM(BlueprintType)
enum class EDialogueVoicePausePolicy : uint8
{
	PauseResume UMETA(DisplayName="Pause/Resume (Try Continue)"),
	RestartOnResume UMETA(DisplayName="MVP: Restart Line On Resume"),
};

UCLASS(ClassGroup=(Dialogue), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesDialogueVoicePlaybackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesDialogueVoicePlaybackComponent();

	void InitIfNeeded();

	// 라인 진입 시 호출(자막 라인과 동일한 라인의 VoiceSound 재생)
	void PlayLine(int32 LineIndex, const FMosesDialogueLine& Line);

	// 대화 종료/스킵/강제 정리
	void StopVoice();

	// Pause/Resume
	void PauseVoice();
	void ResumeVoice();

	// 정책 설정(옵션 A/B)
	void SetPausePolicy(EDialogueVoicePausePolicy InPolicy);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 내부 헬퍼
	bool IsAudioReady() const;
	void EnsureAudioComponent();
	void RestartCurrentLineIfPossible();
	void LogPlay(int32 LineIndex) const;
	void LogPause() const;
	void LogResume(bool bRestarted) const;
	void LogNoVoice(int32 LineIndex) const;

private:
	// === State ===
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudioComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CurrentVoiceSound = nullptr;

	int32 CurrentLineIndex = INDEX_NONE;

	bool bIsPausedByDialogue = false;

	// === Config ===
	UPROPERTY(EditAnywhere, Category="Dialogue|Voice")
	EDialogueVoicePausePolicy PausePolicy = EDialogueVoicePausePolicy::RestartOnResume;

	// UI 사운드로 처리할지(원치 않으면 false)
	UPROPERTY(EditAnywhere, Category="Dialogue|Voice")
	bool bTreatAsUISound = true;
};
