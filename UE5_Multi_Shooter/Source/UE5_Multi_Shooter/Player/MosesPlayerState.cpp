#include "MosesPlayerState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"
#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"

#include "Net/UnrealNetwork.h"

#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

AMosesPlayerState::AMosesPlayerState()
{
	// PlayerState는 기본적으로 복제 대상(서버 단일진실)
	bReplicates = true;

	// 로비/상태 갱신 체감을 빠르게(필요하면 나중에 낮춰도 됨)
	SetNetUpdateFrequency(100.f);

	// ✅ ASC 생성: "단일 진실(Owner)"은 PlayerState
	MosesAbilitySystemComponent = CreateDefaultSubobject<UMosesAbilitySystemComponent>(TEXT("MosesASC"));

	// ✅ ASC는 반드시 Replicate 되어야 태그/GE/Attribute가 클라로 내려옴
	MosesAbilitySystemComponent->SetIsReplicated(true);

	// ✅ Mixed: 소유자/관전자 등 상황에 맞춰 적당히 Rep (Day1에 무난)
	MosesAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ✅ AttributeSet도 PS에 소유( Pawn은 죽어서 교체되지만 PS는 유지 )
	AttributeSet = CreateDefaultSubobject<UMosesAttributeSet>(TEXT("MosesAttributeSet"));
}

void AMosesPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 개발자 주석(정책):
	// - PersistentId 발급은 "서버 GameMode(PostLogin)" 한 곳에서만 책임지는 것을 추천.
	// - 여기서 발급까지 해버리면 책임 분산으로 PK 꼬임/디버깅 난이도 상승 가능.
}

void AMosesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ✅ UI/표시용
	DOREPLIFETIME(AMosesPlayerState, PlayerNickName);

	// ✅ 서버 단일진실 필드
	DOREPLIFETIME(AMosesPlayerState, PersistentId);
	DOREPLIFETIME(AMosesPlayerState, bLoggedIn);
	DOREPLIFETIME(AMosesPlayerState, bReady);
	DOREPLIFETIME(AMosesPlayerState, SelectedCharacterId);

	DOREPLIFETIME(AMosesPlayerState, RoomId);
	DOREPLIFETIME(AMosesPlayerState, bIsRoomHost);
}

UAbilitySystemComponent* AMosesPlayerState::GetAbilitySystemComponent() const
{
	return MosesAbilitySystemComponent;
}

void AMosesPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	AMosesPlayerState* NewPS = Cast<AMosesPlayerState>(NewPlayerState);
	if (!NewPS)
	{
		return;
	}

	// ✅ SeamlessTravel 유지 대상 복사(서버)
	NewPS->PersistentId = PersistentId;
	NewPS->PlayerNickName = PlayerNickName;

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
	if (!OldPS)
	{
		return;
	}

	// ✅ SeamlessTravel 덮어쓰기(서버/클라)
	// - Travel 후에도 UI/상태가 유지되는 것을 관찰 가능
	PersistentId = OldPS->PersistentId;
	PlayerNickName = OldPS->PlayerNickName;

	bLoggedIn = OldPS->bLoggedIn;
	bReady = OldPS->bReady;
	SelectedCharacterId = OldPS->SelectedCharacterId;

	RoomId = OldPS->RoomId;
	bIsRoomHost = OldPS->bIsRoomHost;

	// (권장) Travel로 PS가 갈아끼워지는 경우, ASC 초기화는 새 Avatar 기준으로 다시 잡아야 하므로
	// 아래 리셋을 넣으면 더 안전하다.
	// bASCInitialized = false;
	// CachedAvatar.Reset();
}

void AMosesPlayerState::TryInitASC(AActor* InAvatarActor)
{
	// 방어: ASC가 없거나 Avatar가 유효하지 않으면 Init 불가
	if (!MosesAbilitySystemComponent || !IsValid(InAvatarActor))
	{
		return;
	}

	// ✅ Avatar가 바뀌었는지(Respawn/Reposssess 감지)
	const bool bAvatarChanged = (CachedAvatar.Get() != InAvatarActor);

	// ✅ 같은 Avatar에 대해 2번 Init되는 "중복"만 막는다
	// - 여기서 중복을 허용하면 Ability/Tag/Effect가 2중으로 꼬일 수 있음
	if (bASCInitialized && !bAvatarChanged)
	{
		UE_LOG(LogMosesGAS, Verbose,
			TEXT("[GAS] ASC Init SKIP (Already) PS=%s Avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InAvatarActor));
		return;
	}

	// ✅ Avatar가 바뀌면 재Init 허용 (Respawn 대응)
	CachedAvatar = InAvatarActor;
	bASCInitialized = true;

	// ✅ GAS 핵심: Owner=PS, Avatar=Pawn 연결
	MosesAbilitySystemComponent->InitAbilityActorInfo(this, InAvatarActor);

	// DoD 로그: 서버/클라에서 "몇 번/언제" Init 됐는지 추적 가능
	UE_LOG(LogMosesGAS, Log,
		TEXT("[GAS] ASC Init PS=%s Pawn=%s Role=%s AvatarChanged=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InAvatarActor),
		*UEnum::GetValueAsString(GetLocalRole()),
		bAvatarChanged ? 1 : 0);

	// Attribute도 같이 찍어 “서버/클라 값 일치” 검증
	LogAttributes();
}

void AMosesPlayerState::ServerSetCombatPhase(bool bEnable)
{
	// ✅ 정책 강제: 서버만 태그를 변경한다(클라 호출하면 즉시 터짐)
	check(HasAuthority());

	if (!MosesAbilitySystemComponent)
	{
		return;
	}

	const FMosesGameplayTags& MosesTags = FMosesGameplayTags::Get();

	// ✅ LooseGameplayTag: 서버에서 부여하면 복제로 내려감(클라는 보기만)
	if (bEnable)
	{
		MosesAbilitySystemComponent->AddLooseGameplayTag(MosesTags.State_Phase_Combat);
	}
	else
	{
		MosesAbilitySystemComponent->RemoveLooseGameplayTag(MosesTags.State_Phase_Combat);
	}
}

void AMosesPlayerState::ServerSetDead(bool bEnable)
{
	// ✅ 정책 강제: 서버만
	check(HasAuthority());

	if (!MosesAbilitySystemComponent)
	{
		return;
	}

	const FMosesGameplayTags& MosesTags = FMosesGameplayTags::Get();

	// ✅ Dead 정책:
	// - Dead면 Dead 태그 부여
	// - Dead면 Combat 태그 제거(입력/공격 차단 정책의 기반)
	if (bEnable)
	{
		MosesAbilitySystemComponent->AddLooseGameplayTag(MosesTags.State_Dead);
		MosesAbilitySystemComponent->RemoveLooseGameplayTag(MosesTags.State_Phase_Combat);
	}
	else
	{
		// ✅ 되살아났을 때 Dead 제거
		MosesAbilitySystemComponent->RemoveLooseGameplayTag(MosesTags.State_Dead);

		// (선택) 부활 시 Combat을 자동으로 다시 줄지 정책으로 결정
		// MosesAbilitySystemComponent->AddLooseGameplayTag(MosesTags.State_Phase_Combat);
	}
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
	ForceNetUpdate();
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
	// - 서버에서만 상태를 확정한다.
	check(HasAuthority());

	if (bLoggedIn == bInLoggedIn)
	{
		return;
	}

	bLoggedIn = bInLoggedIn;

	ForceNetUpdate();
}

void AMosesPlayerState::ServerSetPlayerNickName(const FString& InNickName)
{
	check(HasAuthority());

	const FString Clean = InNickName.TrimStartAndEnd().Left(16);
	if (Clean.IsEmpty())
	{
		return;
	}

	if (PlayerNickName == Clean)
	{
		return;
	}

	PlayerNickName = Clean;
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Log, TEXT("[Nick][SV][PS] Set Nick=%s Pid=%s"),
		*PlayerNickName,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens));
}

void AMosesPlayerState::OnRep_PlayerNickName()
{
	NotifyOwningLocalPlayer_PSChanged(this);
}

void AMosesPlayerState::LogASCInit(AActor* InAvatarActor) const
{
	const ENetRole LocalRole = GetLocalRole();

	UE_LOG(LogMosesGAS, Log, TEXT("[GAS] ASC Init PS=%s Pawn=%s Role=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InAvatarActor),
		*UEnum::GetValueAsString(LocalRole));
}

void AMosesPlayerState::LogAttributes() const
{
	if (!AttributeSet)
	{
		return;
	}

	UE_LOG(LogMosesGAS, Log, TEXT("[GAS] Attr HP=%.1f MaxHP=%.1f"),
		AttributeSet->GetHealth(),
		AttributeSet->GetMaxHealth());
}