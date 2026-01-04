#include "MosesPlayerState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "Net/UnrealNetwork.h"

#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

AMosesPlayerState::AMosesPlayerState()
{
	// PlayerState는 기본적으로 복제 대상
	bReplicates = true;
}

void AMosesPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// ⚠️ 권장:
	// - PersistentId 발급은 GameMode(PostLogin) 한 곳에서만 책임지게 고정하는 것을 추천.
	// - 여기서는 "발급"을 하지 않고, 값이 이미 있다면 그대로 둔다.
	// - (정말 필요하면 서버에서만 임시 생성은 가능하지만, 책임이 분산되면 나중에 PK 꼬임)
}

void AMosesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesPlayerState, DebugName);

	DOREPLIFETIME(AMosesPlayerState, PersistentId);
	DOREPLIFETIME(AMosesPlayerState, bLoggedIn);
	DOREPLIFETIME(AMosesPlayerState, bReady);
	DOREPLIFETIME(AMosesPlayerState, SelectedCharacterId);

	DOREPLIFETIME(AMosesPlayerState, RoomId);
	DOREPLIFETIME(AMosesPlayerState, bIsRoomHost);
}

void AMosesPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	AMosesPlayerState* NewPS = Cast<AMosesPlayerState>(NewPlayerState);
	if (!NewPS) return;

	// SeamlessTravel 유지 대상 복사
	NewPS->PersistentId = PersistentId;
	NewPS->DebugName = DebugName;

	NewPS->bLoggedIn = bLoggedIn;
	NewPS->bReady = bReady;
	NewPS->SelectedCharacterId = SelectedCharacterId;

	NewPS->RoomId = RoomId;
	NewPS->bIsRoomHost = bIsRoomHost;
}

void AMosesPlayerState::OverrideWith(APlayerState* OldPlayerState)
{
	Super::OverrideWith(OldPlayerState);

	const AMosesPlayerState* OldPS = Cast<AMosesPlayerState>(OldPlayerState);
	if (!OldPS) return;

	// 클라에서도 값이 덮어씌워져 UI가 유지되는 것을 관찰 가능
	PersistentId = OldPS->PersistentId;
	DebugName = OldPS->DebugName;

	bLoggedIn = OldPS->bLoggedIn;
	bReady = OldPS->bReady;
	SelectedCharacterId = OldPS->SelectedCharacterId;

	RoomId = OldPS->RoomId;
	bIsRoomHost = OldPS->bIsRoomHost;
}

void AMosesPlayerState::ServerSetLoggedIn(bool bInLoggedIn)
{
	check(HasAuthority());

	if (bLoggedIn == bInLoggedIn)
	{
		return;
	}

	// 서버 단일진실
	bLoggedIn = bInLoggedIn;
}

void AMosesPlayerState::ServerSetReady(bool bInReady)
{
	check(HasAuthority());

	if (bReady == bInReady)
	{
		return;
	}

	bReady = bInReady;
}

void AMosesPlayerState::ServerSetSelectedCharacterId(int32 InId)
{
	check(HasAuthority());

	const int32 SafeId = FMath::Clamp(InId, 1, 2);

	if (SelectedCharacterId == SafeId)
	{
		return;
	}

	SelectedCharacterId = SafeId;

	UE_LOG(LogMosesPlayer, Log, TEXT("[CharSel][SV][PS] Set SelectedCharacterId=%d Pid=%s"),
		SelectedCharacterId,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens));

	// 개발자 주석:
	// - 즉시 반영 체감을 원하면 강제 업데이트(선택)
	ForceNetUpdate();
}

void AMosesPlayerState::ServerSetRoom(const FGuid& InRoomId, bool bInIsHost)
{
	check(HasAuthority());

	if (RoomId == InRoomId && bIsRoomHost == bInIsHost)
	{
		return;
	}

	RoomId = InRoomId;
	bIsRoomHost = bInIsHost;
}

void AMosesPlayerState::DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const
{
	const TCHAR* CallerName = Caller ? *Caller->GetName() : TEXT("None");

	UE_LOG(LogMosesPlayer, Log,
		TEXT("[PS][%s] %s Caller=%s Pid=%s RoomId=%s Host=%d Ready=%d LoggedIn=%d SelChar=%d"),
		Phase,
		*GetName(),
		CallerName,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		bIsRoomHost ? 1 : 0,
		bReady ? 1 : 0,
		bLoggedIn ? 1 : 0,
		SelectedCharacterId);
}

static void NotifyOwningLocalPlayer_PSChanged(AMosesPlayerState* PS)
{
	if (!PS)
	{
		return;
	}

	// ✅ “모든 LocalPlayer 순회” 대신 “이 PlayerState의 소유자”에게만 노티
	APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
	if (!PC)
	{
		// Owner가 없거나 타입이 다르면 기존 방식으로 폴백할 수도 있지만,
		// Phase0에서는 과한 순회를 줄이는 쪽이 깔끔함.
		return;
	}

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
	{
		Subsys->NotifyPlayerStateChanged();
	}
}

void AMosesPlayerState::OnRep_PersistentId()
{
	// 개발자 주석:
	// - 클라에서 PersistentId가 "도착"한 타이밍.
	// - 여기서 UI 갱신 트리거(Subsystem Notify) 등을 걸면
	//   'Pid not ready' 타이밍 이슈가 구조적으로 사라진다.
	UE_LOG(LogMosesPlayer, Log, TEXT("[PS][CL] OnRep_PersistentId -> %s PS=%s"),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetNameSafe(this));

	NotifyOwningLocalPlayer_PSChanged(this);
}

void AMosesPlayerState::OnRep_LoggedIn()
{
	NotifyOwningLocalPlayer_PSChanged(this);
}

void AMosesPlayerState::OnRep_Ready()
{
	NotifyOwningLocalPlayer_PSChanged(this);
}

void AMosesPlayerState::OnRep_SelectedCharacterId()
{
	UE_LOG(LogMosesPlayer, Log, TEXT("[CharSel][CL][PS] OnRep SelectedCharacterId=%d Pid=%s"),
		SelectedCharacterId,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens));

	NotifyOwningLocalPlayer_PSChanged(this);
}

void AMosesPlayerState::OnRep_Room()
{
	NotifyOwningLocalPlayer_PSChanged(this);
}

void AMosesPlayerState::EnsurePersistentId_Server()
{
	// 개발자 주석:
	// - PersistentId는 룸 시스템의 "키"다. (RoomList.MemberPids, HostPid 등)
	// - 이 키가 Invalid면 GameState에서 check가 터지거나, 멤버 관리가 불가능해진다.
	// - 따라서 서버 PostLogin 시점에 "무조건" 세팅해준다.
	if (!HasAuthority())
	{
		return;
	}

	if (PersistentId.IsValid())
	{
		return;
	}

	PersistentId = FGuid::NewGuid();

	// 개발자 주석:
	// - 가능하면 빨리 클라로 보내서 UI/RoomList 로직이 안정적으로 돌게 한다.
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Log, TEXT("[PS][SV] EnsurePersistentId_Server -> %s PS=%s"),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetNameSafe(this));
}

void AMosesPlayerState::SetLoggedIn_Server(bool bInLoggedIn)
{
	// 개발자 주석:
	// - 서버에서만 상태를 확정한다.
	check(HasAuthority());

	bLoggedIn = bInLoggedIn;

	// 서버도 동일하게 후처리를 하고 싶으면 여기서 OnRep 호출(선택)
	OnRep_LoggedIn();
}