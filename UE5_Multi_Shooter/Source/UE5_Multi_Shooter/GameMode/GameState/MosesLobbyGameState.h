#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Net/UnrealNetwork.h"
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

	UPROPERTY() 
	FGuid RoomId;

	UPROPERTY() 
	int32 MaxPlayers = 0;

	UPROPERTY() 
	FGuid HostPid;

	UPROPERTY() 
	TArray<FGuid> MemberPids;

	UPROPERTY() 
	TArray<uint8> MemberReady;

	bool IsFull() const;
	bool Contains(const FGuid& Pid) const;
	bool IsAllReady() const;
	void EnsureReadySize();
	bool SetReadyByPid(const FGuid& Pid, bool bInReady);

	void PostReplicatedAdd(const struct FMosesLobbyRoomList& InArraySerializer);
	void PostReplicatedChange(const struct FMosesLobbyRoomList& InArraySerializer);
	void PreReplicatedRemove(const struct FMosesLobbyRoomList& InArraySerializer);
};

USTRUCT()
struct FMosesLobbyRoomList : public FFastArraySerializer
{
	GENERATED_BODY()
	void SetOwner(class AMosesLobbyGameState* InOwner) { Owner = InOwner; }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(Items, DeltaParams, *this);
	}

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
	FGuid Server_CreateRoom(AMosesPlayerState* RequestPS, int32 MaxPlayers);
	bool Server_JoinRoom(AMosesPlayerState* RequestPS, const FGuid& RoomId);
	void Server_LeaveRoom(AMosesPlayerState* RequestPS);

	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	/**
	 * ✅ 너 GameMode가 지금 호출하는 시그니처 그대로 제공
	 * - HostPS가 유효하고
	 * - HostPS->RoomId로 방을 찾아서
	 * - Start 조건을 검사
	 */
	bool Server_CanStartGame(AMosesPlayerState* HostPS, FString& OutFailReason) const;

	/**
	 * 내부 구현용(정책 검사)
	 * - RoomId 기준으로 Start 가능 검사
	 */
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

	// ---------------------------
	// Client/UI Read-only accessors
	// ---------------------------
	/** ✅ UI에서 조회해야 하므로 public으로 둔다(변경은 서버 API만) */
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;

	// 개발자 주석:
	// - UI(ListView)에서 RoomList를 읽기 위한 읽기 전용 accessor
	const TArray<FMosesLobbyRoomItem>& GetRooms() const { return RoomList.Items; }

protected:
	// ---------------------------
	// RepNotify
	// ---------------------------
	UFUNCTION()
	void OnRep_RoomList();

private:
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);

	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoomList)
	FMosesLobbyRoomList RoomList;
};
