// MosesLobbyGameState.cpp
#include "MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

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

	// 개발자 주석:
	// - FastArray 콜백에서 GameState(NetMode 등) 컨텍스트를 찍기 위해 Owner를 세팅한다(비복제).
	RoomList.SetOwner(this);

	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] PostInit -> RoomList Owner set."),
		MosesNetModeToStr_UObject(this));
}

void AMosesLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// 개발자 주석:
	// - Travel/재생성/PIE 케이스 대비해서 BeginPlay에서도 한 번 더 세팅한다.
	RoomList.SetOwner(this);

	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] BeginPlay -> RoomList Owner set."),
		MosesNetModeToStr_UObject(this));
}

void AMosesLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 개발자 주석:
	// - FastArray 컨테이너 자체를 Replicate 해야 Delta 복제가 동작한다.
	DOREPLIFETIME(AMosesLobbyGameState, RoomList);
}

// =========================================================
// UI Notify (single entry)
// =========================================================

void AMosesLobbyGameState::NotifyRoomStateChanged_LocalPlayers() const
{
	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] NotifyRoomStateChanged_LocalPlayers()"), MosesNetModeToStr_UObject(this));
	// 개발자 주석:
	// - GameState는 UI를 직접 만지지 않는다.
	// - LocalPlayerSubsystem에게만 "RoomStateChanged"를 Notify한다.
	// - Widget은 Subsystem 이벤트를 받아 갱신한다.
	// - DS는 LocalPlayer가 없으므로 자연스럽게 아무 일도 안 한다.
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

void AMosesLobbyGameState::OnRep_RoomList()
{
	// 개발자 주석:
	// - RepNotify는 "백업용"으로 둔다.
	// - FastArray는 PostReplicatedAdd/Change/Remove가 메인 트리거다.
	NotifyRoomStateChanged_LocalPlayers();
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
	// 개발자 주석:
	// - 정책(최종):
	//   ✅ "호스트 제외" 모든 멤버가 Ready=1 이면 AllReady
	//   ✅ 호스트는 Ready UI가 없으므로, Ready 값은 검사 대상에서 제외한다.
	//
	// - 선택 정책(권장):
	//   ✅ 비호스트가 최소 1명은 있어야 Start 가능 (혼자 방에서 Start 방지)
	if (!HostPid.IsValid())
	{
		return false;
	}

	if (MemberPids.Num() <= 1)
	{
		// Host만 있는 방(또는 멤버가 없음) -> Start 불가
		return false;
	}

	if (MemberReady.Num() != MemberPids.Num())
	{
		return false;
	}

	const int32 HostIndex = MemberPids.IndexOfByKey(HostPid);
	if (HostIndex == INDEX_NONE)
	{
		// 방 정보가 꼬인 상태(HostPid가 멤버에 없음)
		return false;
	}

	for (int32 i = 0; i < MemberPids.Num(); ++i)
	{
		if (i == HostIndex)
		{
			continue; // ✅ 호스트 Ready는 검사하지 않는다
		}

		if (!MemberReady.IsValidIndex(i) || MemberReady[i] == 0)
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
	// 개발자 주석:
	// - FastArray 아이템 변경 시 반드시 호출(델타 복제 반영)
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

	// 개발자 주석:
	// - 아이템 제거는 MarkArrayDirty가 필요
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
FGuid AMosesLobbyGameState::Server_CreateRoom(
	AMosesPlayerState* HostPS,
	const FString& RoomTitle,
	int32 MaxPlayers
)
{
	// - 이 함수는 "방 생성"의 최종 권한을 가진 서버 전용 로직이다.
	// - GameState는 서버 + 모든 클라이언트에 복제되므로
	//   방 목록 관리에 가장 적합한 위치다.
	check(HasAuthority());

	// - 방장을 맡을 PlayerState가 없거나
	// - 최대 인원이 비정상 값이면
	//   방 생성 자체를 거부한다.
	if (!HostPS || MaxPlayers <= 0)
	{
		// - Invalid Guid는 "방 생성 실패"를 의미한다.
		return FGuid();
	}

	// - 방을 새로 만들려는 플레이어가
	//   이미 다른 방에 소속되어 있는 경우
	//   한 플레이어가 여러 방에 속하지 않도록
	//   기존 방에서 먼저 나가게 처리한다.
	if (HostPS->GetRoomId().IsValid())
	{
		Server_LeaveRoom(HostPS);
	}

	// - 새로 생성될 방 정보를 담을 구조체.
	// - 아직 네트워크로 전파되지 않은 로컬 서버 데이터 상태.
	FMosesLobbyRoomItem NewRoom;

	// - 방을 유일하게 식별하기 위한 서버 생성 GUID.
	// - 클라이언트 입력이 아닌, 서버가 직접 생성한다.
	NewRoom.RoomId = FGuid::NewGuid();

	// - 방 최대 인원은 서버가 확정한 값.
	// - UI에서 받은 값이라도 서버 기준이 최종 정답이다.
	NewRoom.MaxPlayers = MaxPlayers;

	// - 방 제목은 클라이언트 입력을 그대로 신뢰하지 않는다.
	// - 앞/뒤 공백 제거로 UI 이상 표시 방지.
	// - 최대 길이 제한으로 패킷/레이아웃 안정성 확보.
	// - 이 값이 그대로 모든 클라이언트 UI에 복제된다.
	NewRoom.RoomTitle = RoomTitle.TrimStartAndEnd().Left(24);

	// - PlayerState를 고유하게 식별하는 PID를 서버에서 획득.
	// - Room 구조체에는 Actor 포인터 대신 ID만 저장해
	//   네트워크 안정성과 직렬화 안전성을 확보한다.
	const FGuid HostPid = GetPidChecked(HostPS);

	// - 방장 PID 지정.
	NewRoom.HostPid = HostPid;

	// - 방 생성 시점에는
	//   방장 혼자만 멤버로 포함된 상태로 시작한다.
	NewRoom.MemberPids = { HostPid };

	// - Ready 상태 배열.
	// - 방장은 방 생성 직후 Ready 상태가 아니다.
	NewRoom.MemberReady = { 0 };

	// - 서버의 방 목록 배열에 새 방 추가.
	// - 아직 클라이언트로 전송되지는 않은 상태.
	RoomList.Items.Add(NewRoom);

	// - FastArraySerializer 사용 중.
	// - 배열이 변경되었음을 네트워크 시스템에 명시적으로 알림.
	// - 이 호출이 있어야 클라이언트에 방 목록 변경이 전파된다.
	RoomList.MarkArrayDirty();

	// - 방장을 맡은 PlayerState에
	//   "내가 어떤 방에 속했는지" 서버 기준으로 확정.
	// - bIsHost=true 로 방장 권한 부여.
	HostPS->ServerSetRoom(NewRoom.RoomId, /*bIsHost*/ true);

	// - 방 생성 직후에는 Ready 상태를 강제로 false로 초기화.
	// - 이전 방의 Ready 상태가 남아있지 않도록 보장.
	HostPS->ServerSetReady(false);

	// - 방 목록 및 PlayerState 변경 사항을
	//   기본 NetUpdate 주기를 기다리지 않고
	//   즉시 클라이언트로 복제하도록 강제 요청.
	ForceNetUpdate();

	// - 로컬 플레이어용 Subsystem/UI에
	//   방 상태 변경 이벤트를 전달.
	// - 네트워크 로직과 UI 로직을 분리하기 위한 호출.
	NotifyRoomStateChanged_LocalPlayers();

	// - 디버깅 및 재현 로그 기록용.
	LogRoom_Create(NewRoom);

	// - 최종적으로 생성된 방의 RoomId 반환.
	// - 호출 측에서 성공 여부 판단 및 후속 처리에 사용.
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

	// 개발자 주석:
	// - 정책: 다른 방에 있으면 먼저 이탈(단일 소속)
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

bool AMosesLobbyGameState::Server_JoinRoomWithResult(AMosesPlayerState* JoinPS, const FGuid& RoomId, EMosesRoomJoinResult& OutResult)
{
	check(HasAuthority());

	OutResult = EMosesRoomJoinResult::Ok;

	if (!JoinPS)
	{
		OutResult = EMosesRoomJoinResult::NoRoom;
		LogRoom_JoinRejected(OutResult, RoomId, JoinPS);
		return false;
	}

	if (!JoinPS->IsLoggedIn())
	{
		OutResult = EMosesRoomJoinResult::NotLoggedIn;
		LogRoom_JoinRejected(OutResult, RoomId, JoinPS);
		return false;
	}

	if (!RoomId.IsValid())
	{
		OutResult = EMosesRoomJoinResult::InvalidRoomId;
		LogRoom_JoinRejected(OutResult, RoomId, JoinPS);
		return false;
	}

	if (JoinPS->GetRoomId().IsValid())
	{
		Server_LeaveRoom(JoinPS);
	}

	FMosesLobbyRoomItem* Room = FindRoomMutable(RoomId);
	if (!Room)
	{
		OutResult = EMosesRoomJoinResult::NoRoom;
		LogRoom_JoinRejected(OutResult, RoomId, JoinPS);
		return false;
	}

	const FGuid Pid = GetPidChecked(JoinPS);

	if (Room->MemberPids.Contains(Pid))
	{
		OutResult = EMosesRoomJoinResult::AlreadyMember;
		LogRoom_JoinRejected(OutResult, RoomId, JoinPS);
		return false;
	}

	if (Room->IsFull())
	{
		OutResult = EMosesRoomJoinResult::RoomFull;
		LogRoom_JoinRejected(OutResult, RoomId, JoinPS);
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

	OutResult = EMosesRoomJoinResult::Ok;
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

	// 개발자 주석:
	// - Room이 없으면 PS만 정리(불일치 복구)
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

	// 개발자 주석:
	// - 호스트 승계 정책: 호스트가 나가면 첫 멤버로 승계
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

	// 개발자 주석:
	// - 정책(최종):
	//   ✅ "호스트 제외" 모든 멤버 Ready면 Start 가능
	//   ✅ 방 정원(MaxPlayers)과 무관 (원하면 여기서 Full 정책을 다시 넣으면 됨)
	//   ✅ Host만 있는 방은 Start 불가 (RoomItem::IsAllReady에서 이미 차단)

	if (!Room->IsAllReady())
	{
		OutReason = TEXT("NotAllReady");
		return false;
	}

	OutReason = TEXT("OK");
	return true;
}

// =========================================================
// FastArray callbacks (client-side logs + UI notify)
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

	// 개발자 주석:
	// - 클라가 델타를 수신한 "바로 그 순간" UI 갱신 신호를 때린다.
	// - 방 목록은 "모두에게" 보여야 하므로 OwnerOnly 같은 조건이 없다.
	if (GS && NM == NM_Client)
	{
		GS->NotifyRoomStateChanged_LocalPlayers();
	}
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

	if (GS && NM == NM_Client)
	{
		GS->NotifyRoomStateChanged_LocalPlayers();
	}
}

void FMosesLobbyRoomItem::PreReplicatedRemove(const FMosesLobbyRoomList& InArraySerializer)
{
	const AMosesLobbyGameState* GS = InArraySerializer.Owner.Get();
	const ENetMode NM = GS ? GS->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesRoom, Log, TEXT("[ROOM][%s][C] RepRoom DEL RoomId=%s"),
		MosesNetModeToStr(NM),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens));

	if (GS && NM == NM_Client)
	{
		GS->NotifyRoomStateChanged_LocalPlayers();
	}
}
