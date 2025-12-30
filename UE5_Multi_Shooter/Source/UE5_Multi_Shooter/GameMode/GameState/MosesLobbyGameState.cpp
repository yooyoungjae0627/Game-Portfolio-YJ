#include "MosesLobbyGameState.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

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
	if (!Obj)
	{
		return TEXT("?");
	}
	
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
	DOREPLIFETIME(AMosesLobbyGameState, RepRoomList);
}

void AMosesLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RepRoomList.SetOwner(this);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGS] PostInit -> RepRoomList Owner set. NetMode=%d"), (int32)GetNetMode());
}

void AMosesLobbyGameState::BeginPlay()
{
	Super::BeginPlay();
	RepRoomList.SetOwner(this);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGS] BeginPlay -> RepRoomList Owner set. NetMode=%d"), (int32)GetNetMode());
}

FGuid AMosesLobbyGameState::GetPidChecked(const AMosesPlayerState* PS)
{
	check(PS);
	check(PS->PersistentId.IsValid());
	return PS->PersistentId;
}

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
	RepRoomList.MarkItemDirty(Item);
}

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


FGuid AMosesLobbyGameState::Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers)
{
	check(HasAuthority());

	if (!HostPS || MaxPlayers <= 0)
	{
		return FGuid();
	}
		
	// 정책: 이미 방 있으면 먼저 나감
	if (HostPS->RoomId.IsValid())
	{
		Server_LeaveRoom(HostPS);
	}

	FMosesLobbyRoomItem NewRoom;
	NewRoom.RoomId = FGuid::NewGuid();
	NewRoom.MaxPlayers = MaxPlayers;

	const FGuid HostPid = GetPidChecked(HostPS);
	NewRoom.HostPid = HostPid;
	NewRoom.MemberPids = { HostPid };
	NewRoom.MemberReady = { 0 }; // 기본 NotReady

	RepRoomList.Items.Add(NewRoom);
	RepRoomList.MarkArrayDirty();

	HostPS->ServerSetRoom(NewRoom.RoomId, /*bIsHost*/true);
	HostPS->ServerSetReady(false);

	ForceNetUpdate();

	// ✅ Day2 DoD 로그
	LogRoom_Create(NewRoom);

	return NewRoom.RoomId;
}

/** Day2: Join 검증 + 사유 반환 */
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

	// 정책: 다른 방에 있으면 먼저 나감
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

	// 정책: 중복 join 방지
	if (Room->MemberPids.Contains(Pid))
	{
		OutResult = EMosesRoomJoinResult::AlreadyMember;
		return false;
	}

	if (Room->IsFull())
	{
		OutResult = EMosesRoomJoinResult::RoomFull;
		return false;
	}

	Room->MemberPids.Add(Pid);
	Room->MemberReady.Add(0);
	MarkRoomDirty(*Room);

	JoinPS->ServerSetRoom(RoomId, /*bIsHost*/ (Pid == Room->HostPid));
	JoinPS->ServerSetReady(false);

	ForceNetUpdate();
	return true;
}

/** Day2: Ex 호출 + DoD 로그 */
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

	if (!Room)
	{
		PS->ServerSetRoom(FGuid(), false);
		return;
	}

	const FGuid Pid = GetPidChecked(PS);

	// MemberPids/MemberReady 인덱스 정합성 유지 제거
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
			Room->MemberReady.SetNum(Room->MemberPids.Num());
		}
	}

	// 호스트 승계
	if (Room->HostPid == Pid)
	{
		Room->HostPid = (Room->MemberPids.Num() > 0) ? Room->MemberPids[0] : FGuid();
	}

	MarkRoomDirty(*Room);
	RemoveEmptyRoomIfNeeded(LeavingRoomId);

	PS->ServerSetRoom(FGuid(), false);
	PS->ServerSetReady(false);

	// ✅ Day2 DoD: Leave 로그 (서버에서만)
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

void AMosesLobbyGameState::Server_SyncReadyFromPlayerState(AMosesPlayerState* PS)
{
	check(HasAuthority());
	if (!PS || !PS->RoomId.IsValid()) return;

	FMosesLobbyRoomItem* Room = FindRoomMutable(PS->RoomId);
	if (!Room) return;

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

void FMosesLobbyRoomItem::PostReplicatedAdd(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesRoom, Log,
		TEXT("[ROOM][%s][C] RepRoom ADD RoomId=%s Members=%d/%d"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		MemberPids.Num(),
		MaxPlayers);

}

void FMosesLobbyRoomItem::PostReplicatedChange(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesRoom, Log,
		TEXT("[ROOM][%s][C] RepRoom CHG RoomId=%s Members=%d/%d Host=%s"),
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

	UE_LOG(LogMosesRoom, Log,
		TEXT("[ROOM][%s][C] RepRoom DEL RoomId=%s"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens));
}

void AMosesLobbyGameState::Server_InitLobbyIfNeeded()
{
	if (!HasAuthority()) return;
	if (bLobbyInitialized_DoD) return;

	bLobbyInitialized_DoD = true;
	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM] Lobby Initialized"));
}

void AMosesLobbyGameState::OnRep_RepRoomList()
{
	// 클라이언트에서 RepRoomList가 변경되면, 각 LocalPlayer의 Lobby UI 갱신을 트리거한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
		for (ULocalPlayer* LP : LocalPlayers)
		{
			if (!LP) continue;

			if (UMosesLobbyLocalPlayerSubsystem* Subsys =
				LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
			{
				Subsys->NotifyRoomStateChanged();
			}
		}
	}
}

