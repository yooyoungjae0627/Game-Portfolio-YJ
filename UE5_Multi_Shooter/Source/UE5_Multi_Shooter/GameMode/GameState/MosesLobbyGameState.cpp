#include "MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/GameMode/GameMode/MosesLobbyGameMode.h"

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
	// - GameState는 기본적으로 복제되지만,
	//   명시적으로 bReplicates=true 해두면 의도를 분명히 한다.
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

	// - 이 두 값이 "대화 모드 전광판"의 핵심.
	// - LateJoin도 이 값만 받으면 화면 복구가 가능해진다.
	DOREPLIFETIME(AMosesLobbyGameState, GamePhase);
	DOREPLIFETIME(AMosesLobbyGameState, DialogueNetState);
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

// =========================================================
// Server-only mutators
// =========================================================

void AMosesLobbyGameState::ServerEnterLobbyDialogue()
{
	// - 이 함수는 "서버만" 실행하는 상태 변경 함수다.
	// - Lobby -> LobbyDialogue로 전환하면서, 대화 전광판(DialogueNetState)의 초기값을 세팅한다.
	// - 클라는 이 함수를 직접 호출하면 안 되고, 반드시 PlayerController의 Server RPC를 통해
	//   서버가 "판정"한 뒤에 여기로 들어오는 흐름을 지켜야 한다.

	// - 서버 권한(Authority) 체크는 1차 안전장치.
	// - 클라가 실수로 호출해도 그냥 return.
	if (!IsServerAuth())
	{
		return;
	}

	// 이미 대화 모드면 중복 진입 금지
	if (GamePhase == EGamePhase::LobbyDialogue)
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[Phase] Lobby -> LobbyDialogue"));

	// - 단일진실 변경: GamePhase는 서버가 확정한다.
	// - 이 값은 Replicated라서 클라이언트들에게 자동으로 전달되고,
	//   클라에서는 OnRep_GamePhase()가 호출된다.
	GamePhase = EGamePhase::LobbyDialogue;

	// - 대화 전광판 초기 세팅.
	// - 실제 프로젝트에서는 DataTable/QuestState/DialogueScript 등에서
	//   어떤 대사를 틀지 결정한 후 여기 값들을 세팅하면 된다.
	DialogueNetState.SubState = EDialogueSubState::Listening; // (예) "듣는 중" 상태로 시작
	DialogueNetState.LineIndex = 0;                           // 첫 번째 라인부터
	DialogueNetState.RemainingTime = 3.0f;                    // (예) 3초짜리 라인/타이머
	DialogueNetState.bNPCSpeaking = true;                     // (예) NPC가 말하는 연출

	// - 서버는 RepNotify(OnRep)가 "자기 자신에게는" 안 온다.
	// - 그래서 서버도 동일한 흐름(이벤트 기반 UI/연출)을 유지하려면
	//   상태 변경 직후 서버에서 직접 Broadcast를 한 번 쏴줘야 한다.
	// - 이게 없으면 "서버에서 플레이 중인 Listen Server" 같은 환경에서
	//   서버 화면 UI가 갱신이 안 되는 현상이 생길 수 있다.
	BroadcastAll_ServerToo();

	// - Replication은 기본적으로 NetUpdate 주기에 맞춰 전송된다.
	// - ForceNetUpdate()는 "이 액터는 곧바로 업데이트할 가치가 있다"는 힌트를 주는 함수.
	// - 즉, 체감상 버튼 누른 직후 상태 전환이 더 빨리 도착하게(즉시성 향상) 도와준다.
	// - (중요) 무조건 즉시 전송을 보장하는 마법은 아니고, 네트워크 상황/엔진 스케줄에 따라 달라진다.
	ForceNetUpdate();
}

void AMosesLobbyGameState::ServerExitLobbyDialogue()
{
	// - LobbyDialogue -> Lobby로 복귀시키는 서버 전용 함수.
	// - 대화 상태를 "완전히 리셋"해서 late join 포함, 모든 클라가
	//   유령 대화 상태를 보지 않도록 만든다.
	if (!IsServerAuth())
	{
		return;
	}

	// - 이미 Lobby면 Exit할 게 없으므로 중복 호출 방지.
	// - (예) Exit 버튼 연타 / 중복 RPC에 대한 방어.
	if (GamePhase == EGamePhase::Lobby)
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[Phase] LobbyDialogue -> Lobby"));

	// - 서버가 phase를 Lobby로 확정한다.
	// - 클라는 이 값이 복제되면서 OnRep_GamePhase()를 통해 UI/카메라 복귀를 수행한다.
	GamePhase = EGamePhase::Lobby;

	// - 전광판(대화 상태) 완전 초기화.
	// - "Phase는 Lobby인데 DialogueNetState가 남아있는" 이상 상태를 방지한다.
	// - 특히 LateJoin(늦게 접속한 플레이어)이 들어왔을 때
	//   복제된 구조체가 이상한 값을 갖고 있으면 UI가 꼬이기 쉽다.
	DialogueNetState = FDialogueNetState();

	// - 서버도 즉시 UI/연출 이벤트를 받게 하기 위한 Broadcast.
	BroadcastAll_ServerToo();
	// - 리셋된 상태를 빠르게 전송하도록 힌트를 준다.
	ForceNetUpdate();
}

void AMosesLobbyGameState::ServerSetSubState(EDialogueSubState NewSubState, float NewRemainingTime, bool bInNPCSpeaking)
{
	// - "대화의 소상태"만 바꾸는 서버 전용 함수.
	// - 예: Listening <-> Speaking, 혹은 Choice, Waiting 등
	// - 이 함수는 "로직"의 결과를 전광판에 쓰는 역할만 한다.
	// - 타이머를 돌리는 로직/대사 진행 로직은 GameMode(서버) 또는
	//   서버 전용 매니저에서 하고, 여기에는 결과만 기록하는 게 깔끔하다.
	if (!IsServerAuth())
	{
		return;
	}

	// - 전광판 값 갱신: 서버가 확정한 값을 기록한다.
	DialogueNetState.SubState = NewSubState;
	// - RemainingTime은 음수가 되면 UI 타이머/프로그레스바가 깨질 수 있다.
	// - 그래서 최소 0으로 클램프(서버에서 최종 방어)한다.
	DialogueNetState.RemainingTime = FMath::Max(0.0f, NewRemainingTime);
	// - "지금 누가 말하는지" 같은 연출 플래그.
	// - 클라는 이 값으로 초상화 하이라이트/자막 위치 등을 바꿀 수 있다.
	DialogueNetState.bNPCSpeaking = bInNPCSpeaking;

	UE_LOG(LogMosesSpawn, Log, TEXT("[DialogueState] Sub=%d Line=%d T=%.2f NPC=%d"),
		(int32)DialogueNetState.SubState,
		DialogueNetState.LineIndex,
		DialogueNetState.RemainingTime,
		DialogueNetState.bNPCSpeaking ? 1 : 0
	);

	// - 서버와 클라의 "이벤트 기반" 흐름을 동일하게 유지.
	// - 서버는 OnRep가 없으니 Broadcast로 통일한다.
	BroadcastAll_ServerToo();

	// - 변경 내용을 빠르게 복제하도록 힌트.
	ForceNetUpdate();
}

void AMosesLobbyGameState::ServerAdvanceLine(int32 NextLineIndex, float Duration, bool bInNPCSpeaking)
{
	// - 대화 라인을 "다음 줄로 넘기는" 서버 전용 함수.
	// - 예: NPC가 한 줄 말하고 끝나면 NextLineIndex로 이동.
	// - 여기서는 "어떤 줄로 갔는지 / 얼마나 보여줄지" 결과만 전광판에 기록한다.
	// - NextLineIndex를 서버에서 보정해두면, 클라 UI는 안전하게 표시만 하면 된다.
	if (!IsServerAuth())
	{
		return;
	}

	// - 라인 인덱스는 음수면 배열 접근/DT row 접근에서 크래시 날 수 있다.
	// - 최소 0으로 보정(서버 최종 방어).
	DialogueNetState.LineIndex = FMath::Max(0, NextLineIndex);

	// - Duration도 음수면 UI 타이머 깨짐 -> 최소 0으로 보정.
	DialogueNetState.RemainingTime = FMath::Max(0.0f, Duration);

	// - 말하는 주체(연출용)
	DialogueNetState.bNPCSpeaking = bInNPCSpeaking;

	UE_LOG(LogMosesSpawn, Log, TEXT("[DialogueState] Sub=%d Line=%d T=%.2f NPC=%d"),
		(int32)DialogueNetState.SubState,
		DialogueNetState.LineIndex,
		DialogueNetState.RemainingTime,
		DialogueNetState.bNPCSpeaking ? 1 : 0
	);

	// - 서버에도 즉시 이벤트 전달 + 빠른 복제 요청
	BroadcastAll_ServerToo();
	ForceNetUpdate();
}

// =========================================================
// RepNotify (이벤트만)
// =========================================================

void AMosesLobbyGameState::OnRep_GamePhase()
{
	// - 이 함수는 "클라이언트에서만" 호출되는 RepNotify 콜백이다.
	// - 서버는 자신에게 복제를 받는 게 아니라 "보내는 쪽"이라 OnRep가 호출되지 않는다.
	//
	// - 핵심 규칙:
	//   ✅ 여기서는 "로직 금지"
	//   ❌ Travel, 상태 변경, 타이머 시작/정지 같은 '게임 규칙' 실행 금지
	//
	// - 이유:
	//   OnRep는 네트워크 타이밍에 따라 여러 번/순서 바뀌어 올 수 있다.
	//   여기서 게임 규칙을 돌리면, 클라마다 결과가 달라지거나 중복 실행 위험이 생긴다.
	// - 따라서: "UI/연출에게 알림(Broadcast)만" 한다.

	UE_LOG(LogMosesSpawn, Log, TEXT("[OnRep] Phase=%d"), (int32)GamePhase);

	// - UI/Subsystem/Camera 쪽이 이 이벤트를 구독하고 있다가
	//   Phase에 따라 카메라 전환, 위젯 Collapsed/Visible 등을 수행한다.
	OnPhaseChanged.Broadcast(GamePhase);
}

void AMosesLobbyGameState::OnRep_DialogueNetState()
{
	// - DialogueNetState 구조체가 복제되어 갱신될 때 호출되는 콜백.
	// - 역시 여기서 "진행 로직"을 만들면 클라가 서버 역할을 하게 되므로 금지.
	// - 오로지 "표시/연출"만 트리거한다.
	UE_LOG(LogMosesSpawn, Log, TEXT("[OnRep] Dialogue Sub=%d Line=%d T=%.2f NPC=%d"),
		(int32)DialogueNetState.SubState,
		DialogueNetState.LineIndex,
		DialogueNetState.RemainingTime,
		DialogueNetState.bNPCSpeaking ? 1 : 0
	);

	// - 위젯(자막/초상화/타이머) 또는 Subsystem이 이 이벤트를 받아서 갱신한다.
	OnDialogueStateChanged.Broadcast(DialogueNetState);
}

// =========================================================
// Helpers
// =========================================================

bool AMosesLobbyGameState::IsServerAuth() const
{
	// - HasAuthority() == 이 Actor의 상태를 최종 결정하는 쪽인가?
	// - Dedicated Server에서는 서버만 true.
	// - Listen Server에서는 Host 머신(서버 역할)이 true.
	return HasAuthority();
}

void AMosesLobbyGameState::BroadcastAll_ServerToo()
{
	// - "서버는 OnRep가 안 온다" 문제를 해결하는 유틸.
	// - 서버가 값을 바꾼 즉시, 서버도 이벤트를 한 번 브로드캐스트해서
	//   서버/클라가 동일한 '이벤트 기반 UI 갱신 흐름'을 갖게 만든다.
	//
	// - 결과:
	//   1) Dedicated Server에서는 서버 UI가 없으니 큰 의미 없어도,
	//   2) Listen Server(호스트가 화면도 보는 상황)에서는 서버 화면 UI도 정상 갱신된다.
	//   3) 코드 구조가 "항상 이벤트로 흘러간다"로 통일되어 유지보수가 쉬워진다.
	OnPhaseChanged.Broadcast(GamePhase);
	OnDialogueStateChanged.Broadcast(DialogueNetState);
}