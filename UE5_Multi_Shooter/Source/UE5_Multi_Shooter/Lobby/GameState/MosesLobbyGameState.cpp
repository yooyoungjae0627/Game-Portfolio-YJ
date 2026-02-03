#include "MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h" 
#include "UE5_Multi_Shooter/Lobby/UI/MosesLobbyChatTypes.h"

#include "Net/UnrealNetwork.h"

static int64 NowUnixMs()
{
	return FDateTime::UtcNow().ToUnixTimestamp() * 1000LL;
}

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

AMosesLobbyGameState::AMosesLobbyGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AMosesLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	RoomList.SetOwner(this);

	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] PostInit -> RoomList Owner set."),
		MosesNetModeToStr_UObject(this));
}

void AMosesLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}
}

void AMosesLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesLobbyGameState, RoomList);
	DOREPLIFETIME(AMosesLobbyGameState, ChatHistory);
}

void AMosesLobbyGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}
}

// =========================================================
// UI Notify (single entry)
// =========================================================

void AMosesLobbyGameState::NotifyRoomStateChanged_LocalPlayers() const
{
	UE_LOG(LogMosesRoom, Log, TEXT("[LobbyGS][%s] NotifyRoomStateChanged_LocalPlayers()"), MosesNetModeToStr_UObject(this));

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
	NotifyRoomStateChanged_LocalPlayers();
}

void AMosesLobbyGameState::OnRep_ChatHistory()
{
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
	if (!HostPid.IsValid())
	{
		return false;
	}

	if (MemberPids.Num() <= 1)
	{
		return false;
	}

	if (MemberReady.Num() != MemberPids.Num())
	{
		return false;
	}

	const int32 HostIndex = MemberPids.IndexOfByKey(HostPid);
	if (HostIndex == INDEX_NONE)
	{
		return false;
	}

	for (int32 i = 0; i < MemberPids.Num(); ++i)
	{
		if (i == HostIndex)
		{
			continue;
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

	if (GS && (NM == NM_Client || NM == NM_ListenServer))
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

	if (GS && (NM == NM_Client || NM == NM_ListenServer))
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

	if (GS && (NM == NM_Client || NM == NM_ListenServer))
	{
		GS->NotifyRoomStateChanged_LocalPlayers();
	}
}

// =========================================================
// AMosesLobbyGameState internal helpers
// =========================================================

FGuid AMosesLobbyGameState::GetPidChecked(const AMosesPlayerState* PS)
{
	check(PS);

	const FGuid Pid = PS->GetPersistentId();
	if (!Pid.IsValid())
	{
		UE_LOG(LogMosesRoom, Error, TEXT("[PID] Invalid! PS=%s Owner=%s"),
			*GetNameSafe(PS),
			*GetNameSafe(PS->GetOwner()));

		check(false);
	}

	return Pid;
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

FGuid AMosesLobbyGameState::Server_CreateRoom(AMosesPlayerState* HostPS, const FString& RoomTitle, int32 MaxPlayers)
{
	check(HasAuthority());

	if (!HostPS || MaxPlayers <= 0)
	{
		return FGuid();
	}

	if (HostPS->GetRoomId().IsValid())
	{
		Server_LeaveRoom(HostPS);
	}

	FMosesLobbyRoomItem NewRoom;
	NewRoom.RoomId = FGuid::NewGuid();
	NewRoom.MaxPlayers = MaxPlayers;
	NewRoom.RoomTitle = RoomTitle.TrimStartAndEnd().Left(24);

	const FGuid HostPid = GetPidChecked(HostPS);
	NewRoom.HostPid = HostPid;

	NewRoom.MemberPids = { HostPid };
	NewRoom.MemberReady = { 0 };

	RoomList.Items.Add(NewRoom);
	RoomList.MarkArrayDirty();

	HostPS->ServerSetRoom(NewRoom.RoomId, /*bIsHost*/ true);
	HostPS->ServerSetReady(false);

	ForceNetUpdate();
	NotifyRoomStateChanged_LocalPlayers();
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

	if (Room->HostPid == Pid)
	{
		Room->HostPid = (Room->MemberPids.Num() > 0) ? Room->MemberPids[0] : FGuid();
	}

	MarkRoomDirty(*Room);

	RemoveEmptyRoomIfNeeded(LeavingRoomId);

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

		NotifyRoomStateChanged_LocalPlayers();
		ForceNetUpdate();
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

	if (Room->MaxPlayers > 0 && Room->MemberPids.Num() != Room->MaxPlayers)
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
// Server Chat
// =========================================================

void AMosesLobbyGameState::Server_AddChatMessage(AMosesPlayerState* SenderPS, const FString& Text)
{
	// [MOD] 서버 권위 게이트 복구
	if (!HasAuthority() || !SenderPS)
	{
		return;
	}

	if (!SenderPS->IsLoggedIn())
	{
		return;
	}

	if (!SenderPS->GetRoomId().IsValid())
	{
		return;
	}

	const FString Clean = Text.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return;
	}

	constexpr int32 MaxLen = 120;
	const FString FinalText = (Clean.Len() > MaxLen) ? Clean.Left(MaxLen) : Clean;

	FLobbyChatMessage Msg;
	Msg.SenderPid = SenderPS->GetPersistentId();
	Msg.SenderName = SenderPS->GetPlayerNickName().IsEmpty() ? SenderPS->GetName() : SenderPS->GetPlayerNickName();
	Msg.Message = FinalText;
	Msg.ServerUnixTimeMs = NowUnixMs();

	ChatHistory.Add(Msg);

	constexpr int32 MaxKeep = 50;
	if (ChatHistory.Num() > MaxKeep)
	{
		ChatHistory.RemoveAt(0, ChatHistory.Num() - MaxKeep);
	}

	// 서버는 RepNotify 자동 호출 안 됨 → 서버도 UI 갱신 파이프 태우려면 직접 호출
	OnRep_ChatHistory();

	// 복제 즉시 밀어주기(UX)
	ForceNetUpdate();
}

// =========================================================
// Helpers
// =========================================================

bool AMosesLobbyGameState::IsServerAuth() const
{
	return HasAuthority();
}
