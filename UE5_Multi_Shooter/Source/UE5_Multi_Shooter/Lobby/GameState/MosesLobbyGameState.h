// ============================================================================
// UE5_Multi_Shooter/Lobby/GameState/MosesLobbyGameState.h  (FULL - REORDERED)
// - Types/Structs → GameState class → Replicated state
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UE5_Multi_Shooter/MosesGameState.h"
#include "MosesLobbyGameState.generated.h"

class AMosesPlayerState;
struct FLobbyChatMessage;

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
	bool IsFull() const;
	bool Contains(const FGuid& Pid) const;
	bool IsAllReady() const;

	void EnsureReadySize();
	bool SetReadyByPid(const FGuid& Pid, bool bInReady);

	// FastArray callbacks
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

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameState : public AMosesGameState
{
	GENERATED_BODY()

public:
	AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// =========================================================================
	// Engine
	// =========================================================================
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// =========================================================================
	// Server APIs (Room)
	// =========================================================================
	FGuid Server_CreateRoom(AMosesPlayerState* RequestPS, const FString& RoomTitle, int32 MaxPlayers);
	bool Server_JoinRoom(AMosesPlayerState* RequestPS, const FGuid& RoomId);
	bool Server_JoinRoomWithResult(AMosesPlayerState* RequestPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult);
	void Server_LeaveRoom(AMosesPlayerState* RequestPS);

	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	bool Server_CanStartGame(AMosesPlayerState* HostPS, FString& OutFailReason) const;
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

public:
	// =========================================================================
	// Server APIs (Chat)
	// =========================================================================
	void Server_AddChatMessage(AMosesPlayerState* SenderPS, const FString& Text);

public:
	// =========================================================================
	// Client/UI read-only
	// =========================================================================
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;
	const TArray<FMosesLobbyRoomItem>& GetRooms() const { return RoomList.Items; }
	const TArray<FLobbyChatMessage>& GetChatHistory() const { return ChatHistory; }

public:
	// =========================================================================
	// UI Notify (single entry)
	// =========================================================================
	void NotifyRoomStateChanged_LocalPlayers() const;

protected:
	// =========================================================================
	// RepNotify
	// =========================================================================
	UFUNCTION()
	void OnRep_RoomList();

	UFUNCTION()
	void OnRep_ChatHistory();

private:
	// =========================================================================
	// Internal helpers
	// =========================================================================
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);
	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

private:
	// =========================================================================
	// Logs (server)
	// =========================================================================
	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

private:
	// =========================================================================
	// Misc
	// =========================================================================
	bool IsServerAuth() const;

private:
	// =========================================================================
	// Replicated state
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_RoomList)
	FMosesLobbyRoomList RoomList;

	UPROPERTY(ReplicatedUsing = OnRep_ChatHistory)
	TArray<FLobbyChatMessage> ChatHistory;
};
