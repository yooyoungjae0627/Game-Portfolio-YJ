#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MosesPlayerController.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

public:
	// ---------------------------
	// Client → Server RPC (Lobby)
	// ---------------------------

	/** UI: “방 만들기” */
	UFUNCTION(Server, Reliable)
	void Server_CreateRoom(int32 MaxPlayers);

	/** UI: “방 참가” */
	UFUNCTION(Server, Reliable)
	void Server_JoinRoom(const FGuid& RoomId);

	/** UI: “방 나가기” */
	UFUNCTION(Server, Reliable)
	void Server_LeaveRoom();

	/** UI: “Start Game” (호스트만) */
	UFUNCTION(Server, Reliable)
	void Server_RequestStartGame();

	// (기존 RPC: Ready/Char 선택은 유지)
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 InId);

	// ---------------------------
	// Dev Exec Helpers (기존 Travel 유지)
	// ---------------------------
	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	UFUNCTION(Exec)
	void TravelToLobby_Exec();

private:
	// ---- Travel 실제 실행은 서버 RPC에서만 ----
	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

	// 서버에서 GM에게 위임하는 실제 함수
	void DoServerTravelToMatch();
	void DoServerTravelToLobby();

	// Lobby Preview Camera
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	FTimerHandle LobbyPreviewCameraTimerHandle;

private:
	void ApplyLobbyPreviewCamera();

	/** "로비 컨텍스트"인지 판단 (단일 맵에서 로비/매치 공존용) */
	bool IsLobbyContext() const;

};
