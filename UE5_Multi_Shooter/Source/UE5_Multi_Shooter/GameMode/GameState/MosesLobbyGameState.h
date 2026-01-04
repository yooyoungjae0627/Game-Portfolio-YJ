// MosesLobbyGameState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MosesLobbyGameState.generated.h"

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

protected:
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

private:
	// ---------------------------
	// Replicated state
	// ---------------------------
	UPROPERTY(ReplicatedUsing = OnRep_RoomList)
	FMosesLobbyRoomList RoomList;
};
