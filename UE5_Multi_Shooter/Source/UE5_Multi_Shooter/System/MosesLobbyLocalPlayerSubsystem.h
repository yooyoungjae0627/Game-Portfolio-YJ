// MosesLobbyLocalPlayerSubsystem.h

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyViewTypes.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"

#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;
class ALobbyPreviewActor;
class ACameraActor;

struct FDialogueNetState;

enum class EGamePhase : uint8;
enum class EMosesRoomJoinResult : uint8;

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
	// ---------------------------
	// Engine
	// ---------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// ---------------------------
	// Dialogue Command (Input pipe)
	// ---------------------------
	/**
	 * SubmitCommandText
	 * - 디버그 텍스트 입력/향후 STT 입력을 하나의 파이프로 통합
	 * - DetectIntent -> PC RPC(Server_SubmitDialogueCommand)
	 */
	void SubmitCommandText(const FString& Text);

	// ---------------------------
	// UI Only: Rules View
	// ---------------------------
	/** [게임 진행 방법] 버튼: 로컬 UI/카메라만 전환 */
	void EnterRulesView_UIOnly();

	/** [나가기] 버튼: 로컬 UI/카메라만 원복 */
	void ExitRulesView_UIOnly();

	// ---------------------------
	// LobbyWidget registration
	// ---------------------------
	/**
	 * LobbyWidget 인스턴스를 Subsystem에 알려줘야
	 * Subsystem이 UI모드 전환을 호출할 수 있다.
	 */
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

	// ✅ NEW: Join 결과(성공/실패)를 UI로 전달
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

	// UI(Widget)는 이 이벤트만 보고 패널/버블 Visible만 전환한다.
	FOnRulesViewModeChanged& OnRulesViewModeChanged() { return RulesViewModeChangedEvent; }

	// UI가 구독하는 이벤트
	FOnLobbyViewModeChanged OnLobbyViewModeChanged;

	// LobbyWidget 구독용
	FOnLobbyDialogueStateChanged& OnLobbyDialogueStateChanged() { return LobbyDialogueStateChanged; }

	// late join / 위젯 재생성 복구용
	const FDialogueNetState& GetLastDialogueNetState() const { return LastDialogueNetState; }

	// GameState 이벤트 수신/브로드캐스트
	void NotifyDialogueStateChanged(const FDialogueNetState& NewState);

	// ✅ StartGame/Lobby 어디서든 호출 가능: 닉네임 Pending 저장 + 전송 보장
	void RequestSetLobbyNickname_LocalOnly(const FString& Nick);


private:
	// ---------------------------
	// Lobby UI Refresh (핵심 파이프)
	// ---------------------------
	/** Room/Chat/PlayerState 변경 등 어떤 이유로든 로비 UI를 한 번 갱신하는 단일 진입점 */
	void RefreshLobbyUI_FromCurrentState();

	/** GameState 바인딩이 너무 이른 경우를 위한 NextTick 재시도 */
	void RequestBindLobbyGameStateEventsRetry_NextTick();
	void ClearBindLobbyGameStateEventsRetry();

private:
	FOnRulesViewModeChanged RulesViewModeChangedEvent;

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
	// ---------------------------
	class AMosesPlayerController* GetMosesPC_LocalOnly() const;
	class AMosesPlayerState* GetMosesPS_LocalOnly() const;

	// ---------------------------
	// Internal
	// ---------------------------
	void SetLobbyViewMode(ELobbyViewMode NewMode);

	// ---------------------------
	// Camera helpers
	// ---------------------------
	ACameraActor* FindCameraByTag(const FName& CameraTag) const;
	void SetViewTargetToCameraTag(const FName& CameraTag, float BlendTime);

	void SwitchToCamera(ACameraActor* TargetCam, const TCHAR* LogFromTo);
	void ApplyRulesViewToWidget(bool bEnable);

	APlayerController* GetLocalPC() const;

	// ---------------------------
	// Retry control (Preview refresh)
	// ---------------------------
	void RequestPreviewRefreshRetry_NextTick();
	void ClearPreviewRefreshRetry();

	// ---------------------------
	// Bind GS events (LateJoin recover)
	// ---------------------------
	void BindLobbyGameStateEvents();
	void UnbindLobbyGameStateEvents();

	class AMosesLobbyGameState* GetLobbyGameState() const;

	void HandlePhaseChanged(EGamePhase NewPhase);
	void HandleDialogueStateChanged(const FDialogueNetState& NewState);

	void TrySendPendingLobbyNickname_Local();

	UFUNCTION()
	void HandleGameStateDialogueChanged(const FDialogueNetState& NewState);

private:
	// ---------------------------
	// Cached pointers
	// ---------------------------
	UPROPERTY(Transient)
	TWeakObjectPtr<class AMosesLobbyGameState> CachedLobbyGS;

	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyWidget> LobbyWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ALobbyPreviewActor> CachedLobbyPreviewActor = nullptr;

	// ---------------------------
	// Events
	// ---------------------------
	FOnLobbyPlayerStateChanged LobbyPlayerStateChangedEvent;
	FOnLobbyRoomStateChanged LobbyRoomStateChangedEvent;
	FOnLobbyJoinRoomResult LobbyJoinRoomResultEvent;

	FOnLobbyDialogueStateChanged LobbyDialogueStateChanged;
	FDialogueNetState LastDialogueNetState;

	// ---------------------------
	// State
	// ---------------------------
	ELobbyViewMode LobbyViewMode = ELobbyViewMode::Default;

	// 개발자 주석: "프리뷰"는 로컬 연출이므로 서버/복제 없이 Local에서만 유지한다.
	int32 LocalPreviewSelectedId = 1;

	// 현재 모드 (왕복 10회 꼬임 방지)
	bool bInRulesView = false;

	bool bBoundToGameState = false;

	/** CharPreview WAIT 로그를 1회만 찍기 위한 플래그 (스팸 방지) */
	bool bLoggedPreviewWaitOnce = false;

	/** GS Bind WAIT 로그를 1회만 찍기 위한 플래그 (스팸 방지) */
	bool bLoggedBindWaitOnce = false;

	// 클라 명령 시퀀스(서버 Gate의 Duplicate/Old 방지용)
	uint16 NextClientCommandSeq = 1;

	// ---------------------------
	// Camera
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName DialogueCameraTag = TEXT("DialogueCamera_LobbyNPC");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float CameraBlendTime = 0.35f;

	// ---------------------------
	// Timers
	// ---------------------------
	FTimerHandle PreviewRefreshRetryHandle;
	FTimerHandle EnterDialogueRetryHandle;

	FTimerHandle BindLobbyGSRetryHandle;

	// ---------------------------
	// Dialogue
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue|Command")
	TObjectPtr<class UCommandLexiconDA> CommandLexicon = nullptr;

	// Pending Nick (Local only)
	FString PendingLobbyNickname_Local;
	bool bPendingLobbyNicknameSend_Local = false;

	// ✅ 로그인 버튼을 실제로 눌렀는지 (의도)
	bool bLoginSubmitted_Local = false;
};
