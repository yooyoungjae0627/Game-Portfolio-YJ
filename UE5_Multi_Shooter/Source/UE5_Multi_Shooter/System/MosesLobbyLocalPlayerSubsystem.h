// MosesLobbyLocalPlayerSubsystem.h

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyViewTypes.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"

#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;
class ALobbyPreviewActor;
class ACameraActor;
class AMosesPlayerController;
class AMosesPlayerState;
class UMoseseGestureDeckComponent;

struct FDialogueNetState;

enum class EGamePhase : uint8;
enum class EMosesRoomJoinResult : uint8;

UENUM(BlueprintType)
enum class EMosesMicState : uint8
{
	Off,
	Listening,
	Processing,
	Error
};

DECLARE_MULTICAST_DELEGATE(FOnLobbyPlayerStateChanged);
DECLARE_MULTICAST_DELEGATE(FOnLobbyRoomStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLobbyViewModeChanged, ELobbyViewMode /*NewMode*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRulesViewModeChanged, bool /*bEnable*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyJoinRoomResult, EMosesRoomJoinResult /*Result*/, const FGuid& /*RoomId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLobbyDialogueStateChanged, const FDialogueNetState& /*NewState*/);

/**
 * UMosesLobbyLocalPlayerSubsystem
 *
 * 역할:
 * - 클라 전용 연출 실행자
 * - 로비 화면 "보기 모드" 전환(규칙 보기 등)을 책임진다.
 * - UI는 상태만 받아서 보여주기/숨기기만 한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// ---------------------------
	// Dialogue Command (Input pipe)
	// ---------------------------
	void SubmitCommandText(const FString& Text);

	// ---------------------------
	// UI Only: Rules View (Public)
	// ---------------------------
	// ✅ 사용자 트리거 진입: Greeting 1회 포함
	void EnterRulesView_UIOnly();

	// ✅ 서버/위젯 복구용: Greeting 없이 상태/카메라/모드만 복구
	void RecoverRulesView_UIOnly(bool bEnable);

	void ExitRulesView_UIOnly();

	// ---------------------------
	// LobbyWidget registration
	// ---------------------------
	void SetLobbyWidget(UMosesLobbyWidget* InWidget);

	void RequestEnterDialogueRetry_NextTick();
	void ClearEnterDialogueRetry();

	// ---------------------------
	// UI control (Public API)
	// ---------------------------
	void ActivateLobbyUI();
	void DeactivateLobbyUI();

	// ---------------------------
	// Notify (PS/GS -> Subsystem)
	// ---------------------------
	void NotifyPlayerStateChanged();
	void NotifyRoomStateChanged();
	void NotifyJoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

	// ---------------------------
	// Events (Widget bind)
	// ---------------------------
	FOnLobbyPlayerStateChanged& OnLobbyPlayerStateChanged() { return LobbyPlayerStateChangedEvent; }
	FOnLobbyRoomStateChanged& OnLobbyRoomStateChanged() { return LobbyRoomStateChangedEvent; }
	FOnLobbyJoinRoomResult& OnLobbyJoinRoomResult() { return LobbyJoinRoomResultEvent; }

	// ---------------------------
	// Lobby Preview (Public API - Widget에서 호출)
	// ---------------------------
	void RequestNextCharacterPreview_LocalOnly();

	ELobbyViewMode GetLobbyViewMode() const { return LobbyViewMode; }

	// ---------------------------
	// UI -> Server Phase Request
	// ---------------------------
	void RequestEnterLobbyDialogue();
	void RequestExitLobbyDialogue();

	FOnRulesViewModeChanged& OnRulesViewModeChanged() { return RulesViewModeChangedEvent; }

	FOnLobbyViewModeChanged OnLobbyViewModeChanged;

	FOnLobbyDialogueStateChanged& OnLobbyDialogueStateChanged() { return LobbyDialogueStateChanged; }

	const FDialogueNetState& GetLastDialogueNetState() const { return LastDialogueNetState; }

	void NotifyDialogueStateChanged(const FDialogueNetState& NewState);

	void RequestSetLobbyNickname_LocalOnly(const FString& Nick);

	// ✅ Widget이 즉시 갱신할 수 있게 Getter 제공(옵션)
	const FText& GetCachedMySpeechText() const { return CachedMySpeechText; }
	EMosesMicState GetMicState() const { return MicState; }

	// =========================================================
	// Answer Trigger API (테스트 & Day5 확장용)
	// - 테스트 설명서의 "답변 시작 시 제스처 1회"를 강제로 확인할 수 있게 함
	// - Day5에서 Q/A 트리 붙이면 이 함수를 통해 UI+제스처만 일관되게 처리
	// =========================================================
	void SetAnswer(int32 AnswerIndex, const FText& AnswerText);

private:
	// ---------------------------
	// Lobby UI Refresh (핵심 파이프)
	// ---------------------------
	void RefreshLobbyUI_FromCurrentState();
	void RequestBindLobbyGameStateEventsRetry_NextTick();
	void ClearBindLobbyGameStateEventsRetry();

	// ✅ 공통 진입/복구 루틴 (Greeting 여부 플래그)
	void EnterOrRecoverRulesView_UIOnly(bool bEnable, bool bPlayGreeting);

	void ApplySessionToWidget();              // Bottom/MicState 반영
	void ApplyGreetingIfNeeded();             // Greeting 1회
	void ClearDialogueUITexts();              // Exit 시 Clear

	// ---------------------------
	// Internal helpers
	// ---------------------------
	bool IsLobbyWidgetValid() const;

	// ---------------------------
	// Lobby Preview (Local only)
	// ---------------------------
	void RefreshLobbyPreviewCharacter_LocalOnly();
	void ApplyPreviewBySelectedId_LocalOnly(int32 SelectedId);
	ALobbyPreviewActor* GetOrFindLobbyPreviewActor_LocalOnly();

	// ---------------------------
	// Player access helper
	AMosesPlayerController* GetMosesPC_LocalOnly() const;
	AMosesPlayerState* GetMosesPS_LocalOnly() const;

	// ---------------------------
	// Internal
	void SetLobbyViewMode(ELobbyViewMode NewMode);

	// ---------------------------
	// Camera helpers
	ACameraActor* FindCameraByTag(const FName& CameraTag) const;
	void SetViewTargetToCameraTag(const FName& CameraTag, float BlendTime);

	void SwitchToCamera(ACameraActor* TargetCam, const TCHAR* LogFromTo);
	void ApplyRulesViewToWidget(bool bEnable);

	APlayerController* GetLocalPC() const;

	// ---------------------------
	// Retry control (Preview refresh)
	void RequestPreviewRefreshRetry_NextTick();
	void ClearPreviewRefreshRetry();

	// ---------------------------
	// Bind GS events (LateJoin recover)
	void BindLobbyGameStateEvents();
	void UnbindLobbyGameStateEvents();

	class AMosesLobbyGameState* GetLobbyGameState() const;

	void HandlePhaseChanged(EGamePhase NewPhase);
	void HandleDialogueStateChanged(const FDialogueNetState& NewState);

	void TrySendPendingLobbyNickname_Local();

	UFUNCTION()
	void HandleGameStateDialogueChanged(const FDialogueNetState& NewState);

	// =========================================================
	// MetaHuman Gesture (Local Only)
	// - 로컬 화면에서만 메타휴먼 제스처를 재생한다.
	// - 다른 플레이어에게 영향이 없도록 "레벨에 배치된 NPC"를 Tag로 찾는다.
	// =========================================================
	AActor* GetOrFindRulesMetaHumanActor_LocalOnly();
	void PlayRulesMetaHumanGesture_LocalOnly();
	void StopRulesMetaHumanGesture_LocalOnly();


private:
	FOnRulesViewModeChanged RulesViewModeChangedEvent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class AMosesLobbyGameState> CachedLobbyGS;

	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyWidget> LobbyWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ALobbyPreviewActor> CachedLobbyPreviewActor = nullptr;

	FOnLobbyPlayerStateChanged LobbyPlayerStateChangedEvent;
	FOnLobbyRoomStateChanged LobbyRoomStateChangedEvent;
	FOnLobbyJoinRoomResult LobbyJoinRoomResultEvent;

	FOnLobbyDialogueStateChanged LobbyDialogueStateChanged;
	FDialogueNetState LastDialogueNetState;

	ELobbyViewMode LobbyViewMode = ELobbyViewMode::Default;

	int32 LocalPreviewSelectedId = 1;

	bool bPendingSelectedIdServerAck = false;
	int32 PendingSelectedIdServerAck = INDEX_NONE;

	// ✅ RulesView 모드 플래그
	bool bInRulesView = false;

	bool bBoundToGameState = false;
	bool bLoggedPreviewWaitOnce = false;
	bool bLoggedBindWaitOnce = false;

	uint16 NextClientCommandSeq = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName DialogueCameraTag = TEXT("DialogueCamera_LobbyNPC");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float CameraBlendTime = 0.35f;

	FTimerHandle PreviewRefreshRetryHandle;
	FTimerHandle EnterDialogueRetryHandle;
	FTimerHandle BindLobbyGSRetryHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue|Command")
	TObjectPtr<class UCommandLexiconDA> CommandLexicon = nullptr;

	FString PendingLobbyNickname_Local;
	bool bPendingLobbyNicknameSend_Local = false;
	bool bLoginSubmitted_Local = false;

	// ✅ Greeting 1회 제어
	bool bGreetingShownThisEntry = false;

	FText GreetingText = FText::FromString(TEXT("환영해, 나는 너에게 게임 규칙에 대해서 설명을 하려고 해. 무엇이든 물어봐. 대답해줄 수 있는 부분은 대화해줄게."));

	// ✅ 하단 내 발화 / 마이크 상태 (표시만)
	FText CachedMySpeechText = FText::GetEmpty();
	EMosesMicState MicState = EMosesMicState::Off;

	// 레벨 배치된 Rules NPC 식별 Tag (에디터에서 Actor Tag로 설정)
	UPROPERTY(EditDefaultsOnly, Category = "RulesView|MetaHuman")
	FName RulesMetaHumanActorTag = TEXT("RulesMetaHuman");

	// 캐시(매번 GetAllActorsWithTag 비용 줄이기)
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedRulesMetaHumanActor;

	// 답변 시작 감지용(LineIndex 변화 감시)
	int32 CachedLastGestureLineIndex = INDEX_NONE;
};
