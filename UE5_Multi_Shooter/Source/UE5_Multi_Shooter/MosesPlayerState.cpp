// ============================================================================
// UE5_Multi_Shooter/MosesPlayerState.cpp  (FULL - UPDATED)  [MOD]
// ----------------------------------------------------------------------------
// [MOD] Changes
//  - Player respawn delay: 10.0f -> 5.0f
//  - On death confirm (server): push announcement "5초 뒤에 리스폰 됩니다." (1 sec)
//  - Keep SSOT: bIsDead + RespawnEndServerTime + GM->ServerScheduleRespawn stay consistent
// ============================================================================

#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/Match/GameMode/MosesMatchGameMode.h"
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesSlotOwnershipComponent.h"
#include "UE5_Multi_Shooter/Match/Flag/MosesCaptureComponent.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h" // ✅ [MOD] Announcement 출력용

#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"
#include "UE5_Multi_Shooter/GAS/MosesAbilitySet.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

AMosesPlayerState::AMosesPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetNetUpdateFrequency(100.f);

	CaptureComponent = CreateDefaultSubobject<UMosesCaptureComponent>(TEXT("MosesCaptureComponent"));

	MosesAbilitySystemComponent = CreateDefaultSubobject<UMosesAbilitySystemComponent>(TEXT("MosesASC"));
	MosesAbilitySystemComponent->SetIsReplicated(true);
	MosesAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMosesAttributeSet>(TEXT("MosesAttributeSet"));

	CombatComponent = CreateDefaultSubobject<UMosesCombatComponent>(TEXT("MosesCombatComponent"));
	SlotOwnershipComponent = CreateDefaultSubobject<UMosesSlotOwnershipComponent>(TEXT("MosesSlotOwnershipComponent"));

	UE_LOG(LogMosesPlayer, Warning, TEXT("[PS][CTOR] SSOT Components Created Combat=%s Slots=%s ASC=%s"),
		*GetNameSafe(CombatComponent),
		*GetNameSafe(SlotOwnershipComponent),
		*GetNameSafe(MosesAbilitySystemComponent));
}

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

	DOREPLIFETIME(AMosesPlayerState, Deaths);
	DOREPLIFETIME(AMosesPlayerState, Captures);
	DOREPLIFETIME(AMosesPlayerState, ZombieKills);
	DOREPLIFETIME(AMosesPlayerState, PvPKills);
	DOREPLIFETIME(AMosesPlayerState, Headshots);

	DOREPLIFETIME(AMosesPlayerState, bIsDead);
	DOREPLIFETIME(AMosesPlayerState, RespawnEndServerTime);
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
	NewPS->Captures = Captures;
	NewPS->ZombieKills = ZombieKills;
	NewPS->PvPKills = PvPKills;
	NewPS->Headshots = Headshots;

	NewPS->bIsDead = bIsDead;
	NewPS->RespawnEndServerTime = RespawnEndServerTime;
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
	Captures = OldPS->Captures;
	ZombieKills = OldPS->ZombieKills;
	PvPKills = OldPS->PvPKills;
	Headshots = OldPS->Headshots;

	bIsDead = OldPS->bIsDead;
	RespawnEndServerTime = OldPS->RespawnEndServerTime;
}

void AMosesPlayerState::OnRep_Score()
{
	Super::OnRep_Score();
	BroadcastScore();
}

UAbilitySystemComponent* AMosesPlayerState::GetAbilitySystemComponent() const
{
	return MosesAbilitySystemComponent;
}

// ============================================================================
// GAS Init
// ============================================================================

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

	UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][PS] InitAbilityActorInfo Owner=%s Avatar=%s"),
		*GetNameSafe(this), *GetNameSafe(InAvatarActor));

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

	BroadcastPlayerCaptures();
	BroadcastPlayerZombieKills();
	BroadcastPlayerPvPKills();
	BroadcastAmmoAndGrenade();
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

// ============================================================================
// AbilitySet Apply (Server only)
// ============================================================================

void AMosesPlayerState::ServerApplyCombatAbilitySetOnce(UMosesAbilitySet* InAbilitySet)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "GAS", TEXT("Client attempted ServerApplyCombatAbilitySetOnce"));

	if (bCombatAbilitySetApplied)
	{
		return;
	}

	if (!InAbilitySet || !MosesAbilitySystemComponent)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV][PS] ApplyAbilitySet FAIL PS=%s"), *GetNameSafe(this));
		return;
	}

	InAbilitySet->GiveToAbilitySystem(*MosesAbilitySystemComponent, CombatAbilitySetHandles);
	bCombatAbilitySetApplied = true;

	UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV][PS] AbilitySet Applied PS=%s Set=%s"),
		*GetNameSafe(this), *GetNameSafe(InAbilitySet));
}

// ============================================================================
// Match default loadout: Rifle + 30/90 (Server)
// ============================================================================

void AMosesPlayerState::ServerEnsureMatchDefaultLoadout()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "WEAPON", TEXT("Client attempted ServerEnsureMatchDefaultLoadout"));

	if (bMatchDefaultLoadoutGranted)
	{
		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][SV][PS] DefaultMatchLoadout already granted PS=%s"), *GetNameSafe(this));
		return;
	}

	if (!CombatComponent)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV][PS] DefaultMatchLoadout FAIL (No CombatComponent) PS=%s"), *GetNameSafe(this));
		return;
	}

	const FGameplayTag Slot1Rifle = FGameplayTag::RequestGameplayTag(FName(TEXT("Weapon.Rifle.A")));
	const FGameplayTag NoneTag;

	CombatComponent->ServerInitDefaultSlots_4(Slot1Rifle, NoneTag, NoneTag, NoneTag);
	CombatComponent->ServerGrantDefaultRifleAmmo_30_90();

	bMatchDefaultLoadoutGranted = true;

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV][PS] DefaultMatchLoadout Granted Weapon=Weapon.Rifle.A Ammo=30/90 PS=%s"),
		*GetNameSafe(this));

	BroadcastAmmoAndGrenade();
}

// ============================================================================
// Lobby / Info server functions
// ============================================================================

void AMosesPlayerState::EnsurePersistentId_Server()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PS", TEXT("Client attempted EnsurePersistentId_Server"));

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

	const int32 OldDeaths = Deaths;
	Deaths++;
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("%s Deaths %d -> %d PS=%s"),
		MOSES_TAG_SCORE_SV, OldDeaths, Deaths, *GetNameSafe(this));

	OnRep_Deaths();
}

// ============================================================================
// Score SSOT
// ============================================================================

void AMosesPlayerState::ServerAddScore(int32 Delta, const TCHAR* Reason)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "SCORE", TEXT("Client attempted ServerAddScore"));

	if (Delta == 0)
	{
		return;
	}

	const int32 OldScore = FMath::RoundToInt(GetScore());
	const int32 NewScore = OldScore + Delta;

	SetScore(static_cast<float>(NewScore));

	OnScoreChanged.Broadcast(NewScore);

	UE_LOG(LogMosesPlayer, Warning, TEXT("[SCORE][SV] %s Old=%d -> New=%d Delta=%d PS=%s"),
		Reason ? Reason : TEXT("None"),
		OldScore,
		NewScore,
		Delta,
		*GetNameSafe(this));

	ForceNetUpdate();
}

// ============================================================================
// Match stats (Server Authority ONLY)
// ============================================================================

void AMosesPlayerState::ServerAddCapture(int32 Delta)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "CAPTURE", TEXT("Client attempted ServerAddCapture"));

	if (Delta == 0)
	{
		return;
	}

	const int32 Old = Captures;
	Captures = FMath::Max(0, Captures + Delta);
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("[CAPTURE][SV][PS] Captures %d -> %d Delta=%d PS=%s"),
		Old, Captures, Delta, *GetNameSafe(this));

	OnRep_Captures();
}

void AMosesPlayerState::ServerAddZombieKill(int32 Delta)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "ZOMBIE", TEXT("Client attempted ServerAddZombieKill"));

	if (Delta == 0)
	{
		return;
	}

	const int32 Old = ZombieKills;
	ZombieKills = FMath::Max(0, ZombieKills + Delta);
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("[ZOMBIE][SV][PS] ZombieKills %d -> %d Delta=%d PS=%s"),
		Old, ZombieKills, Delta, *GetNameSafe(this));

	OnRep_ZombieKills();
}

void AMosesPlayerState::ServerAddPvPKill(int32 Delta)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "PVP", TEXT("Client attempted ServerAddPvPKill"));

	if (Delta == 0)
	{
		return;
	}

	const int32 Old = PvPKills;
	PvPKills = FMath::Max(0, PvPKills + Delta);

	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("[PVP][SV][PS] PvPKills %d -> %d Delta=%d PS=%s"),
		Old, PvPKills, Delta, *GetNameSafe(this));

	OnRep_PvPKills();
}

void AMosesPlayerState::ServerAddHeadshot(int32 Delta)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "HEADSHOT", TEXT("Client attempted ServerAddHeadshot"));

	const int32 Old = Headshots;
	Headshots = FMath::Max(0, Headshots + Delta);
	ForceNetUpdate();

	UE_LOG(LogMosesPlayer, Warning, TEXT("[HEADSHOT][SV][PS] Headshots %d -> %d Delta=%d PS=%s"),
		Old, Headshots, Delta, *GetNameSafe(this));

	OnRep_Headshots();
}

// ============================================================================
// [FIX] Death / Respawn state (SSOT=PlayerState + CombatComponent)
// ============================================================================

void AMosesPlayerState::ServerNotifyDeathFromGAS()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDead)
	{
		UE_LOG(LogMosesSpawn, Verbose, TEXT("[DEATH][SV][PS] SKIP AlreadyDead PS=%s"), *GetNameSafe(this));
		return;
	}

	// ---------------------------------------------------------------------
	// 1) Dead state (SSOT)
	// ---------------------------------------------------------------------
	bIsDead = true;

	if (UMosesCombatComponent* Combat = GetCombatComponent())
	{
		Combat->ServerMarkDead();
	}

	// ---------------------------------------------------------------------
	// 2) Respawn time (5 seconds)
	// ---------------------------------------------------------------------
	const float Delay = 5.0f; // ✅ [MOD] 10 -> 5
	const float Now = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	RespawnEndServerTime = Now + Delay;

	ForceNetUpdate();

	// ListenServer 즉시 UI 갱신
	OnRep_DeathState();

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[DEATH][SV][PS] MarkDead OK bIsDead=1 RespawnEnd=%.2f Delay=%.2f PS=%s Pawn=%s"),
		RespawnEndServerTime,
		Delay,
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()));

	// ---------------------------------------------------------------------
	// 3) Announcement (Server -> Rep to all clients)
	// - "5초 뒤에 리스폰됩니다."를 5초 동안 표시
	// ---------------------------------------------------------------------
	if (UWorld* World = GetWorld())
	{
		if (AMosesMatchGameState* MGS = World->GetGameState<AMosesMatchGameState>())
		{
			// ✅ [MOD] 너가 원하는 한글 고정 문구
			//MGS->ServerStartAnnouncementText(FText::FromString(TEXT("잠시 후 리스폰됩니다.")), 5);

			UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] DeathRespawnNotice -> 5 sec PS=%s"), *GetNameSafe(this));
		}
	}

	// ---------------------------------------------------------------------
	// 4) Ask GameMode to schedule respawn
	// ---------------------------------------------------------------------
	APawn* Pawn = GetPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;

	if (Controller)
	{
		if (AMosesMatchGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesMatchGameMode>() : nullptr)
		{
			GM->ServerScheduleRespawn(Controller, Delay);

			UE_LOG(LogMosesSpawn, Warning,
				TEXT("[RESPAWN][SV] Schedule REQ Delay=%.2f Controller=%s Pawn=%s PS=%s"),
				Delay,
				*GetNameSafe(Controller),
				*GetNameSafe(Pawn),
				*GetNameSafe(this));
		}
	}
}

void AMosesPlayerState::ServerClearDeadAfterRespawn()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "RESPAWN", TEXT("Client attempted ServerClearDeadAfterRespawn"));

	if (!bIsDead)
	{
		return;
	}

	bIsDead = false;
	RespawnEndServerTime = 0.0f;

	ForceNetUpdate();
	OnRep_DeathState();

	if (UMosesCombatComponent* Combat = GetCombatComponent())
	{
		Combat->ServerClearDeadAfterRespawn();
	}

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[RESPAWN][SV][PS] ClearDead DONE bIsDead=0 PS=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()));
}

// ============================================================================
// Shield Regen
// ============================================================================

void AMosesPlayerState::ServerStartShieldRegen()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "ARMOR", TEXT("Client attempted ServerStartShieldRegen"));

	if (TimerHandle_ShieldRegen.IsValid())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		TimerHandle_ShieldRegen,
		[this]()
		{
			if (!HasAuthority() || !MosesAbilitySystemComponent)
			{
				return;
			}

			if (bIsDead)
			{
				return;
			}

			if (!GE_ShieldRegen_One)
			{
				UE_LOG(LogMosesCombat, Warning, TEXT("[ARMOR][SV] Regen SKIP (GE_ShieldRegen_One NULL) PS=%s"), *GetNameSafe(this));
				return;
			}

			const float Cur = MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetShieldAttribute());
			const float Max = MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxShieldAttribute());

			if (Cur >= Max)
			{
				return;
			}

			FGameplayEffectContextHandle Ctx = MosesAbilitySystemComponent->MakeEffectContext();
			Ctx.AddSourceObject(this);

			FGameplayEffectSpecHandle Spec = MosesAbilitySystemComponent->MakeOutgoingSpec(GE_ShieldRegen_One, 1.0f, Ctx);
			if (Spec.IsValid())
			{
				MosesAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

				UE_LOG(LogMosesCombat, Warning, TEXT("[ARMOR][SV] Regen +1 Shield=%.0f/%.0f PS=%s"),
					MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetShieldAttribute()),
					Max,
					*GetNameSafe(this));
			}
		},
		15.0f,
		true);

	UE_LOG(LogMosesCombat, Warning, TEXT("[ARMOR][SV] RegenTimer START PS=%s"), *GetNameSafe(this));
}

void AMosesPlayerState::ServerStopShieldRegen()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "ARMOR", TEXT("Client attempted ServerStopShieldRegen"));

	if (TimerHandle_ShieldRegen.IsValid())
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_ShieldRegen);
		UE_LOG(LogMosesCombat, Warning, TEXT("[ARMOR][SV] RegenTimer STOP PS=%s"), *GetNameSafe(this));
	}
}

// ============================================================================
// RepNotifies
// ============================================================================

void AMosesPlayerState::OnRep_PersistentId() { NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_PersistentId")); }
void AMosesPlayerState::OnRep_LoggedIn() { NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_LoggedIn")); }
void AMosesPlayerState::OnRep_Ready() { NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_Ready")); }

void AMosesPlayerState::OnRep_SelectedCharacterId()
{
	BroadcastSelectedCharacterChanged(TEXT("OnRep_SelectedCharacterId"));
	NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_SelectedCharacterId"));
}

void AMosesPlayerState::OnRep_Room() { NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_Room")); }
void AMosesPlayerState::OnRep_PlayerNickName() { NotifyLobbyPlayerStateChanged_Local(TEXT("OnRep_PlayerNickName")); }

void AMosesPlayerState::OnRep_Deaths() { BroadcastDeaths(); }
void AMosesPlayerState::OnRep_Captures() { BroadcastPlayerCaptures(); }
void AMosesPlayerState::OnRep_ZombieKills() { BroadcastPlayerZombieKills(); }

void AMosesPlayerState::OnRep_PvPKills() { BroadcastPlayerPvPKills(); }
void AMosesPlayerState::OnRep_Headshots() { BroadcastPlayerHeadshots(); }

void AMosesPlayerState::OnRep_DeathState() { BroadcastDeathState(); }

// ============================================================================
// Broadcast
// ============================================================================

void AMosesPlayerState::BroadcastSelectedCharacterChanged(const TCHAR* Reason)
{
	const int32 NewId = SelectedCharacterId;

	OnSelectedCharacterChangedNative.Broadcast(NewId);
	OnSelectedCharacterChangedBP.Broadcast(NewId);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PS] SelectedCharacterChanged Reason=%s Id=%d"),
		Reason ? Reason : TEXT("None"), NewId);
}

void AMosesPlayerState::BroadcastAmmoAndGrenade()
{
	int32 Mag = 0;
	int32 Reserve = 0;

	if (CombatComponent)
	{
		Mag = CombatComponent->GetCurrentMagAmmo();
		Reserve = CombatComponent->GetCurrentReserveAmmo();
	}

	OnAmmoChanged.Broadcast(Mag, Reserve);
	OnGrenadeChanged.Broadcast(0);
}

void AMosesPlayerState::BroadcastScore()
{
	const int32 IntScore = FMath::RoundToInt(GetScore());
	OnScoreChanged.Broadcast(IntScore);
}

void AMosesPlayerState::BroadcastDeaths()
{
	OnDeathsChanged.Broadcast(Deaths);
}

void AMosesPlayerState::BroadcastPlayerCaptures()
{
	OnPlayerCapturesChanged.Broadcast(Captures);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[CAPTURE][CL][PS] BroadcastPlayerCaptures=%d PS=%s"),
		Captures, *GetNameSafe(this));
}

void AMosesPlayerState::BroadcastPlayerZombieKills()
{
	OnPlayerZombieKillsChanged.Broadcast(ZombieKills);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[ZOMBIE][CL][PS] BroadcastPlayerZombieKills=%d PS=%s"),
		ZombieKills, *GetNameSafe(this));
}

void AMosesPlayerState::BroadcastPlayerPvPKills()
{
	OnPlayerPvPKillsChanged.Broadcast(PvPKills);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[PVP][CL][PS] Broadcast PvPKills=%d PS=%s"),
		PvPKills, *GetNameSafe(this));

	BroadcastTotalKills();
}

void AMosesPlayerState::BroadcastPlayerHeadshots()
{
	OnPlayerHeadshotsChanged.Broadcast(Headshots);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[HEADSHOT][CL][PS] Broadcast Headshots=%d PS=%s"),
		Headshots, *GetNameSafe(this));
}

void AMosesPlayerState::BroadcastDeathState()
{
	OnDeathStateChanged.Broadcast(bIsDead, RespawnEndServerTime);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DEATH][CL] bIsDead=%d RespawnEndServerTime=%.2f PS=%s"),
		bIsDead ? 1 : 0, RespawnEndServerTime, *GetNameSafe(this));
}

void AMosesPlayerState::BroadcastTotalKills()
{
	OnTotalKillsChanged.Broadcast(GetTotalKills());
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

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[LPS][NotifyFromPS] Reason=%s PS=%s"),
		Reason ? Reason : TEXT("None"),
		*GetNameSafe(this));

	LPS->NotifyPlayerStateChanged();
}

void AMosesPlayerState::DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const
{
	const TCHAR* CallerName = Caller ? *Caller->GetName() : TEXT("None");
	const TCHAR* PhaseStr = Phase ? Phase : TEXT("None");

	UE_LOG(LogMosesPlayer, Warning,
		TEXT("[PS][DOD][%s] PS=%s Caller=%s Nick=%s Pid=%s RoomId=%s Host=%d Ready=%d LoggedIn=%d SelChar=%d Deaths=%d Captures=%d ZombieKills=%d Score=%d"),
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
		Captures,
		ZombieKills,
		FMath::RoundToInt(GetScore()));
}

float AMosesPlayerState::GetHealth_Current() const
{
	return MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetHealthAttribute())
		: 0.f;
}

float AMosesPlayerState::GetHealth_Max() const
{
	return MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxHealthAttribute())
		: 0.f;
}

float AMosesPlayerState::GetShield_Current() const
{
	return MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetShieldAttribute())
		: 0.f;
}

float AMosesPlayerState::GetShield_Max() const
{
	return MosesAbilitySystemComponent
		? MosesAbilitySystemComponent->GetNumericAttribute(UMosesAttributeSet::GetMaxShieldAttribute())
		: 0.f;
}

// ============================================================================
// [MOD][DAMAGE_POLICY] Getters
// ============================================================================

TSubclassOf<UGameplayEffect> AMosesPlayerState::GetDamageGE_Player_SetByCaller() const
{
	return DamageGE_Player_SetByCaller.LoadSynchronous();
}

TSubclassOf<UGameplayEffect> AMosesPlayerState::GetDamageGE_Zombie_SetByCaller() const
{
	return DamageGE_Zombie_SetByCaller.LoadSynchronous();
}
