#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MosesPlayerController.generated.h"

class AMosesLobbyGameState;
class AMosesPlayerState;
class ACameraActor;

enum class EMosesRoomJoinResult : uint8;

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
 * - StartGame 최종 검증 + Travel 트리거: AMosesLobbyGameMode
 *
 * 안전장치
 * - LocalController만 UI/카메라를 조작 (다른 PC/리슨서버 오염 방지)
 * - SeamlessTravel로 PC가 유지되면 bAutoManageActiveCameraTarget 같은 플래그가 "다음 맵에도 남는다"
 *   → 로비가 아닐 때는 반드시 원복해줘야 한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 기본 생성자: 프로젝트 카메라 매니저 지정 */
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------------------------
	// Dev Exec Helpers (디버그용)
	// ---------------------------

	/** 디버그: 매치로 이동 (서버=즉시, 클라=서버 RPC 우회) */
	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	/** 디버그: 로비로 이동 (서버=즉시, 클라=서버 RPC 우회) */
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

	UFUNCTION(Server, Reliable)
	void Server_RequestStartGame();

	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 InId);

	// ---------------------------
	// Client ↔ Server (Login)
	// ---------------------------

	UFUNCTION(Server, Reliable)
	void Server_RequestLogin(const FString& InId, const FString& InPw);

	UFUNCTION(Client, Reliable)
	void Client_LoginResult(bool bOk, const FString& Message);

	// ---------------------------
	// Room Chat
	// ---------------------------

	UFUNCTION(Server, Reliable)
	void Server_SendRoomChat(const FString& Text);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveRoomChat(const FString& FromName, const FString& Text);


	/** Character Select Confirm: 서버에 선택 확정 요청 */
	UFUNCTION(Server, Reliable)
	void Server_RequestSelectCharacter(const FName CharacterId);

	// ---------------------------
	// JoinRoom Result
	// ---------------------------
	UFUNCTION(Client, Reliable)
	void Client_JoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

protected:
	// ---------------------------
	// Lifecycle (Local UI / Camera)
	// ---------------------------

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	// ---------------------------
	// Travel은 서버에서만 실행되도록 방어
	// ---------------------------

	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

	void DoServerTravelToMatch();
	void DoServerTravelToLobby();

	// ---------------------------
	// Lobby Context / Camera
	// ---------------------------

	/** 로비 컨텍스트 판별 게이트 (로비 UI/카메라 오염 방지) */
	bool IsLobbyContext() const;

	/** 로비 프리뷰 카메라 적용(즉시 1회 + 지연 1회 재적용 가능) */
	void ApplyLobbyPreviewCamera();

	/** (로컬 전용) 로비 UI 활성화: LocalPlayerSubsystem에 위임 */
	void ActivateLobbyUI_LocalOnly();

	/** SeamlessTravel 대비: 로비가 아니면 "로비에서 꺼둔 플래그/타이머"를 원복 */
	void RestoreNonLobbyDefaults_LocalOnly();

	/** 공통 레퍼런스 획득(서버 RPC에서 반복되는 null 체크 제거 목적) */
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(const TCHAR* Caller) const;

	// ---------------------------
	// Lobby Input / Cursor policy
	// ---------------------------
	void ApplyLobbyInputMode_LocalOnly();
	void RestoreNonLobbyInputMode_LocalOnly();

private:
	// ---------------------------
	// Lobby Preview Camera 정책 변수들 (변수는 아래)
	// ---------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	FTimerHandle LobbyPreviewCameraTimerHandle;
};
