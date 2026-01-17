#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MosesPlayerController.generated.h"

class AMosesLobbyGameState;
class AMosesLobbyGameMode;
class AMosesStartGameMode;
class AMosesMatchGameMode;
class AMosesPlayerState;
class ACameraActor;

enum class EMosesRoomJoinResult : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyNicknameChanged, const FString&, NewNick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJoinedRoom, EMosesRoomJoinResult, Result, const FGuid&, RoomId);

/**
 * AMosesPlayerController
 *
 * 책임(요약)
 * - UI/입력 이벤트를 서버 권한 로직으로 전달하는 "RPC 진입점"
 * - (로비 컨텍스트에서만) 로비 UI 활성화 + 프리뷰 카메라(ViewTarget) 강제
 *
 * [FIX]
 * - 입력 컴포넌트 강제 생성(CreateInputComponent override) 제거.
 *   입력 바인딩은 Pawn::SetupPlayerInputComponent → HeroComponent 경로에서 확정한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	/*====================================================
	= Debug Exec
	====================================================*/
	UFUNCTION(Exec)
	void gas_DumpTags();

	UFUNCTION(Exec)
	void gas_DumpAttr();

public:
	/*====================================================
	= Dev Exec Helpers
	====================================================*/
	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	UFUNCTION(Exec)
	void TravelToLobby_Exec();

public:
	/*====================================================
	= Client → Server RPC (Lobby)
	====================================================*/
	UFUNCTION(Server, Reliable)
	void Server_CreateRoom(const FString& RoomTitle, int32 MaxPlayers);

	UFUNCTION(Server, Reliable)
	void Server_JoinRoom(const FGuid& RoomId);

	UFUNCTION(Server, Reliable)
	void Server_LeaveRoom();

	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void Server_RequestStartMatch();

	UFUNCTION(Server, Reliable)
	void Server_SetLobbyNickname(const FString& Nick);

	UFUNCTION(Server, Reliable)
	void Server_SendLobbyChat(const FString& Text);

public:
	/*====================================================
	= JoinRoom Result (Server → Client)
	====================================================*/
	UFUNCTION(Client, Reliable)
	void Client_JoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

public:
	/*====================================================
	= Travel Guard (Dev Exec → Server Only)
	====================================================*/
	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

public:
	/*====================================================
	= Enter Lobby / Character Select
	====================================================*/
	UFUNCTION(Server, Reliable)
	void Server_RequestEnterLobby(const FString& Nickname);

	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 SelectedId);

public:
	/*====================================================
	= Local helpers (UI)
	====================================================*/
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPendingLobbyNickname_Local(const FString& Nick);

public:
	/*====================================================
	= Delegates for Blueprint/UI
	====================================================*/
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyNicknameChanged OnLobbyNicknameChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnJoinedRoom OnLobbyJoinedRoom;

protected:
	/*====================================================
	= Lifecycle (Local UI / Camera)
	====================================================*/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;

	/*====================================================
	= Replication
	====================================================*/
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/*====================================================
	= Pending nickname send (Local-only)
	====================================================*/
	void TrySendPendingLobbyNickname_Local();

private:
	/*====================================================
	= Server-side Travel helpers
	====================================================*/
	void DoServerTravelToMatch();
	void DoServerTravelToLobby();

private:
	/*====================================================
	= Lobby Context / Camera / UI (Local-only)
	====================================================*/
	bool IsLobbyContext() const;
	void ApplyLobbyPreviewCamera();

	void ActivateLobbyUI_LocalOnly();
	void RestoreNonLobbyDefaults_LocalOnly();

	void ApplyLobbyInputMode_LocalOnly();
	void RestoreNonLobbyInputMode_LocalOnly();

private:
	/*====================================================
	= Shared getters (null/log guard)
	====================================================*/
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(const TCHAR* Caller) const;

private:
	/*====================================================
	= Lobby Preview Camera policy vars
	====================================================*/
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	FTimerHandle LobbyPreviewCameraTimerHandle;

private:
	/*====================================================
	= Local pending nickname
	====================================================*/
	UPROPERTY(Transient)
	FString PendingLobbyNickname_Local;

	UPROPERTY(Transient)
	bool bPendingLobbyNicknameSend_Local = false;

private:
	/*====================================================
	= Replicated lobby-related state (주의: PS와 중복되면 안 됨)
	====================================================*/
	UPROPERTY(Replicated)
	int32 SelectedCharacterId = INDEX_NONE;

	UPROPERTY(Replicated)
	bool bRep_IsReady = false;
};
