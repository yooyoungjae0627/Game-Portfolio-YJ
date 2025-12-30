// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesLobbyGameState.h"

#include "Net/UnrealNetwork.h"                      
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"  

/*
	생성자
	- UE의 많은 UObject/Actor 계열은 FObjectInitializer 기반 생성자 패턴을 사용한다.
	- Super(ObjectInitializer)를 호출하여 부모쪽 UPROPERTY/컴포넌트 초기화 흐름을 보장한다.
	- 여기서는 특별한 초기화가 없지만, 패턴 유지가 중요하다.
*/
AMosesLobbyGameState::AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/*
	복제 변수 등록
	- "어떤 UPROPERTY를 네트워크로 보낼지" 엔진에 알려주는 리스트를 구성한다.
	- 서버가 값을 업데이트하면, 엔진이 여기 등록된 변수들을 기준으로 클라에 동기화한다.
*/
void AMosesLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// 부모가 등록한 복제 변수들도 유지
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// RepRoomList(FastArray 컨테이너)를 복제 대상으로 등록
	// -> 내부 Items 배열은 FastArray 규칙에 따라 증분 복제됨
	DOREPLIFETIME(AMosesLobbyGameState, RepRoomList);
}

void AMosesLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// FastArray 콜백에서 Owner 기반 NetMode 판단이 필요함
	RepRoomList.SetOwner(this);

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[LobbyGS] PostInit -> RepRoomList Owner set. NetMode=%d"), (int32)GetNetMode());
}

void AMosesLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// FastArray 콜백 로그에서 NetMode/Authority 판단용 컨텍스트 제공
	RepRoomList.SetOwner(this);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGS] BeginPlay -> RepRoomList Owner set. NetMode=%d"), (int32)GetNetMode());
}

/*
	GetPidChecked
	- PlayerState에서 PersistentId를 꺼내는 "안전한 헬퍼"
	- check는 "런타임 논리 오류"를 조기 발견하기 위한 강제 크래시(디버그/개발용)
	- 서버 로직에서는 PS/PersistentId가 반드시 있어야 하므로, 실패하면 그냥 터지게 해서 원인 추적을 쉽게 한다.
*/
FGuid AMosesLobbyGameState::GetPidChecked(const AMosesPlayerState* PS)
{
	check(PS);
	check(PS->PersistentId.IsValid());
	return PS->PersistentId;
}

/*
	FindRoomMutable (수정 가능)
	- RoomId로 방을 찾아서 "수정 가능한 포인터"를 돌려준다.
	- FastArray 아이템을 직접 수정할 때 사용한다.
	- 주의: 반환된 포인터로 수정 후 MarkItemDirty를 꼭 호출해야 클라에 반영됨
*/
FMosesLobbyRoomItem* AMosesLobbyGameState::FindRoomMutable(const FGuid& RoomId)
{
	for (FMosesLobbyRoomItem& It : RepRoomList.Items)
	{
		if (It.RoomId == RoomId)
		{
			return &It;
		}
	}

	return nullptr;
}

/*
	FindRoom (읽기 전용)
	- const 버전. 조건 검사/검증 로직에서 사용
	- "수정하지 않을 때"는 const 버전이 의도가 명확해서 좋다.
*/
const FMosesLobbyRoomItem* AMosesLobbyGameState::FindRoom(const FGuid& RoomId) const
{
	for (const FMosesLobbyRoomItem& It : RepRoomList.Items)
	{
		if (It.RoomId == RoomId)
		{
			return &It;
		}
	}

	return nullptr;
}

/*
	MarkRoomDirty
	- FastArray에서 "특정 아이템이 바뀌었다"를 엔진에게 알림
	- 아이템의 멤버(HostPid/MemberPids/ReadyMap 등)를 변경한 뒤 호출해야
	  그 아이템만 증분 복제되어 클라가 갱신된다.
*/
void AMosesLobbyGameState::MarkRoomDirty(FMosesLobbyRoomItem& Item)
{
	// FastArray는 “바뀐 아이템만” 증분 복제가 가능하다.
	// 변경 후에는 반드시 MarkItemDirty를 호출해야 클라가 갱신된다.
	RepRoomList.MarkItemDirty(Item);
}

/*
	Server_CreateRoom
	- 서버에서 방을 만든다. (check(HasAuthority())로 서버 전용 강제)
	- 정책:
	  1) HostPS가 이미 방에 있으면 먼저 나가고 새 방 생성
	  2) 방 생성 시 Host는 자동 참가(MemberPids), Ready는 false
	  3) FastArray에 "새 아이템 추가"는 MarkArrayDirty가 필요
*/
FGuid AMosesLobbyGameState::Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers)
{
	check(HasAuthority()); // 서버 권한에서만 실행되어야 함

	if (HostPS == nullptr || MaxPlayers <= 0)
	{
		return FGuid(); // 실패 시 Invalid Guid 반환
	}

	// 이미 방에 있으면 먼저 나가게(정책)
	if (HostPS->RoomId.IsValid())
	{
		Server_LeaveRoom(HostPS);
	}

	// 새 방 아이템 구성
	FMosesLobbyRoomItem NewRoom;
	NewRoom.RoomId = FGuid::NewGuid(); // 고유 방 ID 발급
	NewRoom.MaxPlayers = MaxPlayers;

	const FGuid HostPid = GetPidChecked(HostPS);
	NewRoom.HostPid = HostPid;

	// 호스트는 자동으로 방 멤버에 포함
	NewRoom.MemberPids = { HostPid };

	// ReadyMap: (Pid -> Ready 여부)
	// 방 생성 직후 기본 false
	// ❌ Replicated TMap 불가로 제거
	// NewRoom.ReadyMap.Add(HostPid, false);

	// ✅ 추가: ReadyMap 대체 (MemberPids와 인덱스 매칭)
	NewRoom.MemberReady = { 0 };

	// FastArray Items에 추가
	RepRoomList.Items.Add(NewRoom);

	// "아이템 추가/삭제"처럼 배열 구조가 바뀌면 MarkArrayDirty 필요
	// (MarkItemDirty는 '기존 아이템의 내용 변경'에 사용)
	RepRoomList.MarkArrayDirty();

	// PlayerState에도 방 정보/Ready를 서버에서 세팅
	// (PlayerState가 클라에게 개별 플레이어 상태를 전달하는 용도라면,
	//  GameState의 방 리스트와 함께 UI 구성 시 유용)
	HostPS->ServerSetRoom(NewRoom.RoomId, /*bIsHost*/true);
	HostPS->ServerSetReady(false); // 방 생성 시 Ready는 기본 false

	// 빠르게 증거 확보용: FastArray 변경을 즉시 네트워크로 밀어냄
	ForceNetUpdate();

	return NewRoom.RoomId;
}

/*
	Server_JoinRoom
	- 플레이어가 특정 RoomId 방에 참가하는 서버 로직
	- 정책:
	  1) 이미 다른 방에 있으면 먼저 나가기
	  2) 방 존재/정원 체크
	  3) MemberPids/ReadyMap에 추가 후 MarkItemDirty 호출
*/
bool AMosesLobbyGameState::Server_JoinRoom(AMosesPlayerState* JoinPS, const FGuid& RoomId)
{
	check(HasAuthority());

	if (JoinPS == nullptr || RoomId.IsValid() == false)
	{
		return false;
	}

	// 다른 방에 있으면 먼저 나가기(정책)
	if (JoinPS->RoomId.IsValid())
	{
		Server_LeaveRoom(JoinPS);
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (Room == nullptr)
	{
		return false; // 방이 없음
	}

	if (Room->IsFull())
	{
		return false; // 정원 초과
	}

	const FGuid Pid = GetPidChecked(JoinPS);

	// 중복 참가 방지
	if (Room->MemberPids.Contains(Pid) == false)
	{
		Room->MemberPids.Add(Pid);

		// ReadyMap.Add(Pid, false);
		// ✅ 변경: ReadyMap 대신 MemberReady에 0 추가(인덱스 정합성 유지)
		Room->MemberReady.Add(0);

		// 기존 아이템 내용 변경 -> MarkItemDirty
		MarkRoomDirty(*Room);
	}
	else
	{
		// ✅ 추가: 이미 멤버인데 Ready 배열 길이가 깨진 경우를 방어적으로 복구
		// (디버그 상황/핫리로드 등에서 깨질 수 있음)
		const int32 Index = Room->MemberPids.IndexOfByKey(Pid);
		if (Index != INDEX_NONE && Room->MemberReady.Num() != Room->MemberPids.Num())
		{
			Room->MemberReady.SetNum(Room->MemberPids.Num());
			// 기본값 0으로 채움
			MarkRoomDirty(*Room);
		}
	}

	// PlayerState에도 반영
	JoinPS->ServerSetRoom(RoomId, /*bIsHost*/ (Pid == Room->HostPid));
	JoinPS->ServerSetReady(false); // 입장하면 기본 false

	ForceNetUpdate();

	return true;
}

/*
	Server_LeaveRoom
	- 방에서 나가는 서버 로직
	- 흐름:
	  1) 유효성 체크
	  2) 방 찾기
	  3) MemberPids/ReadyMap에서 제거
	  4) 호스트가 나가면 호스트 승계 정책 적용
	  5) MarkItemDirty 후, 비었으면 방 삭제(배열 구조 변경 -> MarkArrayDirty)
	  6) PlayerState 룸/레디 초기화
*/
void AMosesLobbyGameState::Server_LeaveRoom(AMosesPlayerState* PS)
{
	check(HasAuthority());

	if (PS == nullptr)
	{
		return;
	}

	if (PS->RoomId.IsValid() == false)
	{
		return; // 애초에 방에 안 들어가 있음
	}

	const FGuid LeavingRoomId = PS->RoomId;

	FMosesLobbyRoomItem* Room = FindRoomMutable(LeavingRoomId);
	if (Room == nullptr)
	{
		// 방이 사라졌거나 비정상 상태면 PlayerState만 초기화
		PS->ServerSetRoom(FGuid(), false);
		return;
	}

	const FGuid Pid = GetPidChecked(PS);

	// 멤버 및 Ready 제거
	// Room->MemberPids.Remove(Pid);
	// Room->ReadyMap.Remove(Pid);

	// ✅ 변경: MemberPids/MemberReady는 같은 인덱스로 제거해야 정합성 유지
	const int32 Index = Room->MemberPids.IndexOfByKey(Pid);
	if (Index != INDEX_NONE)
	{
		Room->MemberPids.RemoveAt(Index);

		if (Room->MemberReady.IsValidIndex(Index))
		{
			Room->MemberReady.RemoveAt(Index);
		}
		else
		{
			// ✅ 추가: 인덱스 정합성이 깨진 경우, 방어적으로 맞춰준다.
			Room->MemberReady.SetNum(Room->MemberPids.Num());
		}
	}

	// 방장이 나가면 정책:
	// - 남은 인원이 있으면 “첫 번째 멤버”를 새 호스트로 승격
	if (Room->HostPid == Pid)
	{
		if (Room->MemberPids.Num() > 0)
		{
			Room->HostPid = Room->MemberPids[0];
		}
		else
		{
			Room->HostPid = FGuid(); // 빈 방
		}
	}

	// 아이템 내용 변경 반영
	MarkRoomDirty(*Room);

	// 비었으면 방 제거(배열 변경)
	RemoveEmptyRoomIfNeeded(LeavingRoomId);

	// PlayerState 초기화
	PS->ServerSetRoom(FGuid(), false);
	PS->ServerSetReady(false);

	// LeaveRoom으로 인한 FastArray 변경을 즉시 클라이언트로 밀어낸다
	ForceNetUpdate();
}
/*
	RemoveEmptyRoomIfNeeded
	- 방 인원이 0이면 방을 Items 배열에서 제거한다.
	- FastArray에서 RemoveAt으로 아이템 삭제 시 "배열 구조 변경"이므로 MarkArrayDirty 필수
*/
void AMosesLobbyGameState::RemoveEmptyRoomIfNeeded(const FGuid& RoomId)
{
	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (Room == nullptr)
	{
		return;
	}

	if (Room->MemberPids.Num() == 0)
	{
		for (int32 i = 0; i < RepRoomList.Items.Num(); ++i)
		{
			if (RepRoomList.Items[i].RoomId == RoomId)
			{
				RepRoomList.Items.RemoveAt(i);

				// 아이템 삭제 -> 배열이 바뀜 -> MarkArrayDirty
				RepRoomList.MarkArrayDirty();

				// 방 삭제는 구조 변경이므로 즉시 NetUpdate 필요
				ForceNetUpdate();

				return;
			}
		}
	}
}

/*
	Server_SyncReadyFromPlayerState
	- Ready의 "원천(Source of Truth)"을 PlayerState로 두고,
	  그 값을 방 리스트(ReadyMap)에 반영한다.
	- 이유:
	  - Ready는 개별 플레이어 상태이며 PlayerState에 있으면 관리/복제가 자연스럽다.
	  - 방 UI는 GameState의 RoomList(ReadyMap)를 보고 "모든 멤버 Ready" 같은 판단을 할 수 있다.
*/
void AMosesLobbyGameState::Server_SyncReadyFromPlayerState(AMosesPlayerState* PS)
{
	check(HasAuthority());

	if (PS == nullptr || PS->RoomId.IsValid() == false)
	{
		return;
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(PS->RoomId);
	if (Room == nullptr)
	{
		return;
	}

	const FGuid Pid = GetPidChecked(PS);

	// ReadyMap에 이미 존재하는 멤버만 동기화
	// if (Room->ReadyMap.Contains(Pid))
	// {
	// 	Room->ReadyMap[Pid] = PS->bReady;
	// 	MarkRoomDirty(*Room);
	// }

	// ✅ 변경: ReadyMap 대신 MemberReady(인덱스 매칭)로 동기화
	// - Pid가 멤버가 아니면 아무 것도 하지 않음(정책 동일)
	if (Room->SetReadyByPid(Pid, PS->bReady))
	{
		MarkRoomDirty(*Room);
	}
}

/*
	Server_CanStartMatch
	- 매치 시작 가능 여부를 "서버에서 최종 판정"
	- OutReason은 디버그/로그/UI 메시지용
	- 체크 순서:
	  1) 방 존재
	  2) MaxPlayers 유효
	  3) 정원 충족
	  4) 전원 Ready
*/
bool AMosesLobbyGameState::Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const
{
	check(HasAuthority());

	const FMosesLobbyRoomItem* Room = FindRoom(RoomId);
	if (Room == nullptr)
	{
		OutReason = TEXT("NoRoom");
		return false;
	}

	if (Room->MaxPlayers <= 0)
	{
		OutReason = TEXT("InvalidMaxPlayers");
		return false;
	}

	if (Room->MemberPids.Num() != Room->MaxPlayers)
	{
		OutReason = TEXT("NotFull");
		return false;
	}

	if (Room->IsAllReady() == false)
	{
		OutReason = TEXT("NotAllReady");
		return false;
	}

	OutReason = TEXT("OK");
	return true;
}

static const TCHAR* MosesNetModeToStr(ENetMode NM)
{
	switch (NM)
	{
	case NM_DedicatedServer: return TEXT("DS");
	case NM_ListenServer:    return TEXT("LS");
	case NM_Client:          return TEXT("CL");
	default:                 return TEXT("ST");
	}
}

void FMosesLobbyRoomItem::PostReplicatedAdd(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[RepRoomList][%s] PostReplicatedAdd Room=%s Max=%d Members=%d"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		MaxPlayers,
		MemberPids.Num());
}

void FMosesLobbyRoomItem::PostReplicatedChange(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	// ✅ 변경: ReadyMap.Num() 대신 MemberReady.Num()
	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[RepRoomList][%s] PostReplicatedChange Room=%s Members=%d Ready=%d Host=%s"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		MemberPids.Num(),
		MemberReady.Num(),
		*HostPid.ToString(EGuidFormats::DigitsWithHyphens));
}

void FMosesLobbyRoomItem::PreReplicatedRemove(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[RepRoomList][%s] PreReplicatedRemove Room=%s"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens));
}

void AMosesLobbyGameState::Server_InitLobbyIfNeeded()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bLobbyInitialized_DoD)
	{
		return; // 1회 보장
	}
	bLobbyInitialized_DoD = true;

	// DoD 3번 라인
	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM] Lobby Initialized"));

	// 여기서 “기본 룸 생성” 같은 정책을 넣고 싶으면 Day2 이후 확장:
	// - 예: Server_CreateRoom(HostPS, MaxPlayers)는 HostPS가 필요하니
	//   Dedicated Server 단독 부트에서는 그냥 '로비 시스템 준비 완료'만 찍는 게 깔끔함.
}