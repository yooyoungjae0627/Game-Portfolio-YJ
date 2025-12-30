#include "MosesLobbyGameState.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

// ----------------------------
// NetMode 문자열 헬퍼(로그 가독성)
// ----------------------------
// NOTE: 로비/복제 디버깅은 "서버/클라 로그 비교"가 핵심이라 NetMode 표기가 중요하다.
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

static const TCHAR* MosesNetModeToStr_UObject(const UObject* Obj)
{
	if (!Obj) return TEXT("?");
	if (const UWorld* W = Obj->GetWorld())
	{
		return MosesNetModeToStr(W->GetNetMode());
	}
	return TEXT("?");
}

AMosesLobbyGameState::AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AMosesLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// FastArray 컨테이너 자체를 Replicate 해야 델타 복제가 동작한다.
	DOREPLIFETIME(AMosesLobbyGameState, RepRoomList);
}

void AMosesLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Owner는 "복제 콜백에서 로그 컨텍스트"를 찍기 위한 비복제 참조
	RepRoomList.SetOwner(this);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGS] PostInit -> RepRoomList Owner set. NetMode=%d"), (int32)GetNetMode());
}

void AMosesLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// BeginPlay에도 한 번 더(Travel/재생성 케이스에서 안전)
	RepRoomList.SetOwner(this);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGS] BeginPlay -> RepRoomList Owner set. NetMode=%d"), (int32)GetNetMode());
}

// ----------------------------
// Item util (구조 정합성)
// ----------------------------

bool FMosesLobbyRoomItem::IsFull() const
{
	return MaxPlayers > 0 && MemberPids.Num() >= MaxPlayers;
}

bool FMosesLobbyRoomItem::IsAllReady() const
{
	// 정책: 정원이 정확히 차야만 Start 가능
	if (MaxPlayers <= 0) return false;
	if (MemberPids.Num() != MaxPlayers) return false;

	// Ready 배열은 MemberPids와 1:1 정합성이 보장되어야 한다.
	if (MemberReady.Num() != MemberPids.Num()) return false;

	for (uint8 bReady : MemberReady)
	{
		if (bReady == 0) return false;
	}
	return true;
}

bool FMosesLobbyRoomItem::SetReadyByPid(const FGuid& Pid, bool bInReady)
{
	// 인덱스 기반 매칭(정합성 유지가 핵심)
	const int32 Index = MemberPids.IndexOfByKey(Pid);
	if (Index == INDEX_NONE) return false;
	if (!MemberReady.IsValidIndex(Index)) return false;

	MemberReady[Index] = bInReady ? 1 : 0;
	return true;
}

FGuid AMosesLobbyGameState::GetPidChecked(const AMosesPlayerState* PS)
{
	// PersistentId는 Room 시스템의 Primary Key 역할
	// 유효하지 않으면 RoomList 정합성이 깨지므로 강제 체크
	check(PS);
	check(PS->PersistentId.IsValid());
	return PS->PersistentId;
}

// ----------------------------
// Find / Dirty
// ----------------------------

FMosesLobbyRoomItem* AMosesLobbyGameState::FindRoomMutable(const FGuid& RoomId)
{
	for (FMosesLobbyRoomItem& It : RepRoomList.Items)
	{
		if (It.RoomId == RoomId) return &It;
	}
	return nullptr;
}

const FMosesLobbyRoomItem* AMosesLobbyGameState::FindRoom(const FGuid& RoomId) const
{
	for (const FMosesLobbyRoomItem& It : RepRoomList.Items)
	{
		if (It.RoomId == RoomId) return &It;
	}
	return nullptr;
}

void AMosesLobbyGameState::MarkRoomDirty(FMosesLobbyRoomItem& Item)
{
	// FastArray 아이템 변경 시 반드시 호출(델타 복제 반영)
	RepRoomList.MarkItemDirty(Item);
}

// ----------------------------
// Server logs (DoD/디버그)
// ----------------------------

void AMosesLobbyGameState::LogRoom_Create(const FMosesLobbyRoomItem& Room) const
{
	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][S] Create RoomId=%s Max=%d HostPid=%s Members=%d"),
		MosesNetModeToStr_UObject(this),
		*Room.RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		Room.MaxPlayers,
		*Room.HostPid.ToString(EGuidFormats::DigitsWithHyphens),
		Room.MemberPids.Num());
}

void AMosesLobbyGameState::LogRoom_JoinAccepted(const FMosesLobbyRoomItem& Room) const
{
	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][S] Join OK RoomId=%s Now=%d/%d HostPid=%s"),
		MosesNetModeToStr_UObject(this),
		*Room.RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		Room.MemberPids.Num(),
		Room.MaxPlayers,
		*Room.HostPid.ToString(EGuidFormats::DigitsWithHyphens));
}

void AMosesLobbyGameState::LogRoom_JoinRejected(EMosesRoomJoinResult Reason, const FGuid& RoomId, const AMosesPlayerState* JoinPS) const
{
	const FGuid JoinPid = JoinPS ? JoinPS->PersistentId : FGuid();
	const FMosesLobbyRoomItem* Room = FindRoom(RoomId);

	const int32 Cur = Room ? Room->MemberPids.Num() : -1;
	const int32 Max = Room ? Room->MaxPlayers : -1;

	const TCHAR* ReasonStr = TEXT("Unknown");
	switch (Reason)
	{
	case EMosesRoomJoinResult::RoomFull:      ReasonStr = TEXT("RoomFull"); break;
	case EMosesRoomJoinResult::NoRoom:        ReasonStr = TEXT("NoRoom"); break;
	case EMosesRoomJoinResult::InvalidRoomId: ReasonStr = TEXT("InvalidRoomId"); break;
	case EMosesRoomJoinResult::AlreadyMember: ReasonStr = TEXT("AlreadyMember"); break;
	default: break;
	}

	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][S] Join REJECT %s RoomId=%s Cur=%d Max=%d JoinPid=%s"),
		MosesNetModeToStr_UObject(this),
		ReasonStr,
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		Cur, Max,
		*JoinPid.ToString(EGuidFormats::DigitsWithHyphens));
}

// ----------------------------
// Server API : Create / Join / Leave
// ----------------------------

FGuid AMosesLobbyGameState::Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers)
{
	check(HasAuthority());

	if (!HostPS || MaxPlayers <= 0)
	{
		return FGuid();
	}

	// 정책: 이미 방에 속해 있으면 먼저 이탈(단일 소속 보장)
	if (HostPS->RoomId.IsValid())
	{
		Server_LeaveRoom(HostPS);
	}

	// Room 생성
	FMosesLobbyRoomItem NewRoom;
	NewRoom.RoomId = FGuid::NewGuid();
	NewRoom.MaxPlayers = MaxPlayers;

	// Host 등록(첫 멤버)
	const FGuid HostPid = GetPidChecked(HostPS);
	NewRoom.HostPid = HostPid;
	NewRoom.MemberPids = { HostPid };
	NewRoom.MemberReady = { 0 }; // 기본 NotReady

	RepRoomList.Items.Add(NewRoom);
	RepRoomList.MarkArrayDirty();   // 아이템 추가/삭제는 ArrayDirty

	// PS에 서버 authoritative 상태 반영
	HostPS->ServerSetRoom(NewRoom.RoomId, /*bIsHost*/ true);
	HostPS->ServerSetReady(false);

	// 즉시 복제 푸시(테스트/디버그에 유리)
	ForceNetUpdate();

	LogRoom_Create(NewRoom);
	return NewRoom.RoomId;
}

bool AMosesLobbyGameState::Server_JoinRoomEx(AMosesPlayerState* JoinPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult)
{
	check(HasAuthority());
	OutResult = EMosesRoomJoinResult::Ok;

	if (!JoinPS)
	{
		OutResult = EMosesRoomJoinResult::NoRoom;
		return false;
	}

	if (!RoomId.IsValid())
	{
		OutResult = EMosesRoomJoinResult::InvalidRoomId;
		return false;
	}

	// 정책: 다른 방에 있으면 먼저 이탈(단일 소속)
	if (JoinPS->RoomId.IsValid())
	{
		Server_LeaveRoom(JoinPS);
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (!Room)
	{
		OutResult = EMosesRoomJoinResult::NoRoom;
		return false;
	}

	const FGuid Pid = GetPidChecked(JoinPS);

	// 중복 join 방지
	if (Room->MemberPids.Contains(Pid))
	{
		OutResult = EMosesRoomJoinResult::AlreadyMember;
		return false;
	}

	// 정원 체크
	if (Room->IsFull())
	{
		OutResult = EMosesRoomJoinResult::RoomFull;
		return false;
	}

	// 참가자 추가 + Ready 배열 정합성 유지
	Room->MemberPids.Add(Pid);
	Room->MemberReady.Add(0);

	MarkRoomDirty(*Room);

	// PS 상태 업데이트
	JoinPS->ServerSetRoom(RoomId, /*bIsHost*/ (Pid == Room->HostPid));
	JoinPS->ServerSetReady(false);

	ForceNetUpdate();
	return true;
}

bool AMosesLobbyGameState::Server_JoinRoom(AMosesPlayerState* JoinPS, const FGuid& RoomId)
{
	EMosesRoomJoinResult Result = EMosesRoomJoinResult::Ok;
	const bool bOk = Server_JoinRoomEx(JoinPS, RoomId, Result);

	if (!bOk)
	{
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	if (const FMosesLobbyRoomItem* Room = FindRoom(RoomId))
	{
		LogRoom_JoinAccepted(*Room);
	}
	return true;
}

void AMosesLobbyGameState::Server_LeaveRoom(AMosesPlayerState* PS)
{
	check(HasAuthority());
	if (!PS) return;
	if (!PS->RoomId.IsValid()) return;

	const FGuid LeavingRoomId = PS->RoomId;
	FMosesLobbyRoomItem* Room = FindRoomMutable(LeavingRoomId);

	// Room이 없으면 PS만 정리(데이터 불일치 복구)
	if (!Room)
	{
		PS->ServerSetRoom(FGuid(), false);
		PS->ServerSetReady(false);
		return;
	}

	const FGuid Pid = GetPidChecked(PS);

	// MemberPids / MemberReady 인덱스 정합성 유지하면서 제거
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
			// 정합성 깨짐 복구(최악 대비)
			Room->MemberReady.SetNum(Room->MemberPids.Num());
		}
	}

	// 호스트 승계: 호스트가 나가면 첫 멤버로 승계(정책)
	if (Room->HostPid == Pid)
	{
		Room->HostPid = (Room->MemberPids.Num() > 0) ? Room->MemberPids[0] : FGuid();
	}

	MarkRoomDirty(*Room);

	// 빈 방은 삭제 정책
	RemoveEmptyRoomIfNeeded(LeavingRoomId);

	// PS 리셋(로비 복귀 정책과 연동)
	PS->ServerSetRoom(FGuid(), false);
	PS->ServerSetReady(false);

	// DoD Leave 로그
	if (const FMosesLobbyRoomItem* After = FindRoom(LeavingRoomId))
	{
		UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][S] Leave RoomId=%s Now=%d/%d LeavingPid=%s NewHost=%s"),
			MosesNetModeToStr_UObject(this),
			*LeavingRoomId.ToString(EGuidFormats::DigitsWithHyphens),
			After->MemberPids.Num(),
			After->MaxPlayers,
			*Pid.ToString(EGuidFormats::DigitsWithHyphens),
			*After->HostPid.ToString(EGuidFormats::DigitsWithHyphens));
	}
	else
	{
		UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][S] Leave -> RoomRemoved RoomId=%s LeavingPid=%s"),
			MosesNetModeToStr_UObject(this),
			*LeavingRoomId.ToString(EGuidFormats::DigitsWithHyphens),
			*Pid.ToString(EGuidFormats::DigitsWithHyphens));
	}

	ForceNetUpdate();
}

void AMosesLobbyGameState::RemoveEmptyRoomIfNeeded(const FGuid& RoomId)
{
	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (!Room) return;

	if (Room->MemberPids.Num() == 0)
	{
		// 아이템 제거는 MarkArrayDirty가 필요
		for (int32 i = 0; i < RepRoomList.Items.Num(); ++i)
		{
			if (RepRoomList.Items[i].RoomId == RoomId)
			{
				RepRoomList.Items.RemoveAt(i);
				RepRoomList.MarkArrayDirty();
				ForceNetUpdate();
				return;
			}
		}
	}
}

// ----------------------------
// Ready sync / Start 조건
// ----------------------------

void AMosesLobbyGameState::Server_SyncReadyFromPlayerState(AMosesPlayerState* PS)
{
	check(HasAuthority());
	if (!PS || !PS->RoomId.IsValid())
	{
		return;
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(PS->RoomId);
	if (!Room)
	{
		return;
	}

	const FGuid Pid = GetPidChecked(PS);
	if (Room->SetReadyByPid(Pid, PS->bReady))
	{
		MarkRoomDirty(*Room);
	}
}

bool AMosesLobbyGameState::Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const
{
	check(HasAuthority());

	const FMosesLobbyRoomItem* Room = FindRoom(RoomId);
	if (!Room) { OutReason = TEXT("NoRoom"); return false; }
	if (Room->MaxPlayers <= 0) { OutReason = TEXT("InvalidMaxPlayers"); return false; }
	if (Room->MemberPids.Num() != Room->MaxPlayers) { OutReason = TEXT("NotFull"); return false; }
	if (!Room->IsAllReady()) { OutReason = TEXT("NotAllReady"); return false; }

	OutReason = TEXT("OK");
	return true;
}

// ----------------------------
// Client-side replication callbacks
// ----------------------------

void FMosesLobbyRoomItem::PostReplicatedAdd(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][C] RepRoom ADD RoomId=%s Members=%d/%d"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		MemberPids.Num(),
		MaxPlayers);
}

void FMosesLobbyRoomItem::PostReplicatedChange(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][C] RepRoom CHG RoomId=%s Members=%d/%d Host=%s"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		MemberPids.Num(),
		MaxPlayers,
		*HostPid.ToString(EGuidFormats::DigitsWithHyphens));
}

void FMosesLobbyRoomItem::PreReplicatedRemove(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][C] RepRoom DEL RoomId=%s"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens));
}

// ----------------------------
// Lobby init / OnRep hook
// ----------------------------

void AMosesLobbyGameState::Server_InitLobbyIfNeeded()
{
	// 로비 초기화는 서버에서 1회만
	if (!HasAuthority()) return;
	if (bLobbyInitialized_DoD) return;

	bLobbyInitialized_DoD = true;
	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM] Lobby Initialized"));
}

void AMosesLobbyGameState::OnRep_RepRoomList()
{
	// RepRoomList가 변경되면 "클라 UI 갱신"을 LocalPlayerSubsystem에 위임한다.
	// (GameState는 UI를 직접 만지지 않는다 / Subsystem이 클라 UI 책임)
	if (UMosesGameInstance* GI = UMosesGameInstance::Get(this))
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
		for (ULocalPlayer* LP : LocalPlayers)
		{
			if (!LP) continue;

			if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
			{
				Subsys->NotifyRoomStateChanged();
			}
		}
	}
}
