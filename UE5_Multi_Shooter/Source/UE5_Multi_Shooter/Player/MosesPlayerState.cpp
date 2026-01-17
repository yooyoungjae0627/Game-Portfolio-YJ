// ============================================================================
// MosesPlayerState.cpp
// ============================================================================

#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

// Project
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h" // [ADD]

// GAS
#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"

// Engine
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

AMosesPlayerState::AMosesPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetNetUpdateFrequency(100.f);

	MosesAbilitySystemComponent = CreateDefaultSubobject<UMosesAbilitySystemComponent>(TEXT("MosesASC"));
	MosesAbilitySystemComponent->SetIsReplicated(true);
	MosesAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMosesAttributeSet>(TEXT("MosesAttributeSet"));
	CombatComponent = CreateDefaultSubobject<UMosesCombatComponent>(TEXT("MosesCombatComponent"));
}

void AMosesPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (HasAuthority() && CombatComponent)
	{
		CombatComponent->Server_EnsureInitialized_Day2();
	}

	BindCombatDelegatesOnce();
}

void AMosesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesPlayerState, PlayerNickName);
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

	NewPS->PersistentId = PersistentId;
	NewPS->PlayerNickName = PlayerNickName;
	NewPS->bLoggedIn = bLoggedIn;
	NewPS->bReady = bReady;
	NewPS->SelectedCharacterId = SelectedCharacterId;
	NewPS->RoomId = RoomId;
	NewPS->bIsRoomHost = bIsRoomHost;
	NewPS->PawnData = PawnData;
}

void AMosesPlayerState::OverrideWith(APlayerState* OldPlayerState)
{
	Super::OverrideWith(OldPlayerState);

	const AMosesPlayerState* OldPS = Cast<AMosesPlayerState>(OldPlayerState);
	if (!OldPS)
	{
		return;
	}

	PersistentId = OldPS->PersistentId;
	PlayerNickName = OldPS->PlayerNickName;
	bLoggedIn = OldPS->bLoggedIn;
	bReady = OldPS->bReady;
	SelectedCharacterId = OldPS->SelectedCharacterId;
	RoomId = OldPS->RoomId;
	bIsRoomHost = OldPS->bIsRoomHost;
	PawnData = OldPS->PawnData;
}

void AMosesPlayerState::TryInitASC(AActor* InAvatarActor)
{
	if (!MosesAbilitySystemComponent || !IsValid(InAvatarActor))
	{
		return;
	}

	const bool bAvatarChanged = (CachedAvatar.Get() != InAvatarActor);

	if (bASCInitialized && !bAvatarChanged)
	{
		return;
	}

	CachedAvatar = InAvatarActor;
	bASCInitialized = true;

	MosesAbilitySystemComponent->InitAbilityActorInfo(this, InAvatarActor);
}

void AMosesPlayerState::EnsurePersistentId_Server()
{
	check(HasAuthority());

	if (PersistentId.IsValid())
	{
		return;
	}

	PersistentId = FGuid::NewGuid();
	ForceNetUpdate();
}

void AMosesPlayerState::ServerSetLoggedIn(bool bInLoggedIn)
{
	check(HasAuthority());

	if (bLoggedIn == bInLoggedIn)
	{
		return;
	}

	bLoggedIn = bInLoggedIn;
	ForceNetUpdate();

	// [ADD] 리슨서버 즉시 UI 갱신
	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetLoggedIn"));
}

void AMosesPlayerState::ServerSetReady(bool bInReady)
{
	check(HasAuthority());

	if (bReady == bInReady)
	{
		return;
	}

	bReady = bInReady;
	ForceNetUpdate();

	// [ADD] 리슨서버 즉시 UI 갱신
	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetReady"));
}

void AMosesPlayerState::ServerSetSelectedCharacterId_Implementation(int32 InId)
{
	check(HasAuthority());

	SelectedCharacterId = FMath::Max(1, InId);
	ForceNetUpdate();

	BroadcastSelectedCharacterChanged(TEXT("ServerSetSelectedCharacterId"));

	// [ADD] 리슨서버 즉시 UI 갱신
	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetSelectedCharacterId"));
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

	ForceNetUpdate();

	// [ADD] 리슨서버 즉시 UI 갱신
	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetRoom"));
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

	// [ADD] 리슨서버 즉시 UI 갱신
	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetPlayerNickName"));
}

void AMosesPlayerState::OnRep_PersistentId()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PS][CL] OnRep_PersistentId -> %s PS=%s"),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetNameSafe(this));

	// [ADD]
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_PersistentId"));
}

void AMosesPlayerState::OnRep_LoggedIn()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PS][CL] OnRep_LoggedIn -> %d PS=%s"),
		bLoggedIn ? 1 : 0,
		*GetNameSafe(this));

	// [ADD]
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_LoggedIn"));
}

void AMosesPlayerState::OnRep_Ready()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PS][CL] OnRep_Ready -> %d PS=%s"),
		bReady ? 1 : 0,
		*GetNameSafe(this));

	// [ADD] ✅ Ready 체크박스 갱신 트리거
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_Ready"));
}

void AMosesPlayerState::OnRep_SelectedCharacterId()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PS][CL] OnRep_SelectedCharacterId=%d PS=%s"),
		SelectedCharacterId,
		*GetNameSafe(this));

	BroadcastSelectedCharacterChanged(TEXT("OnRep_SelectedCharacterId"));

	// [ADD]
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_SelectedCharacterId"));
}

void AMosesPlayerState::OnRep_Room()
{
	UE_LOG(LogMosesPlayer, Warning, TEXT("[PS][CL] OnRep_Room RoomId=%s Host=%d PS=%s"),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		bIsRoomHost ? 1 : 0,
		*GetNameSafe(this));

	// [ADD] ✅ 방 진입/퇴장/호스트 변경 UI 트리거 (Pending 해제의 핵심)
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_Room"));
}

void AMosesPlayerState::OnRep_PlayerNickName()
{
	UE_LOG(LogMosesPlayer, Warning, TEXT("[Nick][CL][PS] OnRep Nick=%s PS=%s Pid=%s"),
		*PlayerNickName,
		*GetNameSafe(this),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens));

	// [ADD] ✅ 닉 변경도 동일 파이프
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_PlayerNickName"));
}

void AMosesPlayerState::NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	UMosesLobbyLocalPlayerSubsystem* LPS = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>();
	if (!LPS)
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Verbose, TEXT("[LPS][NotifyFromPS] Reason=%s PS=%s"), Reason ? Reason : TEXT("None"), *GetNameSafe(this));

	// 기존 파이프 재사용
	LPS->NotifyPlayerStateChanged();
}

// ---- 아래 함수들은 네가 이미 가진 구현 유지(여기서는 생략해도 됨) ----
void AMosesPlayerState::BroadcastSelectedCharacterChanged(const TCHAR* Reason)
{
	const int32 NewId = SelectedCharacterId;
	OnSelectedCharacterChangedNative.Broadcast(NewId);
	OnSelectedCharacterChangedBP.Broadcast(NewId);
}

void AMosesPlayerState::BindCombatDelegatesOnce()
{
	if (bCombatDelegatesBound || !CombatComponent)
	{
		return;
	}

	bCombatDelegatesBound = true;

	CombatComponent->OnCombatDataChangedNative.AddUObject(this, &ThisClass::HandleCombatDataChanged_Native);
	CombatComponent->OnCombatDataChangedBP.AddDynamic(this, &ThisClass::HandleCombatDataChanged_BP);
}

void AMosesPlayerState::HandleCombatDataChanged_Native(const TCHAR* Reason)
{
}

void AMosesPlayerState::HandleCombatDataChanged_BP(FString Reason)
{
}

// [ADD] 링커 에러 해결: 선언만 있고 정의가 없던 함수 구현
void AMosesPlayerState::DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const
{
	const TCHAR* CallerName = Caller ? *Caller->GetName() : TEXT("None");
	const TCHAR* PhaseStr = Phase ? Phase : TEXT("None");

	UE_LOG(LogMosesPlayer, Warning,
		TEXT("[PS][DOD][%s] PS=%s Caller=%s Nick=%s Pid=%s RoomId=%s Host=%d Ready=%d LoggedIn=%d SelChar=%d"),
		PhaseStr,
		*GetNameSafe(this),
		CallerName,
		*PlayerNickName,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		bIsRoomHost ? 1 : 0,
		bReady ? 1 : 0,
		bLoggedIn ? 1 : 0,
		SelectedCharacterId);
}
