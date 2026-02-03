#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

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
 *
 * [MOD 핵심]
 * - Start→Lobby Travel 직후 GameState 생성/복제 타이밍 때문에 IsLobbyContext()가 잠깐 false가 될 수 있음.
 *   그 순간 RestoreNonLobbyInputMode()가 실행되면 커서 OFF가 되어 "커서가 안 보이는" 현상이 발생한다.
 * - 따라서 Lobby 여부 판정은 "맵 이름"을 1순위로 사용하고, GameState 체크는 보조로 사용한다.
 * - Match 맵에서는 커서를 무조건 OFF로 강제한다.
 *
 * [ADD: START]
 * - Start 맵(StartGameLevel 등)에서도 UI 입력(닉네임 입력/버튼 클릭)을 위해 커서를 ON으로 유지한다.
 *
 * [MOD: DAY8 Scope]
 * - 스코프는 로컬 연출이다.
 * - 스나이퍼만 허용(Tag 기반)
 * - 스코프 ON/OFF 시:
 *   - HUD 스코프 위젯 토글
 *   - 카메라 FOV 오버라이드(2배율)
 *   - 이동 속도 기반 블러(로컬 타이머)
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

	UFUNCTION(Server, Reliable)
	void Server_RequestCaptureSuccessAnnouncement();


protected:
	/*====================================================
	= Lifecycle (Local UI / Camera)
	====================================================*/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;

protected:
	virtual void OnRep_Pawn() override; // Pawn 복제 도착 훅(클라 핵심)

	/*====================================================
	= Replication
	====================================================*/
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void ApplyMatchCameraPolicy_LocalOnly(const TCHAR* Reason);
	void RetryApplyMatchCameraPolicy_NextTick_LocalOnly();

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
	= Lobby/Match/Start Context 판정 (Local-only)
	====================================================*/
	bool IsLobbyContext() const;        // [MOD] 맵 이름 기반 1순위 판정
	bool IsLobbyMap_Local() const;      // [ADD]
	bool IsMatchMap_Local() const;      // [ADD]
	bool IsStartMap_Local() const;      // [ADD]

private:
	/*====================================================
	= Lobby Context / Camera / UI (Local-only)
	====================================================*/
	void ApplyLobbyPreviewCamera();

	void ActivateLobbyUI_LocalOnly();
	void RestoreNonLobbyDefaults_LocalOnly();

	void ApplyLobbyInputMode_LocalOnly();
	void RestoreNonLobbyInputMode_LocalOnly();

	void ReapplyLobbyInputMode_NextTick_LocalOnly(); // ✅ FIX: 함수 선언 정상화
	void ApplyStartInputMode_LocalOnly();
	void ReapplyStartInputMode_NextTick_LocalOnly(); // ✅ FIX: 함수 선언 정상화

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
	= Map Policy (중요)
	====================================================*/
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Policy")
	FName LobbyMapName = TEXT("LobbyLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Match|Policy")
	FName MatchMapName = TEXT("MatchLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Start|Policy")
	FName StartMapName = TEXT("StartGameLevel");

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

private:
	FTimerHandle MatchCameraRetryTimerHandle;

	// =====================================================================
	// [MOD][DAY8] Scope Local (연출 전용)
	// =====================================================================
public:
	/** 로컬 입력(우클릭 Press)에서 호출: Scope ON */
	void Scope_OnPressed_Local();

	/** 로컬 입력(우클릭 Release)에서 호출: Scope OFF */
	void Scope_OnReleased_Local();

public:
	bool IsScopeActive_Local() const { return bScopeActive_Local; }

private:
	/** Scope 토글(로컬) */
	void SetScopeActive_Local(bool bActive, const UMosesWeaponData* WeaponData);

	/** 스코프 중 이동 블러 타이머 */
	void StartScopeBlurTimer_Local();
	void StopScopeBlurTimer_Local();
	void TickScopeBlur_Local();

	/** 현재 무기 데이터가 스코프 가능한지(스나이퍼인지) 판정 */
	bool CanUseScope_Local(const UMosesWeaponData*& OutWeaponData) const;

	/** 로컬 CombatComponent/Camera/HUD 찾기 */
	UMosesCombatComponent* FindCombatComponent_Local() const;
	UMosesCameraComponent* FindMosesCameraComponent_Local() const;
	UMosesMatchHUD* FindMatchHUD_Local();

private:
	/** 로컬 스코프 상태 */
	bool bScopeActive_Local = false;

	/** 블러 업데이트 타이머 */
	FTimerHandle ScopeBlurTimerHandle;

	/** 블러 업데이트 주기(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Scope|Policy")
	float ScopeBlurUpdateInterval = 0.05f; // 20Hz

	/** HUD 캐시(찾았으면 재사용) */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMosesMatchHUD> CachedMatchHUD_Local;

	/** 마지막으로 스코프 ON에 사용한 무기 데이터(로컬) */
	UPROPERTY(Transient)
	TWeakObjectPtr<const UMosesWeaponData> CachedScopeWeaponData_Local;
};
