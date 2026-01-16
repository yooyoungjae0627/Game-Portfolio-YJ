// ============================================================================
// MosesLobbyGameState.h
// ============================================================================

#pragma once

#include "Net/Serialization/FastArraySerializer.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesGameState.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatTypes.h"

#include "MosesLobbyGameState.generated.h"

class AMosesPlayerState;

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
	/*====================================================
	= Functions
	====================================================*/
	bool IsFull() const;
	bool Contains(const FGuid& Pid) const;
	bool IsAllReady() const;

	void EnsureReadySize();
	bool SetReadyByPid(const FGuid& Pid, bool bInReady);

	void PostReplicatedAdd(const struct FMosesLobbyRoomList& InArraySerializer);
	void PostReplicatedChange(const struct FMosesLobbyRoomList& InArraySerializer);
	void PreReplicatedRemove(const struct FMosesLobbyRoomList& InArraySerializer);

public:
	/*====================================================
	= Variables
	====================================================*/
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

	UPROPERTY()
	TArray<uint8> MemberReady;
};

USTRUCT()
struct FMosesLobbyRoomList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	void SetOwner(class AMosesLobbyGameState* InOwner) { Owner = InOwner; }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(Items, DeltaParams, *this);
	}

public:
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
class UE5_MULTI_SHOOTER_API AMosesLobbyGameState : public AMosesGameState
{
	GENERATED_BODY()

public:
	AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer);

public:
	/*====================================================
	= Engine
	====================================================*/
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void Tick(float DeltaSeconds) override;

public:
	/*====================================================
	= Server APIs (Room)
	====================================================*/
	FGuid Server_CreateRoom(AMosesPlayerState* RequestPS, const FString& RoomTitle, int32 MaxPlayers);
	bool Server_JoinRoom(AMosesPlayerState* RequestPS, const FGuid& RoomId);
	bool Server_JoinRoomWithResult(AMosesPlayerState* RequestPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult);
	void Server_LeaveRoom(AMosesPlayerState* RequestPS);

	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	bool Server_CanStartGame(AMosesPlayerState* HostPS, FString& OutFailReason) const;
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

public:
	/*====================================================
	= Server APIs (Chat)
	====================================================*/
	void Server_AddChatMessage(AMosesPlayerState* SenderPS, const FString& Text);

public:
	/*====================================================
	= Client/UI Read-only accessors
	====================================================*/
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;
	const TArray<FMosesLobbyRoomItem>& GetRooms() const { return RoomList.Items; }

	const TArray<FLobbyChatMessage>& GetChatHistory() const { return ChatHistory; }

public:
	/*====================================================
	= UI Notify (single entry)
	====================================================*/
	void NotifyRoomStateChanged_LocalPlayers() const;

protected:
	/*====================================================
	= RepNotify (backup)
	====================================================*/
	UFUNCTION()
	void OnRep_RoomList();

	UFUNCTION()
	void OnRep_ChatHistory();

private:
	/*====================================================
	= Internal helpers
	====================================================*/
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);

	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

private:
	/*====================================================
	= Logs
	====================================================*/
	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

private:
	/*====================================================
	= Internal helpers
	====================================================*/
	bool IsServerAuth() const;

private:
	/*====================================================
	= Replicated state
	====================================================*/
	UPROPERTY(ReplicatedUsing = OnRep_RoomList)
	FMosesLobbyRoomList RoomList;

	UPROPERTY(ReplicatedUsing = OnRep_ChatHistory)
	TArray<FLobbyChatMessage> ChatHistory;
};
