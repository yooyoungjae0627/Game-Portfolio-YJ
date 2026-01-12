#pragma once


#include "GameFramework/GameStateBase.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MosesLobbyGameState.generated.h"

class UMosesDialogueLineDataAsset;
class AMosesPlayerState;
class UMosesLobbyLocalPlayerSubsystem;

UENUM()
enum class EMosesRoomJoinResult : uint8
{
	Ok,
	InvalidRoomId,
	NoRoom,
	RoomFull,
	AlreadyMember,
	NotLoggedIn,
};

USTRUCT()
struct FMosesLobbyRoomItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Functions
	// ---------------------------
	bool IsFull() const;
	bool Contains(const FGuid& Pid) const;
	bool IsAllReady() const;

	void EnsureReadySize();
	bool SetReadyByPid(const FGuid& Pid, bool bInReady);

	void PostReplicatedAdd(const struct FMosesLobbyRoomList& InArraySerializer);
	void PostReplicatedChange(const struct FMosesLobbyRoomList& InArraySerializer);
	void PreReplicatedRemove(const struct FMosesLobbyRoomList& InArraySerializer);

public:
	// ---------------------------
	// Variables
	// ---------------------------
	UPROPERTY()
	FGuid RoomId;

	// 개발자 주석:
	// - 네트워크로 보내는 값은 FString이 가장 안전하다.
	// - UI에서만 FText로 변환해서 표시한다(한글 OK).
	UPROPERTY()
	FString RoomTitle;

	UPROPERTY()
	int32 MaxPlayers = 0;

	UPROPERTY()
	FGuid HostPid;

	UPROPERTY()
	TArray<FGuid> MemberPids;

	UPROPERTY()
	TArray<uint8> MemberReady;
};

USTRUCT()
struct FMosesLobbyRoomList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Functions
	// ---------------------------

	void SetOwner(class AMosesLobbyGameState* InOwner) { Owner = InOwner; }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(Items, DeltaParams, *this);
	}

public:
	// ---------------------------
	// Variables
	// ---------------------------

	UPROPERTY()
	TArray<FMosesLobbyRoomItem> Items;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<class AMosesLobbyGameState> Owner;
};

template<>
struct TStructOpsTypeTraits<FMosesLobbyRoomList> : public TStructOpsTypeTraitsBase2<FMosesLobbyRoomList>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPhaseChanged, EGamePhase /*NewPhase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesDialogueStateChanged, const FDialogueNetState& /*NewState*/);

/**
 * AMosesLobbyGameState
 *
 * 단일진실(룸 전광판)
 * - 방 목록(RoomList)
 * - 방 멤버/Ready/정원 등의 공유 상태
 *
 * 원칙:
 * - 서버가 RoomList를 수정
 * - 클라는 RoomList 복제값을 받아 UI만 갱신
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameState();

	// ---------------------------
	// Engine
	// ---------------------------
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------------------------
	// Server APIs
	// ---------------------------
	FGuid Server_CreateRoom(AMosesPlayerState* RequestPS, const FString& RoomTitle, int32 MaxPlayers);
	bool Server_JoinRoom(AMosesPlayerState* RequestPS, const FGuid& RoomId);
	
	// ✅ NEW: Join 결과 이유까지 반환
	bool Server_JoinRoomWithResult(AMosesPlayerState* RequestPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult);

	void Server_LeaveRoom(AMosesPlayerState* RequestPS);

	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	bool Server_CanStartGame(AMosesPlayerState* HostPS, FString& OutFailReason) const;
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

	// ---------------------------
	// Client/UI Read-only accessors
	// ---------------------------
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;
	const TArray<FMosesLobbyRoomItem>& GetRooms() const { return RoomList.Items; }

	// ---------------------------
	// UI Notify (single entry)
	// ---------------------------
	// 개발자 주석:
	// - "방 리스트 UI 갱신" 신호를 로컬 플레이어(들)의 Subsystem에만 보낸다.
	// - DS는 LocalPlayer가 없으므로 자연스럽게 아무 일도 안 한다.
	// - ListenServer는 서버 코드에서 호출해도 "로컬 UI"만 즉시 갱신된다.
	void NotifyRoomStateChanged_LocalPlayers() const;

	// ---------------------------
	// Read-only (클라/서버 모두 읽기 가능)
	// ---------------------------
	EGamePhase GetGamePhase() const { return GamePhase; }
	const FDialogueNetState& GetDialogueNetState() const { return DialogueNetState; }

	// ---------------------------
	// Events (클라/서버 모두 구독 가능)
	// ---------------------------
	FOnMosesPhaseChanged OnPhaseChanged;
	FOnMosesDialogueStateChanged OnDialogueStateChanged;

	// ---------------------------
	// Server-only mutators
	// ---------------------------
	// 개발자 주석:
	// - "상태 변경"은 서버만 한다.
	// - 클라는 절대 이 함수 호출하면 안 된다(불려도 내부에서 HasAuthority()로 컷).
	void ServerEnterLobbyDialogue();
	void ServerExitLobbyDialogue();
	void ServerSetSubState(EDialogueSubState NewSubState, float NewRemainingTime, bool bInNPCSpeaking);
	
	// 서버 전용: "다음 줄"로 진행 
	void ServerAdvanceLine();
	void ServerAdvanceLine(int32 NextLineIndex, float Duration, bool bInNPCSpeaking);

	// ===========================
	// Dialogue (Server Machine)
	// ===========================
	// 
	// 서버 전용: Dialogue 시작 (DataAsset 확정)
	void ServerBeginDialogue(UMosesDialogueLineDataAsset* InData);

	// 서버 전용: Pause / Resume / 기타 Flow 전환
	void ServerSetFlowState(EDialogueFlowState NewState);

	// 서버 전용: 같은 라인 재시작(Repeat)
	void ServerRepeatCurrentLine();

	// 서버 전용: 0번부터 재시작(Restart)
	void ServerRestartDialogue();

	/** 서버 권위 채팅 추가 */
	void Server_AddChatMessage(class AMosesPlayerState* SenderPS, const FString& Text);

	/** UI 읽기용 */
	const TArray<FLobbyChatMessage>& GetChatHistory() const { return ChatHistory; }

protected:
	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------
	// RepNotify (backup)
	// ---------------------------
	// 개발자 주석:
	// - FastArray는 OnRep가 항상 호출되는 게 아니라서,
	//   PostReplicatedAdd/Change/Remove가 메인 트리거다.
	// - 그래도 "백업"으로 RepNotify를 남겨둔다.
	UFUNCTION()
	void OnRep_RoomList();

private:
	// ---------------------------
	// RepNotify (이벤트만)
	// ---------------------------
	UFUNCTION()
	void OnRep_GamePhase();

	UFUNCTION()
	void OnRep_DialogueNetState();

	UFUNCTION()
	void OnRep_ChatHistory();


	// ---------------------------
	// Internal helpers
	// ---------------------------
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);

	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

	// ---------------------------
	// Logs
	// ---------------------------
	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

	// ---------------------------
	// Internal helpers
	// ---------------------------
	bool IsServerAuth() const;

	/**
	 * BroadcastAll_ServerToo
	 *
	 * 개발자 주석:
	 * - 서버는 "자기 자신에게" RepNotify가 오지 않는다.
	 * - 그러나 서버에서도 UI/연출(리스너)이 동일 이벤트를 받아야 하는 경우가 있다.
	 * - 따라서 서버에서 값을 바꾼 직후, 서버도 동일 브로드캐스트를 수동 호출한다.
	 */
	void BroadcastAll_ServerToo();

	// ---------------------------
	// Dialogue Machine (Server-only)
	// ---------------------------
	void TickDialogue(float DeltaSeconds);
	void AdvanceDialogueLine();
	void AdvanceDialogueLine_Internal();

	// DoD 로그 유틸 (서버 전용)
	void LogDialogueSpeaking_DoD() const;
	void LogDialoguePause_DoD() const;
	void LogDialogueResume_DoD() const;

	void ServerApplyPhase(EGamePhase NewPhase, EDialogueCommandType CmdType);
	void ServerStampCommand(EDialogueCommandType CmdType);
	void ServerResetDialogueNetState_KeepSeq(EDialogueCommandType CmdType);

private:
	// ---------------------------
	// Replicated state
	// ---------------------------
	UPROPERTY(ReplicatedUsing = OnRep_RoomList)
	FMosesLobbyRoomList RoomList;

	// ---------------------------
	// Replicated state (전광판 데이터)
	// ---------------------------
	UPROPERTY(ReplicatedUsing = OnRep_GamePhase)
	EGamePhase GamePhase = EGamePhase::Lobby;

	UPROPERTY(ReplicatedUsing = OnRep_DialogueNetState)
	FDialogueNetState DialogueNetState;

	// Dialogue 기본 데이터 (경로 기반 로딩용)
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	TSoftObjectPtr<UMosesDialogueLineDataAsset> DefaultLobbyDialogueDataAsset;

	// 서버 전용 런타임 데이터 (복제 ❌)
	UPROPERTY(Transient)
	TObjectPtr<UMosesDialogueLineDataAsset> DialogueData = nullptr;

	// 서버 전용: Speaking 로그 스팸 방지용 (복제 ❌)
	UPROPERTY()
	float DialogueSpeakingLogCooldown = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ChatHistory)
	TArray<FLobbyChatMessage> ChatHistory;
};
