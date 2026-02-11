// ============================================================================
// MosesLobbyGameState.h (CLEAN)
// - Lobby 전용 GameState (RoomList/Chat 등)
// - 부모: AMosesGameState (ExperienceManagerComponent 공통 보장)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UE5_Multi_Shooter/Lobby/UI/MosesLobbyChatTypes.h" 

#include "UE5_Multi_Shooter/MosesGameState.h"
#include "MosesLobbyGameState.generated.h"

class AMosesPlayerState;

/** 채팅 메시지 구조체는 별도 헤더에서 정의된 것으로 가정 (Forward only) */
struct FLobbyChatMessage;

/** 룸 참가 결과(서버가 결정 → UI 표시용) */
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

// ============================================================================
// FastArray Item
// ============================================================================

USTRUCT()
struct FMosesLobbyRoomItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	// ---- Query ----
	bool IsFull() const;
	bool Contains(const FGuid& Pid) const;
	bool IsAllReady() const;

	// ---- Mutations ----
	void EnsureReadySize();
	bool SetReadyByPid(const FGuid& Pid, bool bInReady);

	// ---- FastArray callbacks (optional) ----
	void PostReplicatedAdd(const struct FMosesLobbyRoomList& InArraySerializer);
	void PostReplicatedChange(const struct FMosesLobbyRoomList& InArraySerializer);
	void PreReplicatedRemove(const struct FMosesLobbyRoomList& InArraySerializer);

public:
	UPROPERTY()
	FGuid RoomId;

	UPROPERTY()
	FString RoomTitle;

	UPROPERTY()
	int32 MaxPlayers = 0;

	UPROPERTY()
	FGuid HostPid;

	UPROPERTY()
	TArray<FGuid> MemberPids;

	/** Ready는 byte로 들고 가되(복제 효율), 0/1만 쓰는 정책 */
	UPROPERTY()
	TArray<uint8> MemberReady;
};

// ============================================================================
// FastArray Container
// ============================================================================

USTRUCT()
struct FMosesLobbyRoomList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	void SetOwner(class AMosesLobbyGameState* InOwner) { Owner = InOwner; }

	/** FastArray DeltaSerialize */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(Items, DeltaParams, *this);
	}

public:
	UPROPERTY()
	TArray<FMosesLobbyRoomItem> Items;

	/** 복제 대상이 아니므로 NotReplicated */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<class AMosesLobbyGameState> Owner;
};

template<>
struct TStructOpsTypeTraits<FMosesLobbyRoomList> : public TStructOpsTypeTraitsBase2<FMosesLobbyRoomList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * AMosesLobbyGameState
 *
 * 단일진실(룸 전광판)
 * - RoomList(방 목록 + 멤버/Ready/정원)
 * - ChatHistory
 *
 * 원칙
 * - 서버만 RoomList/ChatHistory를 수정
 * - 클라는 복제값을 받고 UI만 갱신
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameState : public AMosesGameState
{
	GENERATED_BODY()

public:
	AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Engine
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ✅ 로비에 Tick이 진짜 필요한 경우만 켜라.
	// 필요 없다면 cpp에서 PrimaryActorTick.bCanEverTick = false; 로 OFF 추천.
	virtual void Tick(float DeltaSeconds) override;

public:
	// -------------------------------------------------------------------------
	// Server APIs (Room)
	// -------------------------------------------------------------------------
	FGuid Server_CreateRoom(AMosesPlayerState* RequestPS, const FString& RoomTitle, int32 MaxPlayers);

	bool Server_JoinRoom(AMosesPlayerState* RequestPS, const FGuid& RoomId);
	bool Server_JoinRoomWithResult(AMosesPlayerState* RequestPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult);

	void Server_LeaveRoom(AMosesPlayerState* RequestPS);

	/** PS의 Ready가 바뀌었을 때 RoomList Ready를 동기화 */
	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	/** 호스트 기준 “게임 시작 가능” 정책(사유 문자열 포함) */
	bool Server_CanStartGame(AMosesPlayerState* HostPS, FString& OutFailReason) const;

	/** RoomId 기준 “매치 시작 가능” 정책 */
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

public:
	// -------------------------------------------------------------------------
	// Server APIs (Chat)
	// -------------------------------------------------------------------------
	void Server_AddChatMessage(AMosesPlayerState* SenderPS, const FString& Text);

public:
	// -------------------------------------------------------------------------
	// Client/UI Read-only accessors
	// -------------------------------------------------------------------------
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;
	const TArray<FMosesLobbyRoomItem>& GetRooms() const { return RoomList.Items; }
	const TArray<FLobbyChatMessage>& GetChatHistory() const { return ChatHistory; }

public:
	// -------------------------------------------------------------------------
	// UI Notify (single entry)
	// - 로컬 플레이어 서브시스템에 “상태 변경”을 전달 (UI는 그 이벤트만 구독)
	// -------------------------------------------------------------------------
	void NotifyRoomStateChanged_LocalPlayers() const;

protected:
	// -------------------------------------------------------------------------
	// RepNotify
	// -------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_RoomList();

	UFUNCTION()
	void OnRep_ChatHistory();

private:
	// -------------------------------------------------------------------------
	// Internal helpers
	// -------------------------------------------------------------------------
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);

	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

	// -------------------------------------------------------------------------
	// Logs (server)
	// -------------------------------------------------------------------------
	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

	// -------------------------------------------------------------------------
	// Safety
	// -------------------------------------------------------------------------
	bool IsServerAuth() const;

private:
	// -------------------------------------------------------------------------
	// Replicated state
	// -------------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_RoomList)
	FMosesLobbyRoomList RoomList;

	UPROPERTY(ReplicatedUsing = OnRep_ChatHistory)
	TArray<FLobbyChatMessage> ChatHistory;
};
