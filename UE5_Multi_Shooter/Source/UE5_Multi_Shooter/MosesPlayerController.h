// ============================================================================
// UE5_Multi_Shooter/MosesPlayerController.h (FULL)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

#include "UE5_Multi_Shooter/Persist/MosesMatchRecordTypes.h"

#include "MosesPlayerController.generated.h"

class AMosesLobbyGameState;
class AMosesLobbyGameMode;
class AMosesStartGameMode;
class AMosesMatchGameMode;
class AMosesPlayerState;
class ACameraActor;

class UMosesMatchHUD;
class UMosesCameraComponent;
class UMosesCombatComponent;
class UMosesWeaponData;

enum class EMosesRoomJoinResult : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyNicknameChanged, const FString&, NewNick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJoinedRoom, EMosesRoomJoinResult, Result, const FGuid&, RoomId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchRecordsReceivedBP, const TArray<FMosesMatchRecordSummary>&, Records);

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	/** BeginPlay: Local-only UI/Input/Camera policy를 맵 컨텍스트에 맞춰 적용한다. */
	virtual void BeginPlay() override;

	/** EndPlay: Local-only 타이머/바인딩/캐시를 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** OnPossess: Pawn 변경 시 카메라/입력 정책을 즉시 재적용한다. */
	virtual void OnPossess(APawn* InPawn) override;

	/** ClientRestart: 클라이언트에서 Pawn 리스타트 시 맵 컨텍스트에 맞게 카메라/입력 모드 갱신. */
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	/** PlayerState Rep 수신: Lobby UI 브릿지(Subsys) 갱신 + Scope 이벤트 재바인딩. */
	virtual void OnRep_PlayerState() override;

	/** Pawn Rep 수신: Match 컨텍스트에서 카메라 정책 강제(Travel 타이밍 흔들림 보호). */
	virtual void OnRep_Pawn() override;

	/** Replication 목록 설정. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/* Debug Exec */

	/** (Dev) ASC 태그 덤프. */
	UFUNCTION(Exec)
	void gas_DumpTags();

	/** (Dev) ASC Attribute 덤프. */
	UFUNCTION(Exec)
	void gas_DumpAttr();

	/* Dev Exec Helpers */

	/** (Dev) 서버로 Match 맵 Travel 요청. */
	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	/** (Dev) 서버로 Lobby 맵 Travel 요청. */
	UFUNCTION(Exec)
	void TravelToLobby_Exec();

	/* Client → Server RPC (Lobby) */

	/** 방 생성 요청. 서버에서 제목/인원 Clamp 후 처리한다. */
	UFUNCTION(Server, Reliable)
	void Server_CreateRoom(const FString& RoomTitle, int32 MaxPlayers);

	/** 방 참가 요청. RoomId 유효성 검사 후 처리한다. */
	UFUNCTION(Server, Reliable)
	void Server_JoinRoom(const FGuid& RoomId);

	/** 방 나가기 요청. */
	UFUNCTION(Server, Reliable)
	void Server_LeaveRoom();

	/** Ready 토글 요청(호스트는 무시). */
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	/** 호스트의 Match 시작 요청. */
	UFUNCTION(Server, Reliable)
	void Server_RequestStartMatch();

	/** 로비 닉네임 설정(서버에서 정리 후 PS에 반영). */
	UFUNCTION(Server, Reliable)
	void Server_SetLobbyNickname(const FString& Nick);

	/** 로비 채팅 전송 요청. */
	UFUNCTION(Server, Reliable)
	void Server_SendLobbyChat(const FString& Text);

	/** 결과 화면에서 Lobby 복귀 요청(서버 승인 후 Travel). */
	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToLobby();

	/* JoinRoom Result (Server → Client) */

	/** JoinRoom 결과를 클라이언트에 전달한다(Subsystem 브릿지). */
	UFUNCTION(Client, Reliable)
	void Client_JoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

	/* Travel Guard (Dev Exec → Server Only) */

	/** (Dev) 서버 전용 Match Travel 실행. */
	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	/** (Dev) 서버 전용 Lobby Travel 실행. */
	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

	/* Enter Lobby / Character Select */

	/** StartGame → Lobby 진입 요청(서버에서 Travel). */
	UFUNCTION(Server, Reliable)
	void Server_RequestEnterLobby(const FString& Nickname);

	/** 캐릭터 선택(서버에서 SafeId로 검증 후 PS 반영). */
	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 SelectedId);

	/* Persist: Lobby Records Request/Receive */

	/** (Local) 기록 요청 브릿지. 내부에서 Server RPC를 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Persist")
	void RequestMatchRecords_Local(const FString& NicknameFilter, int32 MaxCount = 50);

	/** (Server) 저장소에서 기록 목록을 조회하고 Client로 전송한다. */
	UFUNCTION(Server, Reliable)
	void Server_RequestMatchRecords(const FString& NicknameFilter, int32 MaxCount);

	/** (Client) 기록 목록 수신 → BP Delegate로 브로드캐스트. */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveMatchRecords(const TArray<FMosesMatchRecordSummary>& Records);

	/* Owner-only Toast */

	/** (Client, OwnerOnly 의미) 픽업 토스트 표시( HUD 준비 전이면 Pending + Retry ). */
	UFUNCTION(Client, Reliable)
	void Client_ShowPickupToast_OwnerOnly(const FText& Text, float DurationSec);

	/** (Client, OwnerOnly 의미) 헤드샷 토스트 표시( HUD 준비 전이면 Pending 유지 ). */
	UFUNCTION(Client, Reliable)
	void Client_ShowHeadshotToast_OwnerOnly(const FText& Text, float DurationSec);

	/* Local helpers (UI) */

	/** (Local) 로비 닉네임을 Pending에 저장하고, 조건이 되면 서버로 전송한다. */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPendingLobbyNickname_Local(const FString& Nick);

	/* Delegates for Blueprint/UI */

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyNicknameChanged OnLobbyNicknameChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnJoinedRoom OnLobbyJoinedRoom;

	UPROPERTY(BlueprintAssignable, Category = "Persist")
	FOnMatchRecordsReceivedBP OnMatchRecordsReceivedBP;

	/* Scope: Local cosmetic only */

	/** 스코프 입력 Press 처리(로컬 코스메틱). */
	void Scope_OnPressed_Local();

	/** 스코프 입력 Release 처리(로컬 코스메틱). */
	void Scope_OnReleased_Local();

	/** 현재 스코프가 켜져있는지 반환(로컬). */
	bool IsScopeActive_Local() const { return bScopeActive_Local; }

private:
	/* Camera Policy (Local-only) */

	/** Match 컨텍스트에서 커서/입력/카메라 타겟 정책을 강제한다. */
	void ApplyMatchCameraPolicy_LocalOnly(const TCHAR* Reason);

	/** Pawn이 아직 없을 때 NextTick으로 재시도한다. */
	void RetryApplyMatchCameraPolicy_NextTick_LocalOnly();

	/* Pending nickname send (Local-only) */

	/** NetConnection/PlayerState 준비 후 Pending 닉네임을 서버로 전송한다. */
	void TrySendPendingLobbyNickname_Local();

	/* Server-side Travel helpers */

	/** 현재 GameMode가 LobbyGM일 때 Match로 Travel. */
	void DoServerTravelToMatch();

	/** 현재 GameMode가 MatchGM일 때 Lobby로 Travel. */
	void DoServerTravelToLobby();

	/* Context (Local-only) */

	/** 로비 컨텍스트인지 판정(맵/GS 기반, Match/Start는 절대 로비로 판정하지 않음). */
	bool IsLobbyContext() const;

	/** 현재 맵이 LobbyMapName인지. */
	bool IsLobbyMap_Local() const;

	/** 현재 맵이 MatchMapName인지. */
	bool IsMatchMap_Local() const;

	/** 현재 맵이 StartMapName인지. */
	bool IsStartMap_Local() const;

	/* Lobby Camera / UI (Local-only) */

	/** 태그 기반 Lobby 프리뷰 카메라로 ViewTarget을 전환한다. */
	void ApplyLobbyPreviewCamera();

	/** Lobby UI를 Subsystem을 통해 활성화한다. */
	void ActivateLobbyUI_LocalOnly();

	/** Lobby가 아닌 상태의 기본값으로 복구한다(카메라 자동관리 등). */
	void RestoreNonLobbyDefaults_LocalOnly();

	/* Input Mode (Local-only) */

	/** Lobby 입력 모드(Game+UI, 커서 ON). */
	void ApplyLobbyInputMode_LocalOnly();

	/** Lobby 입력 모드 복구(GameOnly, 커서 OFF). */
	void RestoreNonLobbyInputMode_LocalOnly();

	/** Travel 타이밍 보호를 위해 NextTick에 Lobby 입력 모드를 재적용한다. */
	void ReapplyLobbyInputMode_NextTick_LocalOnly();

	/** Start 입력 모드(Game+UI, 커서 ON). */
	void ApplyStartInputMode_LocalOnly();

	/** NextTick에 Start 입력 모드를 재적용한다. */
	void ReapplyStartInputMode_NextTick_LocalOnly();

	/* Pickup Toast retry (Local-only) */

	/** HUD가 늦게 생성되는 케이스 대비 Retry 타이머 시작. */
	void StartPickupToastRetryTimer_Local();

	/** Retry 타이머 종료. */
	void StopPickupToastRetryTimer_Local();

	/** Retry 틱에서 Pending 토스트 Flush를 시도한다. */
	void HandlePickupToastRetryTimer_Local();

	/* Shared getters (null/log guard) */

	/** LobbyGameState를 가져오고 없으면 로그를 남긴다. */
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;

	/** MosesPlayerState를 가져오고 없으면 로그를 남긴다. */
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(const TCHAR* Caller) const;

	/* Toast pending flush */

	/** Pending 픽업 토스트를 HUD에 Flush 시도한다. */
	void TryFlushPendingPickupToast_Local();

	/** Pending 헤드샷 토스트를 HUD에 Flush 시도한다. */
	void TryFlushPendingHeadshotToast_Local();

	/** 소유자 기준 MatchHUD를 탐색하고 캐시한다. */
	UMosesMatchHUD* FindMatchHUD_Local();

	/* Scope: weapon-change safety (Local-only) */

	/** CombatComponent의 장착 변경 이벤트를 바인딩한다. */
	void BindScopeWeaponEvents_Local();

	/** CombatComponent의 장착 변경 이벤트를 언바인딩한다. */
	void UnbindScopeWeaponEvents_Local();

	/** 장착 무기 변경 시 스코프/HUD 상태를 안전하게 갱신한다. */
	UFUNCTION()
	void HandleEquippedChanged_ScopeLocal(int32 SlotIndex, FGameplayTag WeaponId);

	/** 스코프 활성/비활성 + HUD/카메라 코스메틱 반영. */
	void SetScopeActive_Local(bool bActive, const UMosesWeaponData* WeaponData);

	/** 스코프 블러 타이머 시작. */
	void StartScopeBlurTimer_Local();

	/** 스코프 블러 타이머 종료. */
	void StopScopeBlurTimer_Local();

	/** 이동 속도 기반으로 블러 강도를 갱신한다. */
	void TickScopeBlur_Local();

	/** 현재 장착 무기가 스코프 사용 가능(스나이퍼)인지 검사한다. */
	bool CanUseScope_Local(const UMosesWeaponData*& OutWeaponData) const;

	/** PlayerState에서 CombatComponent를 찾는다. */
	UMosesCombatComponent* FindCombatComponent_Local() const;

	/** Pawn에서 MosesCameraComponent를 찾는다. */
	UMosesCameraComponent* FindMosesCameraComponent_Local() const;

private:
	/* =========================================================================
	 * Variables (UPROPERTY first, then non-UPROPERTY)
	 * ========================================================================= */

	 /* Map Policy */

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Policy")
	FName LobbyMapName = TEXT("LobbyLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Match|Policy")
	FName MatchMapName = TEXT("MatchLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Start|Policy")
	FName StartMapName = TEXT("StartGameLevel");

	/* Lobby Preview Camera policy */

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	/* Replicated lobby-related state */

	UPROPERTY(Replicated)
	int32 SelectedCharacterId = INDEX_NONE;

	UPROPERTY(Replicated)
	bool bRep_IsReady = false;

	/* Local pending nickname */

	UPROPERTY(Transient)
	FString PendingLobbyNickname_Local;

	UPROPERTY(Transient)
	bool bPendingLobbyNicknameSend_Local = false;

	/* Pending Pickup Toast */

	UPROPERTY(Transient)
	bool bPendingPickupToast = false;

	UPROPERTY(Transient)
	FText PendingPickupToastText;

	UPROPERTY(Transient)
	float PendingPickupToastDuration = 0.f;

	/* Pending Headshot Toast */

	UPROPERTY(Transient)
	bool bPendingHeadshotToast = false;

	UPROPERTY(Transient)
	FText PendingHeadshotToastText;

	UPROPERTY(Transient)
	float PendingHeadshotToastDuration = 0.f;

	/* Scope: Local cosmetic state */

	UPROPERTY(EditDefaultsOnly, Category = "Scope|Policy")
	float ScopeBlurUpdateInterval = 0.05f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMosesMatchHUD> CachedMatchHUD_Local;

	UPROPERTY(Transient)
	TWeakObjectPtr<const UMosesWeaponData> CachedScopeWeaponData_Local;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatForScope_Local;

	/* Non-UPROPERTY timers / counters / flags */

	FTimerHandle LobbyPreviewCameraTimerHandle;
	FTimerHandle MatchCameraRetryTimerHandle;

	FTimerHandle TimerHandle_PickupToastRetry;
	int32 PickupToastRetryCount = 0;

	bool bScopeActive_Local = false;
	FTimerHandle ScopeBlurTimerHandle;

	static constexpr int32 MaxPickupToastRetryCount = 60;
	static constexpr float PickupToastRetryIntervalSec = 0.1f;
};
