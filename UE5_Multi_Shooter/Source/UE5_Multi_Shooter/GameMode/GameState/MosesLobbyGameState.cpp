#include "MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/MosesGameInstance.h"

#include "Net/UnrealNetwork.h"

// =========================================================
// NetMode 문자열 헬퍼 (이 cpp 내부 전용)
//
// 의도:
// - 서버/클라 로그 비교를 쉽게 하기 위해 NetMode를 짧은 문자열로 찍는다.
// - 아직 공용 유틸로 뺄 단계가 아니므로 "이 파일 안"에 둔다.
// =========================================================
FORCEINLINE const TCHAR* MosesNetModeToStr(ENetMode NM)
{
	switch (NM)
	{
	case NM_DedicatedServer: return TEXT("DS");
	case NM_ListenServer:    return TEXT("LS");
	case NM_Client:          return TEXT("CL");
	default:                 return TEXT("ST");
	}
}

FORCEINLINE const TCHAR* MosesNetModeToStr_UObject(const UObject* Obj)
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

// =========================================================
// AMosesLobbyGameState
// =========================================================

AMosesLobbyGameState::AMosesLobbyGameState()
{
	bReplicates = true;
}

void AMosesLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// FastArray 콜백에서 로그 컨텍스트를 찍기 위한 Owner 설정(비복제)
	RoomList.SetOwner(this);

	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] PostInit -> RoomList Owner set."),
		MosesNetModeToStr_UObject(this));
}

void AMosesLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// Travel/재생성/PIE 케이스 대비해서 BeginPlay에서도 한 번 더
	RoomList.SetOwner(this);

	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] BeginPlay -> RoomList Owner set."),
		MosesNetModeToStr_UObject(this));
}

void AMosesLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// FastArray 컨테이너 자체를 Replicate 해야 Delta 복제가 동작한다.
	DOREPLIFETIME(AMosesLobbyGameState, RoomList);
}

// =========================================================
// FMosesLobbyRoomItem utils
// =========================================================

bool FMosesLobbyRoomItem::IsFull() const
{
	return MaxPlayers > 0 && MemberPids.Num() >= MaxPlayers;
}

bool FMosesLobbyRoomItem::Contains(const FGuid& Pid) const
{
	return MemberPids.Contains(Pid);
}

bool FMosesLobbyRoomItem::IsAllReady() const
{
	// 정책:
	// - Start 가능 조건은 "정원 충족 + 모두 Ready"
	// - 정원 미충족이면 Ready가 모두 1이어도 Start 불가
	if (MaxPlayers <= 0)
	{
		return false;
	}

	if (MemberPids.Num() != MaxPlayers)
	{
		return false;
	}
		
	// Ready 배열 정합성(1:1)
	if (MemberReady.Num() != MemberPids.Num())
	{
		return false;
	}

	for (uint8 R : MemberReady)
	{
		if (R == 0)
		{
			return false;
		}
	}

	return true;
}

void FMosesLobbyRoomItem::EnsureReadySize()
{
	if (MemberReady.Num() != MemberPids.Num())
	{
		MemberReady.SetNum(MemberPids.Num());
	}
}

bool FMosesLobbyRoomItem::SetReadyByPid(const FGuid& Pid, bool bInReady)
{
	const int32 Index = MemberPids.IndexOfByKey(Pid);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	EnsureReadySize();

	if (!MemberReady.IsValidIndex(Index))
	{
		return false;
	}

	MemberReady[Index] = bInReady ? 1 : 0;
	return true;
}

// =========================================================
// AMosesLobbyGameState internal helpers
// =========================================================

FGuid AMosesLobbyGameState::GetPidChecked(const AMosesPlayerState* PS)
{
	check(PS);
	check(PS->GetPersistentId().IsValid());
	return PS->GetPersistentId();
}

FMosesLobbyRoomItem* AMosesLobbyGameState::FindRoomMutable(const FGuid& RoomId)
{
	for (FMosesLobbyRoomItem& It : RoomList.Items)
	{
		if (It.RoomId == RoomId)
		{
			return &It;
		}
	}

	return nullptr;
}

const FMosesLobbyRoomItem* AMosesLobbyGameState::FindRoom(const FGuid& RoomId) const
{
	for (const FMosesLobbyRoomItem& It : RoomList.Items)
	{
		if (It.RoomId == RoomId)
		{
			return &It;
		}
	}

	return nullptr;
}

void AMosesLobbyGameState::MarkRoomDirty(FMosesLobbyRoomItem& Item)
{
	// FastArray 아이템 변경 시 반드시 호출(델타 복제 반영)
	RoomList.MarkItemDirty(Item);
}

void AMosesLobbyGameState::RemoveEmptyRoomIfNeeded(const FGuid& RoomId)
{
	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (!Room)
	{
		return;
	}

	if (Room->MemberPids.Num() > 0)
	{
		return;
	}

	// 아이템 제거는 MarkArrayDirty가 필요
	for (int32 i = 0; i < RoomList.Items.Num(); ++i)
	{
		if (RoomList.Items[i].RoomId == RoomId)
		{
			RoomList.Items.RemoveAt(i);
			RoomList.MarkArrayDirty();
			ForceNetUpdate();
			return;
		}
	}
}

// =========================================================
// Logs (server)
// =========================================================

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
	const FGuid JoinPid = JoinPS ? JoinPS->GetPersistentId() : FGuid();
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
	case EMosesRoomJoinResult::NotLoggedIn:   ReasonStr = TEXT("NotLoggedIn"); break;
	default: break;
	}

	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][S] Join REJECT %s RoomId=%s Cur=%d Max=%d JoinPid=%s"),
		MosesNetModeToStr_UObject(this),
		ReasonStr,
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		Cur, Max,
		*JoinPid.ToString(EGuidFormats::DigitsWithHyphens));
}

// =========================================================
// Server API : Create / Join / Leave
// =========================================================

FGuid AMosesLobbyGameState::Server_CreateRoom(AMosesPlayerState* HostPS, int32 MaxPlayers)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("ROOM ADDED"));

	check(HasAuthority());

	if (!HostPS || MaxPlayers <= 0)
	{
		return FGuid();
	}

	// 정책: 이미 방에 있으면 먼저 이탈(단일 소속)
	if (HostPS->GetRoomId().IsValid())
	{
		Server_LeaveRoom(HostPS);
	}

	FMosesLobbyRoomItem NewRoom;
	NewRoom.RoomId = FGuid::NewGuid();
	NewRoom.MaxPlayers = MaxPlayers;

	const FGuid HostPid = GetPidChecked(HostPS);

	// Host 등록(첫 멤버)
	NewRoom.HostPid = HostPid;
	NewRoom.MemberPids = { HostPid };
	NewRoom.MemberReady = { 0 }; // 기본 NotReady

	RoomList.Items.Add(NewRoom);
	RoomList.MarkArrayDirty(); // 아이템 추가/삭제는 ArrayDirty

	// PS 상태 반영(서버 authoritative)
	HostPS->ServerSetRoom(NewRoom.RoomId, /*bIsHost*/ true);
	HostPS->ServerSetReady(false);

	ForceNetUpdate();
	LogRoom_Create(NewRoom);
	return NewRoom.RoomId;
}

bool AMosesLobbyGameState::Server_JoinRoom(AMosesPlayerState* JoinPS, const FGuid& RoomId)
{
	check(HasAuthority());

	EMosesRoomJoinResult Result = EMosesRoomJoinResult::Ok;

	if (!JoinPS)
	{
		Result = EMosesRoomJoinResult::NoRoom;
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	if (!JoinPS->IsLoggedIn())
	{
		Result = EMosesRoomJoinResult::NotLoggedIn;
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	if (!RoomId.IsValid())
	{
		Result = EMosesRoomJoinResult::InvalidRoomId;
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	// 정책: 다른 방에 있으면 먼저 이탈(단일 소속)
	if (JoinPS->GetRoomId().IsValid())
	{
		Server_LeaveRoom(JoinPS);
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (!Room)
	{
		Result = EMosesRoomJoinResult::NoRoom;
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	const FGuid Pid = GetPidChecked(JoinPS);

	if (Room->MemberPids.Contains(Pid))
	{
		Result = EMosesRoomJoinResult::AlreadyMember;
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	if (Room->IsFull())
	{
		Result = EMosesRoomJoinResult::RoomFull;
		LogRoom_JoinRejected(Result, RoomId, JoinPS);
		return false;
	}

	Room->MemberPids.Add(Pid);
	Room->MemberReady.Add(0);
	Room->EnsureReadySize();

	MarkRoomDirty(*Room);

	JoinPS->ServerSetRoom(RoomId, /*bIsHost*/ (Pid == Room->HostPid));
	JoinPS->ServerSetReady(false);

	ForceNetUpdate();

	LogRoom_JoinAccepted(*Room);
	return true;
}

void AMosesLobbyGameState::Server_LeaveRoom(AMosesPlayerState* PS)
{
	check(HasAuthority());

	if (!PS)
	{
		return;
	}

	if (!PS->GetRoomId().IsValid())
	{
		return;
	}
		
	const FGuid LeavingRoomId = PS->GetRoomId();
	FMosesLobbyRoomItem* Room = FindRoomMutable(LeavingRoomId);

	// Room이 없으면 PS만 정리(불일치 복구)
	if (!Room)
	{
		PS->ServerSetRoom(FGuid(), false);
		PS->ServerSetReady(false);
		return;
	}

	const FGuid Pid = GetPidChecked(PS);

	const int32 Index = Room->MemberPids.IndexOfByKey(Pid);
	if (Index != INDEX_NONE)
	{
		Room->MemberPids.RemoveAt(Index);
		Room->EnsureReadySize();

		if (Room->MemberReady.IsValidIndex(Index))
		{
			Room->MemberReady.RemoveAt(Index);
		}

		Room->EnsureReadySize();
	}

	// 호스트 승계 정책: 호스트가 나가면 첫 멤버로 승계
	if (Room->HostPid == Pid)
	{
		Room->HostPid = (Room->MemberPids.Num() > 0) ? Room->MemberPids[0] : FGuid();
	}

	MarkRoomDirty(*Room);

	// 빈 방 삭제
	RemoveEmptyRoomIfNeeded(LeavingRoomId);

	// PS 리셋
	PS->ServerSetRoom(FGuid(), false);
	PS->ServerSetReady(false);

	ForceNetUpdate();
}

void AMosesLobbyGameState::Server_SyncReadyFromPlayerState(AMosesPlayerState* PS)
{
	check(HasAuthority());

	if (!PS)
	{
		return;
	}

	if (!PS->GetRoomId().IsValid())
	{
		return;
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(PS->GetRoomId());
	if (!Room)
	{
		return;
	}

	const FGuid Pid = GetPidChecked(PS);

	if (Room->SetReadyByPid(Pid, PS->IsReady()))
	{
		MarkRoomDirty(*Room);
	}
}

bool AMosesLobbyGameState::Server_CanStartGame(AMosesPlayerState* HostPS, FString& OutFailReason) const
{
	check(HasAuthority());

	if (!HostPS)
	{
		OutFailReason = TEXT("NoHostPS");
		return false;
	}

	if (!HostPS->IsRoomHost())
	{
		OutFailReason = TEXT("NotHost");
		return false;
	}

	const FGuid RoomId = HostPS->GetRoomId();
	if (!RoomId.IsValid())
	{
		OutFailReason = TEXT("NoRoomId");
		return false;
	}

	return Server_CanStartMatch(RoomId, OutFailReason);
}

bool AMosesLobbyGameState::Server_CanStartMatch(const FGuid& RoomId, FString& OutReason) const
{
	check(HasAuthority());

	const FMosesLobbyRoomItem* Room = FindRoom(RoomId);
	if (!Room) 
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

	if (!Room->IsAllReady()) 
	{
		OutReason = TEXT("NotAllReady"); 
		return false; 
	}

	OutReason = TEXT("OK");
	return true;
}

// =========================================================
// FastArray callbacks (client-side logs)
// =========================================================

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

// =========================================================
// RepNotify -> UI update delegation
// =========================================================

void AMosesLobbyGameState::OnRep_RoomList()
{
	// GameState는 UI를 직접 만지지 않는다.
	// LocalPlayerSubsystem이 "클라 UI 갱신" 책임을 가진다.
	if (UMosesGameInstance* GI = UMosesGameInstance::Get(this))
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
		for (ULocalPlayer* LP : LocalPlayers)
		{
			if (!LP)
			{
				continue;
			}

			if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
			{
				Subsys->NotifyRoomStateChanged();
			}
		}
	}
}
