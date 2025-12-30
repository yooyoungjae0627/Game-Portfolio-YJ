#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MosesPlayerController.generated.h"

/**
 * AMosesPlayerController
 *
 * 책임:
 * - 클라이언트 입력/UI 이벤트를 "서버 권한 로직"으로 전달하는 RPC 진입점
 * - 로비 컨텍스트에서만: 로비 UI 활성화 + 프리뷰 카메라 ViewTarget 강제
 * - Dev/디버그 목적: Exec 커맨드를 서버 RPC로 우회하여 Travel 테스트 가능
 *
 * 경계:
 * - 실제 룸 데이터/상태 변경은 LobbyGameState가 Source of Truth
 * - StartGame 판정/Travel 트리거는 LobbyGameMode에서 최종 검증 후 수행
 *
 * 주의:
 * - LocalController만 UI/카메라를 만진다(리슨서버/관전/다른 PC 오염 방지)
 * - SeamlessTravel/Experience 흐름에서 ViewTarget이 여러 번 덮일 수 있어 재적용 로직이 있다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** 로비 진입 시 UI/카메라 세팅, 로컬만 수행 */
	virtual void BeginPlay() override;

	/** Possess 직후 ViewTarget이 Pawn으로 덮이는 케이스 방지(로비에서만 재적용) */
	virtual void OnPossess(APawn* InPawn) override;

public:
	// ---------------------------
	// Client → Server RPC (Lobby)
	// ---------------------------

	/** UI: CreateRoom 요청(서버에서 MaxPlayers Clamp 후 처리) */
	UFUNCTION(Server, Reliable)
	void Server_CreateRoom(int32 MaxPlayers);

	/** UI: JoinRoom 요청(룸ID 유효성은 서버에서 재검증) */
	UFUNCTION(Server, Reliable)
	void Server_JoinRoom(const FGuid& RoomId);

	/** UI: LeaveRoom 요청(Disconnect 처리와 동일하게 서버에서 정리) */
	UFUNCTION(Server, Reliable)
	void Server_LeaveRoom();

	/** UI: StartGame 요청(서버 GM에서 Host/Ready/Full 최종 검증) */
	UFUNCTION(Server, Reliable)
	void Server_RequestStartGame();

	/** UI: Ready 토글(PS->RoomReadyMap 동기화 포함) */
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	/** UI: 캐릭터 선택(PS에 저장/복제) */
	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 InId);

	// ---------------------------
	// Dev Exec Helpers
	// ---------------------------
	/** 디버그: 매치 이동(서버에서만 실제 Travel, 클라는 서버 RPC로 우회) */
	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	/** 디버그: 로비 이동(서버에서만 실제 Travel, 클라는 서버 RPC로 우회) */
	UFUNCTION(Exec)
	void TravelToLobby_Exec();

private:
	// ---- Travel은 서버에서만 실행되도록 방어 ----

	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

	/** 서버에서 AuthGameMode를 확인하고 올바른 GM에 위임 */
	void DoServerTravelToMatch();
	void DoServerTravelToLobby();

	/**
	 * "로비 컨텍스트" 판별
	 * - 단일 맵 운용/Phase 기반 운용에서도 로비 UI/카메라를 오염시키지 않기 위한 게이트
	 * - 현재 정책: LobbyGameState 존재 여부로 판단
	 */
	bool IsLobbyContext() const;

	/**
	 * 로비 프리뷰 카메라 적용
	 * - Experience/Possess/CameraManager가 ViewTarget을 덮는 케이스 대응으로
	 *   BeginPlay에서 즉시 1회 + 타이머로 1회 재적용할 수 있다.
	 */
	void ApplyLobbyPreviewCamera();

private:
	// Lobby Preview Camera 정책(레벨에 Tag로 지정된 CameraActor 사용)

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	/** ViewTarget 덮임 방지용 재적용 딜레이 */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	FTimerHandle LobbyPreviewCameraTimerHandle;
};
