#pragma once

#include "MosesGameState.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MosesLobbyGameState.generated.h"

class AMosesPlayerState;
class ULocalPlayer;

/**
 * EMosesRoomJoinResult
 * - Join 실패 사유를 서버가 "확정"해서 남기는 용도
 * - 디버그 로그 / UI 에러 메시지 / QA 재현에 사용
 */
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
// FastArray Item : Room 1개
// ----------------------------

/**
 * FMosesLobbyRoomItem
 * - Room 단위로 복제되는 데이터(서버 authoritative)
 * - FastArray 아이템이므로:
 *   · 서버: 수정 후 MarkItemDirty / MarkArrayDirty 필수
 *   · 클라: PostReplicatedAdd/Change/Remove에서 "받았다" 로그 증명 가능
 *
 * 데이터 정책:
 * - MemberReady는 MemberPids와 인덱스 1:1 고정(정합성 중요)
 * - HostPid는 현재 방장 PersistentId (호스트 이탈 시 승계)
 */
USTRUCT()
struct FMosesLobbyRoomItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY() FGuid RoomId;
	UPROPERTY() int32 MaxPlayers = 0;

	/** 방장 PersistentId (서버가 관리) */
	UPROPERTY() FGuid HostPid;

	/** 참가자 PersistentId 목록 (서버가 관리) */
	UPROPERTY() TArray<FGuid> MemberPids;

	/** Ready 상태 배열: MemberPids와 동일 인덱스(1:1) */
	UPROPERTY() TArray<uint8> MemberReady;

	/** 현재 인원이 MaxPlayers를 충족했는지 */
	bool IsFull() const;

	/** 정원 충족 + 모든 인원이 Ready인지 */
	bool IsAllReady() const;

	/** 특정 Pid의 Ready 값을 갱신(인덱스 매칭 기반) */
	bool SetReadyByPid(const FGuid& Pid, bool bInReady);

	// ---- FastArray Rep callbacks ----
	// 클라에서 "복제가 실제로 왔다" 증거 로그를 남기는 용도
	void PostReplicatedAdd(const struct FMosesLobbyRoomList& InArraySerializer);
	void PostReplicatedChange(const struct FMosesLobbyRoomList& InArraySerializer);
	void PreReplicatedRemove(const struct FMosesLobbyRoomList& InArraySerializer);
};

// ----------------------------
// FastArray Container : RoomList
// ----------------------------

/**
 * FMosesLobbyRoomList
 * - FastArray 컨테이너(RepRoomList)
 * - NetDeltaSerialize를 구현해야 FastArray 델타 복제가 동작한다.
 *
 * Owner 포인터:
 * - 복제 콜백(PostReplicatedAdd 등)에서 NetMode/로그 컨텍스트를 찍기 위한 "비복제" 참조
 * - PostInitializeComponents/BeginPlay에서 SetOwner를 반드시 해줄 것
 */
USTRUCT()
struct FMosesLobbyRoomList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY() TArray<FMosesLobbyRoomItem> Items;

	UPROPERTY(Transient) TObjectPtr<class AMosesLobbyGameState> Owner = nullptr;

	void SetOwner(class AMosesLobbyGameState* InOwner) { Owner = InOwner; }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(
			Items, DeltaParams, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FMosesLobbyRoomList> : public TStructOpsTypeTraitsBase2<FMosesLobbyRoomList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * AMosesLobbyGameState
 *
 * 책임(서버 authoritative 상태 + 클라 복제):
 * - Room CRUD(Create/Join/Leave) + Ready 동기화
 * - StartGame 가능 조건 판단(서버에서만)
 * - FastArray 기반으로 룸 리스트를 델타 복제
 *
 * 역할 분리:
 * - GameMode: "룰 트리거"(Start → Travel)
 * - GameState: "상태 소유/복제"(RoomList, Ready, Host, Members)
 * - UI(LocalPlayerSubsystem/Widget): Rep에 반응해서 표시만 갱신
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameState : public AMosesGameState
{
	GENERATED_BODY()

public:
	AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer);

	// ----------------------------
	// Server API (Room Ops)
	// ----------------------------

	/** HostPS가 새 Room 생성(기존 방 소속이면 Leave 후 생성) */
	FGuid Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers);

	/** Join 검증 + 실패 사유 반환(로그/디버그/UI 확장용) */
	bool Server_JoinRoomEx(AMosesPlayerState* JoinPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult);

	/** Join 수행(Ex 호출 + DoD 로그) */
	bool Server_JoinRoom(AMosesPlayerState* JoinPS, const FGuid& RoomId);

	/** PS가 속한 Room에서 제거 + Host 승계 + 빈 방 정리 */
	void Server_LeaveRoom(AMosesPlayerState* PS);

	/** StartGame 가능 조건 판정(서버에서만) */
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

	/** Ready의 Source of Truth(PlayerState)를 RoomList(MemberReady)로 동기화 */
	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

	/** 로비 초기화 1회 보장(DoD 로그/Phase 확정 타이밍에서 호출) */
	void Server_InitLobbyIfNeeded();

	// ----------------------------
	// Read helpers
	// ----------------------------
	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

protected:
	/** RepRoomList 갱신 시 클라 UI 갱신 트리거 */
	UFUNCTION()
	void OnRep_RepRoomList();

private:
	// ----------------------------
	// Internal helpers
	// ----------------------------

	/** Item 변경을 FastArray 델타 복제에 반영(서버에서만 호출) */
	void MarkRoomDirty(FMosesLobbyRoomItem& Item);

	/** 인원 0명인 방은 삭제 정책(서버 authoritative) */
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

	// Day2 DoD 로그 helpers(서버에서만)
	void LogRoom_Create(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const;
	void LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const;

	/** PersistentId 검증(반드시 유효해야 함) */
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

private:
	/** 로비 초기화 1회 보장 플래그 */
	bool bLobbyInitialized_DoD = false;

protected:
	/** RoomList FastArray(복제) */
	UPROPERTY(ReplicatedUsing = OnRep_RepRoomList)
	FMosesLobbyRoomList RepRoomList;
};
