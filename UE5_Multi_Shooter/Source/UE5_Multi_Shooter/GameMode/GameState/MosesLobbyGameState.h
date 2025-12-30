#pragma once

#include "MosesGameState.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MosesLobbyGameState.generated.h"

class AMosesPlayerState;

/** Join 실패 사유를 서버가 확정해서 로그/디버그/UI에 쓰기 위한 enum */
UENUM()
enum class EMosesRoomJoinResult : uint8
{
	Ok,
	InvalidRoomId,
	NoRoom,
	RoomFull,
	AlreadyMember,
};

// ----------------------------
// FastArray Item (Room 1개)
// ----------------------------
USTRUCT()
struct FMosesLobbyRoomItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY() FGuid RoomId;
	UPROPERTY() int32 MaxPlayers = 0;

	/** 방장 PersistentId */
	UPROPERTY() FGuid HostPid;

	/** 참가자 PersistentId 목록 */
	UPROPERTY() TArray<FGuid> MemberPids;

	/** ReadyMap 대체: MemberPids와 인덱스 1:1 */
	UPROPERTY() TArray<uint8> MemberReady;

	bool IsFull() const
	{
		return MaxPlayers > 0 && MemberPids.Num() >= MaxPlayers;
	}

	bool IsAllReady() const
	{
		if (MaxPlayers <= 0) return false;
		if (MemberPids.Num() != MaxPlayers) return false;
		if (MemberReady.Num() != MemberPids.Num()) return false;

		for (uint8 bReady : MemberReady)
		{
			if (bReady == 0) return false;
		}
		return true;
	}

	bool SetReadyByPid(const FGuid& Pid, bool bInReady)
	{
		const int32 Index = MemberPids.IndexOfByKey(Pid);
		if (Index == INDEX_NONE) return false;
		if (!MemberReady.IsValidIndex(Index)) return false;

		MemberReady[Index] = bInReady ? 1 : 0;
		return true;
	}

	// ---- FastArray Rep callbacks (클라에서 "받았다" 증명 로그) ----
	void PostReplicatedAdd(const struct FMosesLobbyRoomList& InArraySerializer);
	void PostReplicatedChange(const struct FMosesLobbyRoomList& InArraySerializer);
	void PreReplicatedRemove(const struct FMosesLobbyRoomList& InArraySerializer);
};

// ----------------------------
// FastArray Container
// ----------------------------
USTRUCT()
struct FMosesLobbyRoomList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY() TArray<FMosesLobbyRoomItem> Items;

	/** (Not Replicated) callback 로그용 컨텍스트 */
	UPROPERTY(Transient) TObjectPtr<class AMosesLobbyGameState> Owner = nullptr;

	void SetOwner(class AMosesLobbyGameState* InOwner) { Owner = InOwner; }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(Items, DeltaParams, *this);
	}
};

template<> struct TStructOpsTypeTraits<FMosesLobbyRoomList> : public TStructOpsTypeTraitsBase2<FMosesLobbyRoomList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameState : public AMosesGameState
{
	GENERATED_BODY()

public:
	AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer);

	// ----------------------------
	// Server API (Room Ops)
	// ----------------------------
	FGuid Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers);

	/** Day2: Join 사유 로그를 찍기 위해 Ex 버전으로 분리 */
	bool Server_JoinRoomEx(AMosesPlayerState* JoinPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult);
	bool Server_JoinRoom(AMosesPlayerState* JoinPS, const FGuid& RoomId);

	void Server_LeaveRoom(AMosesPlayerState* PS);

	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;
	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	void Server_InitLobbyIfNeeded();

	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnRep_RepRoomList();

private:
	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

	// Day2 DoD 로그 helpers
	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

	static FGuid GetPidChecked(const AMosesPlayerState* PS);

	bool bLobbyInitialized_DoD = false;

protected:
	// FastArray 컨테이너로 선언해야 .Items / MarkArrayDirty / MarkItemDirty 사용 가능
	UPROPERTY(ReplicatedUsing = OnRep_RepRoomList)
	FMosesLobbyRoomList RepRoomList;
};
