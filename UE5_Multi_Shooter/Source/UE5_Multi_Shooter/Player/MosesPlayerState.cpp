#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

// Project
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

// GAS
#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"
#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"

// Engine
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

// ------------------------------------------------------------
// ctor
// ------------------------------------------------------------

AMosesPlayerState::AMosesPlayerState()
{
	bReplicates = true;
	SetNetUpdateFrequency(100.f);

	MosesAbilitySystemComponent = CreateDefaultSubobject<UMosesAbilitySystemComponent>(TEXT("MosesASC"));
	MosesAbilitySystemComponent->SetIsReplicated(true);
	MosesAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMosesAttributeSet>(TEXT("MosesAttributeSet"));
	CombatComponent = CreateDefaultSubobject<UMosesCombatComponent>(TEXT("MosesCombatComponent"));
}

// ------------------------------------------------------------
// Engine / Net
// ------------------------------------------------------------

void AMosesPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
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
}

// ------------------------------------------------------------
// GAS Init
// ------------------------------------------------------------

void AMosesPlayerState::TryInitASC(AActor* InAvatarActor)
{
	if (!MosesAbilitySystemComponent || !IsValid(InAvatarActor))
	{
		return;
	}

	const bool bAvatarChanged = (CachedAvatar.Get() != InAvatarActor);

	if (bASCInitialized && !bAvatarChanged)
	{
		UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS] ASC Init SKIP PS=%s Avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InAvatarActor));
		return;
	}

	CachedAvatar = InAvatarActor;
	bASCInitialized = true;

	MosesAbilitySystemComponent->InitAbilityActorInfo(this, InAvatarActor);

	UE_LOG(LogMosesGAS, Log, TEXT("[GAS] ASC Init PS=%s Avatar=%s Role=%s AvatarChanged=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InAvatarActor),
		*UEnum::GetValueAsString(GetLocalRole()),
		bAvatarChanged ? 1 : 0);

	LogAttributes();
}

// ------------------------------------------------------------
// Server-only GameplayTag Policy
// ------------------------------------------------------------

void AMosesPlayerState::ServerSetCombatPhase(bool bEnable)
{
	check(HasAuthority());

	if (!MosesAbilitySystemComponent)
	{
		return;
	}

	const FMosesGameplayTags& MosesTags = FMosesGameplayTags::Get(); // [MOD-FIX] 이름 충돌 방지(AActor::Tags)

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
	check(HasAuthority());

	if (!MosesAbilitySystemComponent)
	{
		return;
	}

	const FMosesGameplayTags& MosesTags = FMosesGameplayTags::Get(); // [MOD-FIX] 이름 충돌 방지(AActor::Tags)

	if (bEnable)
	{
		MosesAbilitySystemComponent->AddLooseGameplayTag(MosesTags.State_Dead);
		MosesAbilitySystemComponent->RemoveLooseGameplayTag(MosesTags.State_Phase_Combat);
	}
	else
	{
		MosesAbilitySystemComponent->RemoveLooseGameplayTag(MosesTags.State_Dead);
	}
}

// ------------------------------------------------------------
// Server authoritative setters
// ------------------------------------------------------------

void AMosesPlayerState::ServerSetLoggedIn(bool bInLoggedIn)
{
	check(HasAuthority());

	if (bLoggedIn == bInLoggedIn)
	{
		return;
	}

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

void AMosesPlayerState::ServerSetSelectedCharacterId_Implementation(int32 InId)
{
	check(HasAuthority());

	SelectedCharacterId = FMath::Max(0, InId);

	UE_LOG(LogMosesPlayer, Log, TEXT("[CharSel][SV][PS] Set SelectedCharacterId=%d Pid=%s"),
		SelectedCharacterId,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens));

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

// ------------------------------------------------------------
// Debug / DoD
// ------------------------------------------------------------

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

// ------------------------------------------------------------
// Local notify helper (UI entry)
// ------------------------------------------------------------

static void NotifyOwningLocalPlayer_PSChanged(AMosesPlayerState* PS)
{
	if (!PS)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
	if (!PC)
	{
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

// ------------------------------------------------------------
// RepNotify
// ------------------------------------------------------------

void AMosesPlayerState::OnRep_PersistentId()
{
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

// ------------------------------------------------------------
// Server-only helpers
// ------------------------------------------------------------

void AMosesPlayerState::EnsurePersistentId_Server()
{
	check(HasAuthority());

	if (PersistentId.IsValid())
	{
		return;
	}

	PersistentId = FGuid::NewGuid();
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Log, TEXT("[PS][SV] EnsurePersistentId_Server -> %s PS=%s"),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetNameSafe(this));
}

void AMosesPlayerState::SetLoggedIn_Server(bool bInLoggedIn)
{
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

// ------------------------------------------------------------
// Internal logs
// ------------------------------------------------------------

void AMosesPlayerState::LogASCInit(AActor* InAvatarActor) const
{
	UE_LOG(LogMosesGAS, Log, TEXT("[GAS] ASC Init PS=%s Avatar=%s Role=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InAvatarActor),
		*UEnum::GetValueAsString(GetLocalRole()));
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
