// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UE5_Multi_Shooter/GameMode/GameState/MosesGameState.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MosesLobbyGameState.generated.h"

class AMosesPlayerState;

// ----------------------------
// FastArray Item (Room 1개)
// ----------------------------
USTRUCT()
struct FMosesLobbyRoomItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid RoomId;

	UPROPERTY()
	int32 MaxPlayers = 0;

	/** 방장 PersistentId */
	UPROPERTY()
	FGuid HostPid;

	/** 참가자 PersistentId 목록 (정렬/중복 방지 필요) */
	UPROPERTY()
	TArray<FGuid> MemberPids;

	/** 레디 상태 (Pid → Ready) */
	// ❌ UE 기본 Replication / FastArray에서 TMap은 지원되지 않는다.
	// UPROPERTY() 
	// TMap<FGuid, bool> ReadyMap;

	// ✅ 추가: ReadyMap 대체 (Replicated-safe)
	// - MemberPids와 "인덱스 1:1"로 매칭되는 Ready 배열
	// - MemberReady[i] == 1  -> MemberPids[i] 가 Ready
	// - MemberReady[i] == 0  -> Not Ready
	// - Join/Leave 시 MemberPids와 같이 Add/RemoveAt 해야 "데이터 정합성"이 유지된다.
	UPROPERTY()
	TArray<uint8> MemberReady;

	bool IsFull() const
	{
		return MaxPlayers > 0 && MemberPids.Num() >= MaxPlayers;
	}

	bool IsAllReady() const
	{
		if (MaxPlayers <= 0)
		{
			return false;
		}

		if (MemberPids.Num() != MaxPlayers)
		{
			return false;
		}

		// ✅ 추가: Ready 배열이 MemberPids와 길이가 다르면 데이터가 깨진 상태로 보고 실패 처리
		if (MemberReady.Num() != MemberPids.Num())
		{
			return false;
		}

		// ✅ 변경: ReadyMap(TMap) 대신 MemberReady 배열로 판정
		for (uint8 bReady : MemberReady)
		{
			if (bReady == 0)
			{
				return false;
			}
		}

		return true;
	}

	// ✅ 추가: 룸 내부에서 특정 Pid의 Ready를 수정하는 헬퍼
	// - Pid가 멤버가 아니면 false
	// - 성공 시 true
	bool SetReadyByPid(const FGuid& Pid, bool bInReady)
	{
		const int32 Index = MemberPids.IndexOfByKey(Pid);
		if (Index == INDEX_NONE)
		{
			return false;
		}

		if (!MemberReady.IsValidIndex(Index))
		{
			// 인덱스 정합성이 깨진 경우(디버그 방어)
			return false;
		}

		MemberReady[Index] = bInReady ? 1 : 0;
		return true;
	}

	// ✅ 추가: 특정 Pid의 Ready 조회 헬퍼
	bool GetReadyByPid(const FGuid& Pid, bool& OutReady) const
	{
		const int32 Index = MemberPids.IndexOfByKey(Pid);
		if (Index == INDEX_NONE || !MemberReady.IsValidIndex(Index))
		{
			return false;
		}

		OutReady = (MemberReady[Index] != 0);
		return true;
	}

	// ---- FastArray Rep Callbacks (Client-side receive proof) ----
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

	UPROPERTY()
	TArray<FMosesLobbyRoomItem> Items;

	/** (Not Replicated) Local pointer for logging / netmode context */
	UPROPERTY(Transient)
	TObjectPtr<class AMosesLobbyGameState> Owner = nullptr;

	void SetOwner(class AMosesLobbyGameState* InOwner)
	{
		Owner = InOwner;
	}

	/** FastArray Replication */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMosesLobbyRoomItem, FMosesLobbyRoomList>(Items, DeltaParams, *this);
	}
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
	AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer);

	// ----------------------------
	// Server API (Room Ops)
	// ----------------------------
	FGuid Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers);
	bool  Server_JoinRoom(AMosesPlayerState* JoinPS, const FGuid& RoomId);
	void  Server_LeaveRoom(AMosesPlayerState* PS);

	/** (Server) Start 가능 조건 체크 */
	bool Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const;

	/** (Server) 특정 PS의 Ready 상태를 룸에 반영 (PS->bReady 기반) */
	void Server_SyncReadyFromPlayerState(AMosesPlayerState* PS);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

private:
	/** 모든 클라에게 복제되는 “방 목록” */
	UPROPERTY(Replicated)
	FMosesLobbyRoomList RepRoomList;

	// 내부 헬퍼
	FMosesLobbyRoomItem* FindRoomMutable(const FGuid& RoomId);
	const FMosesLobbyRoomItem* FindRoom(const FGuid& RoomId) const;

	void MarkRoomDirty(FMosesLobbyRoomItem& Item);
	void RemoveEmptyRoomIfNeeded(const FGuid& RoomId);

	/** (서버) PS의 PersistentId를 안전하게 얻기 */
	static FGuid GetPidChecked(const AMosesPlayerState* PS);

};
