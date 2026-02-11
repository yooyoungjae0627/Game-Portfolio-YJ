// ============================================================================
// UE5_Multi_Shooter/MosesPlayerController.h  (FULL / CLEANED)  [MOD]
// - 역할: 입력/카메라/로컬 UI 정책 + Lobby RPC + Persist 요청/수신 + (OwnerOnly Toast)
// - 주의: CombatComponent 델리게이트는 "Native multicast"로 바뀐 상태를 전제로
//         (Combat->OnEquippedChanged.AddUObject(...) 그대로 유지 가능)
//
// [정리 포인트]
// 1) include 최소화 + forward decl 정리
// 2) 섹션을 "기능 단위"로 재배치 (Lobby / Persist / Toast / Scope / Perf / Map&UI)
// 3) override/replication 구간을 상단에 모음
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/Persist/MosesMatchRecordTypes.h"
#include "MosesPlayerController.generated.h"

// ----------------------------------------------------------------------------
// Forward Decls
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Blueprint delegates (Lobby/Persist UI용: BP 바인딩)
// ----------------------------------------------------------------------------
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyNicknameChanged, const FString&, NewNick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJoinedRoom, EMosesRoomJoinResult, Result, const FGuid&, RoomId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchRecordsReceivedBP, const TArray<FMosesMatchRecordSummary>&, Records);

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// =========================================================================
	// Engine overrides
	// =========================================================================
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Pawn() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// =========================================================================
	// Debug Exec (GAS)
	// =========================================================================
	UFUNCTION(Exec) void gas_DumpTags();
	UFUNCTION(Exec) void gas_DumpAttr();

	// =========================================================================
	// Dev Exec Helpers (Travel)
	// =========================================================================
	UFUNCTION(Exec) void TravelToMatch_Exec();
	UFUNCTION(Exec) void TravelToLobby_Exec();

	// =========================================================================
	// Client -> Server RPC (Lobby)
	// =========================================================================
	UFUNCTION(Server, Reliable) void Server_CreateRoom(const FString& RoomTitle, int32 MaxPlayers);
	UFUNCTION(Server, Reliable) void Server_JoinRoom(const FGuid& RoomId);
	UFUNCTION(Server, Reliable) void Server_LeaveRoom();
	UFUNCTION(Server, Reliable) void Server_SetReady(bool bInReady);
	UFUNCTION(Server, Reliable) void Server_RequestStartMatch();
	UFUNCTION(Server, Reliable) void Server_SetLobbyNickname(const FString& Nick);
	UFUNCTION(Server, Reliable) void Server_SendLobbyChat(const FString& Text);

	// Result Confirm -> Return to Lobby
	UFUNCTION(Server, Reliable) void Server_RequestReturnToLobby();

	// JoinRoom Result (Server -> Client)
	UFUNCTION(Client, Reliable) void Client_JoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

	// =========================================================================
	// Travel Guard (Dev Exec -> Server Only)
	// =========================================================================
	UFUNCTION(Server, Reliable) void Server_TravelToMatch();
	UFUNCTION(Server, Reliable) void Server_TravelToLobby();

	// =========================================================================
	// Enter Lobby / Character Select
	// =========================================================================
	UFUNCTION(Server, Reliable) void Server_RequestEnterLobby(const FString& Nickname);
	UFUNCTION(Server, Reliable) void Server_SetSelectedCharacterId(int32 SelectedId);

	// =========================================================================
	// [PERSIST] Lobby Records Request/Receive
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Persist")
	void RequestMatchRecords_Local(const FString& NicknameFilter, int32 MaxCount = 50);

	UFUNCTION(Server, Reliable)
	void Server_RequestMatchRecords(const FString& NicknameFilter, int32 MaxCount);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveMatchRecords(const TArray<FMosesMatchRecordSummary>& Records);

	// =========================================================================
	// OwnerOnly Toasts (Pickup / Headshot)
	// =========================================================================
	UFUNCTION(Client, Reliable)
	void Client_ShowPickupToast_OwnerOnly(const FText& Text, float DurationSec);

	UFUNCTION(Client, Reliable)
	void Client_ShowHeadshotToast_OwnerOnly(const FText& Text, float DurationSec);

	// =========================================================================
	// Local helpers (UI)
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPendingLobbyNickname_Local(const FString& Nick);

	// =========================================================================
	// Delegates for Blueprint/UI
	// =========================================================================
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyNicknameChanged OnLobbyNicknameChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnJoinedRoom OnLobbyJoinedRoom;

	UPROPERTY(BlueprintAssignable, Category = "Persist")
	FOnMatchRecordsReceivedBP OnMatchRecordsReceivedBP;

	// =========================================================================
	// [SCOPE] Local cosmetic only (Zoom/Blur etc)
	// =========================================================================
public:
	void Scope_OnPressed_Local();
	void Scope_OnReleased_Local();
	bool IsScopeActive_Local() const { return bScopeActive_Local; }

	// =========================================================================
	// [PERF] Perf Infra console entry points (called from CheatManager)
	// =========================================================================
public:
	void Perf_Dump();
	void Perf_Marker(int32 MarkerIndex);
	void Perf_Spawn(int32 Count);
	void Perf_MeasureBegin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec);
	void Perf_MeasureEnd();

	UFUNCTION(Server, Reliable)
	void Server_PerfMarker(const FName MarkerId);

	UFUNCTION(Server, Reliable)
	void Server_PerfSpawn(int32 Count);

protected:
	UFUNCTION(Server, Reliable)
	void Server_PerfMeasureBegin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec);

	UFUNCTION(Server, Reliable)
	void Server_PerfMeasureEnd();

private:
	// =========================================================================
	// Camera Policy (Local-only)
	// =========================================================================
	void ApplyMatchCameraPolicy_LocalOnly(const TCHAR* Reason);
	void RetryApplyMatchCameraPolicy_NextTick_LocalOnly();

	// =========================================================================
	// Pending nickname send (Local-only)
	// =========================================================================
	void TrySendPendingLobbyNickname_Local();

	// =========================================================================
	// Server-side Travel helpers
	// =========================================================================
	void DoServerTravelToMatch();
	void DoServerTravelToLobby();

	// =========================================================================
	// Context 판정 (Local-only)
	// =========================================================================
	bool IsLobbyContext() const;
	bool IsLobbyMap_Local() const;
	bool IsMatchMap_Local() const;
	bool IsStartMap_Local() const;

	// =========================================================================
	// Lobby Context / Camera / UI (Local-only)
	// =========================================================================
	void ApplyLobbyPreviewCamera();
	void ActivateLobbyUI_LocalOnly();
	void RestoreNonLobbyDefaults_LocalOnly();

	void ApplyLobbyInputMode_LocalOnly();
	void RestoreNonLobbyInputMode_LocalOnly();
	void ReapplyLobbyInputMode_NextTick_LocalOnly();

	void ApplyStartInputMode_LocalOnly();
	void ReapplyStartInputMode_NextTick_LocalOnly();

	// =========================================================================
	// Toast Pending (HUD 교체 타이밍 대비)
	// =========================================================================
	void TryFlushPendingPickupToast_Local();
	void TryFlushPendingHeadshotToast_Local();
	UMosesMatchHUD* FindMatchHUD_Local();

	// [MOD] HUD가 아직 없을 때 재시도하기 위한 타이머/루프
	void StartPickupToastRetryTimer_Local();
	void StopPickupToastRetryTimer_Local();
	void HandlePickupToastRetryTimer_Local();

	// =========================================================================
	// Shared getters (null/log guard)
	// =========================================================================
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(const TCHAR* Caller) const;

	// =========================================================================
	// [SCOPE] weapon-change safety (local only)
	// =========================================================================
	void BindScopeWeaponEvents_Local();
	void UnbindScopeWeaponEvents_Local();

	UFUNCTION()
	void HandleEquippedChanged_ScopeLocal(int32 SlotIndex, FGameplayTag WeaponId);

	void SetScopeActive_Local(bool bActive, const UMosesWeaponData* WeaponData);
	void StartScopeBlurTimer_Local();
	void StopScopeBlurTimer_Local();
	void TickScopeBlur_Local();

	bool CanUseScope_Local(const UMosesWeaponData*& OutWeaponData) const;

	UMosesCombatComponent* FindCombatComponent_Local() const;
	UMosesCameraComponent* FindMosesCameraComponent_Local() const;

private:
	// =========================================================================
	// Lobby Preview Camera policy vars
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	FName LobbyPreviewCameraTag = TEXT("LobbyPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Camera")
	float LobbyPreviewCameraDelay = 0.2f;

	FTimerHandle LobbyPreviewCameraTimerHandle;

	// =========================================================================
	// Map Policy
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Policy")
	FName LobbyMapName = TEXT("LobbyLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Match|Policy")
	FName MatchMapName = TEXT("MatchLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Start|Policy")
	FName StartMapName = TEXT("StartGameLevel");

	// =========================================================================
	// Local pending nickname
	// =========================================================================
	UPROPERTY(Transient)
	FString PendingLobbyNickname_Local;

	UPROPERTY(Transient)
	bool bPendingLobbyNicknameSend_Local = false;

	// =========================================================================
	// Replicated lobby-related state (주의: PS와 중복되면 안 됨)
	// =========================================================================
	UPROPERTY(Replicated)
	int32 SelectedCharacterId = INDEX_NONE;

	UPROPERTY(Replicated)
	bool bRep_IsReady = false;

	FTimerHandle MatchCameraRetryTimerHandle;

	// =========================================================================
	// Pending Pickup Toast
	// =========================================================================
	UPROPERTY(Transient)
	bool bPendingPickupToast = false;

	UPROPERTY(Transient)
	FText PendingPickupToastText;

	UPROPERTY(Transient)
	float PendingPickupToastDuration = 0.f;

	// =========================================================================
	// Pending Headshot Toast  [MOD]
	// =========================================================================
	UPROPERTY(Transient)
	bool bPendingHeadshotToast = false;

	UPROPERTY(Transient)
	FText PendingHeadshotToastText;

	UPROPERTY(Transient)
	float PendingHeadshotToastDuration = 0.f;

	// =========================================================================
	// [SCOPE] Local cosmetic state
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Scope|Policy")
	float ScopeBlurUpdateInterval = 0.05f; // 20Hz

	UPROPERTY(Transient)
	TWeakObjectPtr<UMosesMatchHUD> CachedMatchHUD_Local;

	UPROPERTY(Transient)
	TWeakObjectPtr<const UMosesWeaponData> CachedScopeWeaponData_Local;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMosesCombatComponent> CachedCombatForScope_Local;

	bool bScopeActive_Local = false;
	FTimerHandle ScopeBlurTimerHandle;

	// =========================================================================
	// [MOD] Retry timer state (HUD 생성 지연 대비)
	// =========================================================================
	FTimerHandle TimerHandle_PickupToastRetry;
	int32 PickupToastRetryCount = 0;

	static constexpr int32 MaxPickupToastRetryCount = 60;     // 60 * 0.1s = 6초
	static constexpr float PickupToastRetryIntervalSec = 0.1f;
};
