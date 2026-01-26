#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"

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

	// [MOD] Combat 초기값은 서버에서만 확정(중복 호출 안전)
	if (HasAuthority() && CombatComponent)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("%s PS PostInit -> EnsureCombatInit PS=%s"),
			MOSES_TAG_COMBAT_SV, *GetNameSafe(this));

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
	DOREPLIFETIME(AMosesPlayerState, Deaths);
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
	NewPS->Deaths = Deaths;
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
	Deaths = OldPS->Deaths;
}

void AMosesPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s OnRep_Score PS=%s Score=%.0f"),
		MOSES_TAG_SCORE_CL, *GetNameSafe(this), GetScore());

	BroadcastScore();
}

UAbilitySystemComponent* AMosesPlayerState::GetAbilitySystemComponent() const
{
	return MosesAbilitySystemComponent;
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

	// Owner = PS, Avatar = Pawn
	MosesAbilitySystemComponent->InitAbilityActorInfo(this, InAvatarActor);

	UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][PS] InitAbilityActorInfo Owner=%s Avatar=%s"),
		*GetNameSafe(this), *GetNameSafe(InAvatarActor));

	// 서버에서 초기 Attribute 확정(속성은 GAS)
	if (HasAuthority())
	{
		MosesAbilitySystemComponent->SetNumericAttributeBase(UMosesAttributeSet::GetMaxHealthAttribute(), 100.f);
		MosesAbilitySystemComponent->SetNumericAttributeBase(UMosesAttributeSet::GetHealthAttribute(), 100.f);

		MosesAbilitySystemComponent->SetNumericAttributeBase(UMosesAttributeSet::GetMaxShieldAttribute(), 100.f);
		MosesAbilitySystemComponent->SetNumericAttributeBase(UMosesAttributeSet::GetShieldAttribute(), 100.f);

		UE_LOG(LogMosesHP, Warning, TEXT("%s GAS Defaults Applied HP=100/100 Shield=100/100 PS=%s"),
			MOSES_TAG_PS_SV, *GetNameSafe(this));
	}

	BindASCAttributeDelegates();
	BroadcastVitals_Initial();
	BroadcastScore();
	BroadcastDeaths();
	BroadcastAmmoAndGrenade();
}

void AMosesPlayerState::EnsurePersistentId_Server()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted EnsurePersistentId_Server"));
	check(HasAuthority());

	if (PersistentId.IsValid())
	{
		return;
	}

	PersistentId = FGuid::NewGuid();
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("%s PersistentId Generated %s PS=%s"),
		MOSES_TAG_PS_SV, *PersistentId.ToString(EGuidFormats::DigitsWithHyphens), *GetNameSafe(this));
}

void AMosesPlayerState::ServerSetLoggedIn(bool bInLoggedIn)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted ServerSetLoggedIn"));
	check(HasAuthority());

	if (bLoggedIn == bInLoggedIn)
	{
		return;
	}

	bLoggedIn = bInLoggedIn;
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s ServerSetLoggedIn=%d PS=%s"),
		MOSES_TAG_PS_SV, bLoggedIn ? 1 : 0, *GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetLoggedIn"));
}

void AMosesPlayerState::ServerSetReady(bool bInReady)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted ServerSetReady"));
	check(HasAuthority());

	if (bReady == bInReady)
	{
		return;
	}

	bReady = bInReady;
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s ServerSetReady=%d PS=%s"),
		MOSES_TAG_PS_SV, bReady ? 1 : 0, *GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetReady"));
}

void AMosesPlayerState::ServerSetSelectedCharacterId_Implementation(int32 InId)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted ServerSetSelectedCharacterId"));
	check(HasAuthority());

	SelectedCharacterId = FMath::Max(1, InId);
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s ServerSetSelectedCharacterId=%d PS=%s"),
		MOSES_TAG_PS_SV, SelectedCharacterId, *GetNameSafe(this));

	BroadcastSelectedCharacterChanged(TEXT("ServerSetSelectedCharacterId"));
	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetSelectedCharacterId"));
}

void AMosesPlayerState::ServerSetRoom(const FGuid& InRoomId, bool bInIsHost)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted ServerSetRoom"));
	check(HasAuthority());

	if (RoomId == InRoomId && bIsRoomHost == bInIsHost)
	{
		return;
	}

	RoomId = InRoomId;
	bIsRoomHost = bInIsHost;

	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s ServerSetRoom RoomId=%s Host=%d PS=%s"),
		MOSES_TAG_PS_SV,
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		bIsRoomHost ? 1 : 0,
		*GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetRoom"));
}

void AMosesPlayerState::ServerSetPlayerNickName(const FString& InNickName)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted ServerSetPlayerNickName"));
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

	UE_LOG(LogMosesPlayer, Warning, TEXT("%s ServerSetNickName=%s PS=%s"),
		MOSES_TAG_PS_SV, *PlayerNickName, *GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("ServerSetPlayerNickName"));
}

void AMosesPlayerState::ServerAddDeath()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted ServerAddDeath"));
	check(HasAuthority());

	const int32 OldDeaths = Deaths;
	Deaths++;
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("%s Deaths %d -> %d PS=%s"),
		MOSES_TAG_SCORE_SV, OldDeaths, Deaths, *GetNameSafe(this));

	OnRep_Deaths(); // 리슨서버 즉시 반영
}

void AMosesPlayerState::OnRep_PersistentId()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s OnRep_PersistentId -> %s PS=%s"),
		MOSES_TAG_PS_CL,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_PersistentId"));
}

void AMosesPlayerState::OnRep_LoggedIn()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s OnRep_LoggedIn -> %d PS=%s"),
		MOSES_TAG_PS_CL,
		bLoggedIn ? 1 : 0,
		*GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_LoggedIn"));
}

void AMosesPlayerState::OnRep_Ready()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s OnRep_Ready -> %d PS=%s"),
		MOSES_TAG_PS_CL,
		bReady ? 1 : 0,
		*GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_Ready"));
}

void AMosesPlayerState::OnRep_SelectedCharacterId()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s OnRep_SelectedCharacterId=%d PS=%s"),
		MOSES_TAG_PS_CL,
		SelectedCharacterId,
		*GetNameSafe(this));

	BroadcastSelectedCharacterChanged(TEXT("OnRep_SelectedCharacterId"));
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_SelectedCharacterId"));
}

void AMosesPlayerState::OnRep_Room()
{
	UE_LOG(LogMosesPlayer, Warning, TEXT("%s OnRep_Room RoomId=%s Host=%d PS=%s"),
		MOSES_TAG_PS_CL,
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		bIsRoomHost ? 1 : 0,
		*GetNameSafe(this));

	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_Room"));
}

void AMosesPlayerState::OnRep_PlayerNickName()
{
	UE_LOG(LogMosesPlayer, Warning, TEXT("[Nick][CL][PS] OnRep Nick=%s PS=%s Pid=%s"),
		*PlayerNickName,
		*GetNameSafe(this),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens));

	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_PlayerNickName"));
}

void AMosesPlayerState::OnRep_Deaths()
{
	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s OnRep_Deaths=%d PS=%s"),
		MOSES_TAG_SCORE_CL, Deaths, *GetNameSafe(this));

	BroadcastDeaths();
}

void AMosesPlayerState::BroadcastSelectedCharacterChanged(const TCHAR* Reason)
{
	const int32 NewId = SelectedCharacterId;

	OnSelectedCharacterChangedNative.Broadcast(NewId);
	OnSelectedCharacterChangedBP.Broadcast(NewId);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PS] SelectedCharacterChanged Reason=%s Id=%d"),
		Reason ? Reason : TEXT("None"), NewId);
}

void AMosesPlayerState::NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const
{
	// PlayerState의 Owner는 보통 PlayerController가 맞지만, 안전하게 검사한다.
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

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[LPS][NotifyFromPS] Reason=%s PS=%s"),
		Reason ? Reason : TEXT("None"),
		*GetNameSafe(this));

	LPS->NotifyPlayerStateChanged();
}

void AMosesPlayerState::BindCombatDelegatesOnce()
{
	if (bCombatDelegatesBound || !CombatComponent)
	{
		return;
	}

	bCombatDelegatesBound = true;

	//CombatComponent->OnCombatDataChangedNative.AddUObject(this, &ThisClass::HandleCombatDataChanged_Native);
	//CombatComponent->OnCombatDataChangedBP.AddDynamic(this, &ThisClass::HandleCombatDataChanged_BP);

	UE_LOG(LogMosesCombat, Warning, TEXT("%s Bound Combat delegates PS=%s CC=%s"),
		MOSES_TAG_COMBAT_CL, *GetNameSafe(this), *GetNameSafe(CombatComponent));
}

void AMosesPlayerState::HandleCombatDataChanged_BP(const FString& Reason)
{
	// BP로 처리할 게 있으면 여기서 확장 가능 (현재는 Native 경유로 HUD 브릿지 처리)
}

void AMosesPlayerState::HandleCombatDataChanged_Native(const TCHAR* Reason)
{
	UE_LOG(LogMosesCombat, Verbose, TEXT("%s CombatDataChanged Reason=%s PS=%s"),
		MOSES_TAG_COMBAT_CL, Reason ? Reason : TEXT("None"), *GetNameSafe(this));

	// Combat 변경 -> HUD 브릿지
	BroadcastAmmoAndGrenade();
}

void AMosesPlayerState::BroadcastAmmoAndGrenade()
{
	if (!CombatComponent)
	{
		return;
	}

	//const TArray<FAmmoState>& AmmoStates = CombatComponent->GetAmmoStates();
	//if (AmmoStates.Num() <= 0)
	//{
	//	return;
	//}

	//// [정책] 현재 무기 개념이 없으므로 Rifle(0) 기준으로 표시 (Day2 완료 후 확장)
	//{
	//	const FAmmoState& Rifle = AmmoStates[0];
	//	OnAmmoChanged.Broadcast(Rifle.MagAmmo, Rifle.ReserveAmmo);

	//	UE_LOG(LogMosesCombat, Verbose, TEXT("%s AmmoChanged Rifle %d/%d"),
	//		MOSES_TAG_HUD_CL, Rifle.MagAmmo, Rifle.ReserveAmmo);
	//}

	//// Grenade는 enum 3번, ReserveAmmo를 보유 개수로 사용 중인 정책 유지
	//if (AmmoStates.Num() > 3)
	//{
	//	const FAmmoState& Grenade = AmmoStates[3];
	//	OnGrenadeChanged.Broadcast(Grenade.ReserveAmmo);

	//	UE_LOG(LogMosesCombat, Verbose, TEXT("%s GrenadeChanged Count=%d"),
	//		MOSES_TAG_HUD_CL, Grenade.ReserveAmmo);
	//}
}

void AMosesPlayerState::BroadcastScore()
{
	const int32 IntScore = FMath::RoundToInt(GetScore());
	OnScoreChanged.Broadcast(IntScore);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s ScoreUpdated=%d PS=%s"),
		MOSES_TAG_SCORE_CL, IntScore, *GetNameSafe(this));
}

void AMosesPlayerState::BroadcastDeaths()
{
	OnDeathsChanged.Broadcast(Deaths);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("%s DeathsUpdated=%d PS=%s"),
		MOSES_TAG_SCORE_CL, Deaths, *GetNameSafe(this));
}

void AMosesPlayerState::BindASCAttributeDelegates()
{
	if (bASCDelegatesBound || !MosesAbilitySystemComponent)
	{
		return;
	}

	bASCDelegatesBound = true;

	MosesAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMosesAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged_Internal);

	MosesAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMosesAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleMaxHealthChanged_Internal);

	MosesAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMosesAttributeSet::GetShieldAttribute())
		.AddUObject(this, &ThisClass::HandleShieldChanged_Internal);

	MosesAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMosesAttributeSet::GetMaxShieldAttribute())
		.AddUObject(this, &ThisClass::HandleMaxShieldChanged_Internal);

	UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS][PS] Bound AttributeChange delegates PS=%s"), *GetNameSafe(this));
}

void AMosesPlayerState::BroadcastVitals_Initial()
{
	if (!MosesAbilitySystemComponent)
	{
		return;
	}

	const float CurHP = MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetHealthAttribute());
	const float MaxHP = MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxHealthAttribute());

	const float CurShield = MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetShieldAttribute());
	const float MaxShield = MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxShieldAttribute());

	OnHealthChanged.Broadcast(CurHP, MaxHP);
	OnShieldChanged.Broadcast(CurShield, MaxShield);

	UE_LOG(LogMosesHP, Verbose, TEXT("%s InitialVitals HP=%.0f/%.0f Shield=%.0f/%.0f PS=%s"),
		MOSES_TAG_HUD_CL, CurHP, MaxHP, CurShield, MaxShield, *GetNameSafe(this));
}

void AMosesPlayerState::HandleHealthChanged_Internal(const FOnAttributeChangeData& Data)
{
	const float Cur = Data.NewValue;
	const float Max = MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxHealthAttribute())
		: Cur;

	OnHealthChanged.Broadcast(Cur, Max);

	UE_LOG(LogMosesHP, Verbose, TEXT("%s OnHealthChanged %.0f/%.0f PS=%s"),
		MOSES_TAG_HUD_CL, Cur, Max, *GetNameSafe(this));
}

void AMosesPlayerState::HandleMaxHealthChanged_Internal(const FOnAttributeChangeData& Data)
{
	const float Max = Data.NewValue;
	const float Cur = MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetHealthAttribute())
		: Max;

	OnHealthChanged.Broadcast(Cur, Max);

	UE_LOG(LogMosesHP, Verbose, TEXT("%s OnMaxHealthChanged %.0f/%.0f PS=%s"),
		MOSES_TAG_HUD_CL, Cur, Max, *GetNameSafe(this));
}

void AMosesPlayerState::HandleShieldChanged_Internal(const FOnAttributeChangeData& Data)
{
	const float Cur = Data.NewValue;
	const float Max = MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxShieldAttribute())
		: Cur;

	OnShieldChanged.Broadcast(Cur, Max);

	UE_LOG(LogMosesHP, Verbose, TEXT("%s OnShieldChanged %.0f/%.0f PS=%s"),
		MOSES_TAG_HUD_CL, Cur, Max, *GetNameSafe(this));
}

void AMosesPlayerState::HandleMaxShieldChanged_Internal(const FOnAttributeChangeData& Data)
{
	const float Max = Data.NewValue;
	const float Cur = MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetShieldAttribute())
		: Max;

	OnShieldChanged.Broadcast(Cur, Max);

	UE_LOG(LogMosesHP, Verbose, TEXT("%s OnMaxShieldChanged %.0f/%.0f PS=%s"),
		MOSES_TAG_HUD_CL, Cur, Max, *GetNameSafe(this));
}

void AMosesPlayerState::DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const
{
	const TCHAR* CallerName = Caller ? *Caller->GetName() : TEXT("None");
	const TCHAR* PhaseStr = Phase ? Phase : TEXT("None");

	UE_LOG(LogMosesPlayer, Warning,
		TEXT("[PS][DOD][%s] PS=%s Caller=%s Nick=%s Pid=%s RoomId=%s Host=%d Ready=%d LoggedIn=%d SelChar=%d Deaths=%d Score=%d"),
		PhaseStr,
		*GetNameSafe(this),
		CallerName,
		*PlayerNickName,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*RoomId.ToString(EGuidFormats::DigitsWithHyphens),
		bIsRoomHost ? 1 : 0,
		bReady ? 1 : 0,
		bLoggedIn ? 1 : 0,
		SelectedCharacterId,
		Deaths,
		FMath::RoundToInt(GetScore()));
}
