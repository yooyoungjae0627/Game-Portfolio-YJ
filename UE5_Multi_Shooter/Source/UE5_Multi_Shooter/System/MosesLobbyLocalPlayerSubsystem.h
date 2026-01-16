// ============================================================================
// MosesLobbyLocalPlayerSubsystem.h
// ============================================================================

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;
class ALobbyPreviewActor;
class ACameraActor;
class AMosesPlayerController;
class AMosesPlayerState;
class AMosesLobbyGameState;

enum class EMosesRoomJoinResult : uint8;

DECLARE_MULTICAST_DELEGATE(FOnLobbyPlayerStateChanged);
DECLARE_MULTICAST_DELEGATE(FOnLobbyRoomStateChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyJoinRoomResult, EMosesRoomJoinResult /*Result*/, const FGuid& /*RoomId*/);

/**
 * UMosesLobbyLocalPlayerSubsystem
 *
 * 책임(요약)
 * - "로컬 전용 연출/표현" 실행자: UI 생성/제거, 카메라 전환, 프리뷰 캐릭터 변경
 * - 네트워크 단일진실(GameState/PlayerState)의 변화를 "위젯 갱신"으로 연결하는 브릿지
 *
 * 단일 진실(Single Source of Truth)
 * - 룸/채팅/Ready 등: AMosesLobbyGameState (복제)
 * - 내 상태/선택 캐릭터/로그인: AMosesPlayerState (복제)
 */
UCLASS(Config = Game)
class UE5_MULTI_SHOOTER_API UMosesLobbyLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

protected:
	/*====================================================
	= Engine
	====================================================*/
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	/*====================================================
	= Widget registration
	====================================================*/
	void RegisterLobbyWidget(UMosesLobbyWidget* InLobbyWidget);
	void SetLobbyWidget(UMosesLobbyWidget* InWidget);

public:
	/*====================================================
	= UI control
	====================================================*/
	void ActivateLobbyUI();
	void DeactivateLobbyUI();

public:
	/*====================================================
	= Notify (PC/GS → Subsys)
	====================================================*/
	void NotifyPlayerStateChanged();
	void NotifyRoomStateChanged();
	void NotifyJoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

public:
	/*====================================================
	= Events (Widget bind)
	====================================================*/
	FOnLobbyPlayerStateChanged& OnLobbyPlayerStateChanged() { return LobbyPlayerStateChangedEvent; }
	FOnLobbyRoomStateChanged& OnLobbyRoomStateChanged() { return LobbyRoomStateChangedEvent; }
	FOnLobbyJoinRoomResult& OnLobbyJoinRoomResult() { return LobbyJoinRoomResultEvent; }

public:
	/*====================================================
	= Login/Nickname (Local-only intent)
	====================================================*/
	void RequestSetLobbyNickname_LocalOnly(const FString& Nick);

public:
	/*====================================================
	= Lobby Preview (Local-only)
	====================================================*/
	void RequestNextCharacterPreview_LocalOnly();

private:
	/*====================================================
	= Lobby UI refresh (single pipeline)
	====================================================*/
	void RefreshLobbyUI_FromCurrentState();

private:
	/*====================================================
	= Retry control (Preview refresh - 중복 예약 방지)
	====================================================*/
	void RequestPreviewRefreshRetry_NextTick();
	void ClearPreviewRefreshRetry();

private:
	/*====================================================
	= Retry control (Bind GameState - Travel/LateJoin 대비)
	====================================================*/
	void RequestBindLobbyGameStateEventsRetry_NextTick();
	void ClearBindLobbyGameStateEventsRetry();

private:
	/*====================================================
	= Internal helpers
	====================================================*/
	bool IsLobbyContext() const;
	bool IsLobbyWidgetValid() const;

	void StopLobbyOnlyTimers();
	void ClearLobbyPreviewCache();

private:
	/*====================================================
	= Preview internal (Local-only)
	====================================================*/
	void RefreshLobbyPreviewCharacter_LocalOnly();
	void ApplyPreviewBySelectedId_LocalOnly(int32 SelectedId);
	ALobbyPreviewActor* GetOrFindLobbyPreviewActor_LocalOnly();

private:
	/*====================================================
	= Player access helpers
	====================================================*/
	AMosesPlayerController* GetMosesPC_LocalOnly() const;
	AMosesPlayerState* GetMosesPS_LocalOnly() const;

private:
	/*====================================================
	= Camera helpers (Local-only)
	====================================================*/
	ACameraActor* FindCameraByTag(const FName& CameraTag) const;
	void SetViewTargetToCameraTag(const FName& CameraTag, float BlendTime);
	void SwitchToCamera(ACameraActor* TargetCam, const TCHAR* LogFromTo);
	APlayerController* GetLocalPC() const;

private:
	/*====================================================
	= Lobby GameState bind
	====================================================*/
	AMosesLobbyGameState* GetLobbyGameState() const;
	void BindLobbyGameStateEvents();
	void UnbindLobbyGameStateEvents();

private:
	/*====================================================
	= Pending nickname send (Local-only)
	====================================================*/
	void TrySendPendingLobbyNickname_Local();

private:
	/*====================================================
	= Cached pointers / events
	====================================================*/
	UPROPERTY(Transient)
	TWeakObjectPtr<AMosesLobbyGameState> CachedLobbyGS;

	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyWidget> LobbyWidget = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALobbyPreviewActor> CachedLobbyPreviewActor;

	FOnLobbyPlayerStateChanged LobbyPlayerStateChangedEvent;
	FOnLobbyRoomStateChanged LobbyRoomStateChangedEvent;
	FOnLobbyJoinRoomResult LobbyJoinRoomResultEvent;

private:
	/*====================================================
	= Preview state
	====================================================*/
	int32 LocalPreviewSelectedId = 1;

	bool bPendingSelectedIdServerAck = false;
	int32 PendingSelectedIdServerAck = INDEX_NONE;

private:
	/*====================================================
	= Bind state flags
	====================================================*/
	bool bBoundToGameState = false;
	bool bLoggedPreviewWaitOnce = false;
	bool bLoggedBindWaitOnce = false;

private:
	/*====================================================
	= Camera policy vars
	====================================================*/
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float CameraBlendTime = 0.35f;

private:
	/*====================================================
	= Timers
	====================================================*/
	FTimerHandle PreviewRefreshRetryHandle;
	FTimerHandle BindLobbyGSRetryHandle;

private:
	/*====================================================
	= Login/Nickname intent (Local-only)
	====================================================*/
	FString PendingLobbyNickname_Local;
	bool bPendingLobbyNicknameSend_Local = false;
	bool bLoginSubmitted_Local = false;
};
