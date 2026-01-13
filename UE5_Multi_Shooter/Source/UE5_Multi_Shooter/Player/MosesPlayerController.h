#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MosesPlayerController.generated.h"

class AMosesLobbyGameState;
class AMosesLobbyGameMode;
class AMosesPlayerState;
class ACameraActor;

enum class EMosesRoomJoinResult : uint8;
enum class EDialogueCommandType : uint8;
enum class EDialogueFlowState : uint8;
enum class EGamePhase : uint8;

UENUM()
enum class ELobbyDialogueEntryType : uint8
{
	RulesView,      // 방 무관
	InRoomDialogue  // 방 필요
};

/**
 * AMosesPlayerController
 *
 * 역할(요약)
 * - UI/입력 이벤트를 서버 권한 로직으로 전달하는 "RPC 진입점"
 * - (로비 컨텍스트에서만) 로비 UI 활성화 + 프리뷰 카메라(ViewTarget) 강제
 * - 디버그: Exec 커맨드로 Travel 테스트 (클라는 서버 RPC로 우회)
 *
 * 단일 진실(Single Source of Truth)
 * - 룸 데이터/상태 변경: AMosesLobbyGameState
 * - StartMatch 최종 검증 + Travel 트리거: AMosesLobbyGameMode
 *
 * 안전장치
 * - LocalController만 UI/카메라를 조작 (다른 PC/리슨서버 오염 방지)
 * - SeamlessTravel로 PC가 유지되면 로비 카메라/입력 플래그가 다음 맵까지 남을 수 있음 → 로비가 아니면 원복
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------------------------
	// Dev Exec Helpers
	// ---------------------------

	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	UFUNCTION(Exec)
	void TravelToLobby_Exec();

	// ---------------------------
	// Client → Server RPC (Lobby)
	// ---------------------------

	UFUNCTION(Server, Reliable)
	void Server_CreateRoom(const FString& RoomTitle, int32 MaxPlayers);

	UFUNCTION(Server, Reliable)
	void Server_JoinRoom(const FGuid& RoomId);

	UFUNCTION(Server, Reliable)
	void Server_LeaveRoom();

	/** Ready는 SetReady만 남긴다. 토글은 UI에서 bool로 만들어 호출 */
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	/** 방장만 StartMatch 요청 가능. 최종 판정은 GM */
	UFUNCTION(Server, Reliable)
	void Server_RequestStartMatch();

	UFUNCTION(Server, Reliable)
	void Server_SetLobbyNickname(const FString& Nick);


	UFUNCTION(Server, Reliable)
	void Server_SendLobbyChat(const FString& Text);

	// ---------------------------
	// JoinRoom Result (Server → Client)
	// ---------------------------

	UFUNCTION(Client, Reliable)
	void Client_JoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

	// ---------------------------
	// Dialogue Gate (Client → Server)
	// ---------------------------

	UFUNCTION(Server, Reliable)
	void Server_RequestEnterLobbyDialogue(ELobbyDialogueEntryType EntryType);

	UFUNCTION(Server, Reliable)
	void Server_RequestExitLobbyDialogue();

	UFUNCTION(Server, Unreliable)
	void Server_SubmitDialogueCommand(EDialogueCommandType Type, uint16 ClientCommandSeq);

	UFUNCTION(Server, Reliable)
	void Server_DialogueAdvanceLine();

	UFUNCTION(Server, Reliable)
	void Server_DialogueSetFlowState(EDialogueFlowState NewState);

	// ---------------------------
	// Travel Guard (Dev Exec → Server Only)
	// ---------------------------

	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

	UFUNCTION(Server, Reliable)
	void Server_RequestEnterLobby(const FString& Nickname);

	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 SelectedId);


	void SetPendingLobbyNickname_Local(const FString& Nick);

protected:
	// ---------------------------
	// Lifecycle (Local UI / Camera)
	// ---------------------------

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;

private:
	void TrySendPendingLobbyNickname_Local();

	void DoServerTravelToMatch();
	void DoServerTravelToLobby();

	// ---------------------------
	// Lobby Context / Camera / UI
	// ---------------------------

	bool IsLobbyContext() const;
	void ApplyLobbyPreviewCamera();

	void ActivateLobbyUI_LocalOnly();
	void RestoreNonLobbyDefaults_LocalOnly();

	void ApplyLobbyInputMode_LocalOnly();
	void RestoreNonLobbyInputMode_LocalOnly();

	// ---------------------------
	// Shared getters (null/log guard)
	// ---------------------------

	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(const TCHAR* Caller) const;

private:
	// ---------------------------
	// Lobby Preview Camera 정책 변수들
	// ---------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	FTimerHandle LobbyPreviewCameraTimerHandle;

	FString PendingLobbyNickname_Local;
	bool bPendingLobbyNicknameSend_Local = false;
};
