#include "UE5_Multi_Shooter/Match/GameMode/MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesPlayerController.h"

#include "UE5_Multi_Shooter/Lobby/UI/CharacterSelect/MSCharacterCatalog.h"
#include "UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h"
#include "UE5_Multi_Shooter/Experience/MosesExperienceDefinition.h"

#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "UE5_Multi_Shooter/Persist/MosesMatchRecordStorageSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/Guid.h"

namespace
{
	static int32 FindMaxAndCount(
		const TArray<AMosesPlayerState*>& Players,
		TFunctionRef<int32(const AMosesPlayerState*)> Getter,
		int32& OutMax)
	{
		OutMax = INT32_MIN;

		for (const AMosesPlayerState* PS : Players)
		{
			OutMax = FMath::Max(OutMax, Getter(PS));
		}

		int32 Count = 0;
		for (const AMosesPlayerState* PS : Players)
		{
			if (Getter(PS) == OutMax)
			{
				++Count;
			}
		}

		return Count;
	}
}

AMosesMatchGameMode::AMosesMatchGameMode()
{
	bUseSeamlessTravel = true;

	PlayerStateClass = AMosesPlayerState::StaticClass();
	PlayerControllerClass = AMosesPlayerController::StaticClass();

	DefaultPawnClass = nullptr;
	FallbackPawnClass = nullptr;

	// [중요] 매치 레벨에서는 MatchGameState를 사용한다.
	GameStateClass = AMosesMatchGameState::StaticClass();

	// [MOD] Warmup 30초 요구사항 기본값 (BeginPlay에서도 서버에서 강제)
	WarmupSeconds = 30.0f;

	UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM] Ctor OK (GameStateClass=MatchGameState) WarmupSeconds=%.2f"),
		MOSES_TAG_RESPAWN_SV, WarmupSeconds);
}

void AMosesMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	FString FinalOptions = Options;

	// Experience 옵션이 없으면 Warmup으로 강제
	if (!UGameplayStatics::HasOption(Options, TEXT("Experience")))
	{
		FinalOptions += TEXT("?Experience=Exp_Match_Warmup");
	}

	Super::InitGame(MapName, FinalOptions, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][InitGame] Map=%s Options=%s"),
		MOSES_TAG_COMBAT_SV, *MapName, *FinalOptions);
}

void AMosesMatchGameMode::BeginPlay()
{
	Super::BeginPlay();

	// [MOD] BP/ini가 WarmupSeconds를 덮어썼을 수 있으므로 서버에서 30초로 고정
	if (HasAuthority())
	{
		const float Before = WarmupSeconds;
		WarmupSeconds = 30.0f;

		UE_LOG(LogMosesPhase, Warning,
			TEXT("[PHASE][SV] WarmupSeconds FORCE 30 (Before=%.2f After=%.2f) GM=%s"),
			Before, WarmupSeconds, *GetNameSafe(this));
	}

	DumpAllDODPlayerStates(TEXT("MatchGM:BeginPlay"));
	DumpPlayerStates(TEXT("[MatchGM][BeginPlay]"));

	if (AutoReturnToLobbySeconds > 0.f && HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			AutoReturnTimerHandle,
			this,
			&ThisClass::HandleAutoReturn,
			AutoReturnToLobbySeconds,
			false);

		UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM] AutoReturnToLobby in %.2f sec"),
			MOSES_TAG_COMBAT_SV, AutoReturnToLobbySeconds);
	}

	StartWarmup_WhenExperienceReady();
}

void AMosesMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	APlayerState* PS = NewPlayer ? NewPlayer->PlayerState : nullptr;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("%s [MatchGM][PostLogin] PC=%s PS=%p PSName=%s PlayerId=%d PlayerName=%s"),
		MOSES_TAG_COMBAT_SV,
		*GetNameSafe(NewPlayer),
		PS,
		*GetNameSafe(PS),
		PS ? PS->GetPlayerId() : -1,
		PS ? *PS->GetPlayerName() : TEXT("None"));

	// Match 기본 로드아웃 보장
	Server_EnsureDefaultMatchLoadout(NewPlayer, TEXT("PostLogin"));
}

void AMosesMatchGameMode::Logout(AController* Exiting)
{
	ReleaseReservedStart(Exiting);
	Super::Logout(Exiting);
}

void AMosesMatchGameMode::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	Super::HandleDoD_AfterExperienceReady(CurrentExperience);

	UE_LOG(LogMosesExp, Warning,
		TEXT("%s [MatchGM][DOD] AfterExperienceReady -> StartMatchFlow Exp=%s"),
		MOSES_TAG_AUTH_SV,
		*GetNameSafe(CurrentExperience));

	StartMatchFlow_AfterExperienceReady();
}

void AMosesMatchGameMode::StartMatchFlow_AfterExperienceReady()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Respawn", TEXT("Client attempted StartMatchFlow_AfterExperienceReady"));

	// Experience Ready 시점: Pawn 없는 플레이어는 서버가 Spawn 확정
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		Server_EnsureDefaultMatchLoadout(PC, TEXT("READY:BeforeRestart"));

		if (!IsValid(PC->GetPawn()))
		{
			UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM][READY] RestartPlayer (NoPawn) PC=%s"),
				MOSES_TAG_RESPAWN_SV, *GetNameSafe(PC));

			RestartPlayer(PC);

			Server_EnsureDefaultMatchLoadout(PC, TEXT("READY:AfterRestart"));
		}
	}

	// Warmup 시작
	SetMatchPhase(EMosesMatchPhase::Warmup);
}

void AMosesMatchGameMode::HandlePhaseTimerExpired()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Phase", TEXT("Client attempted HandlePhaseTimerExpired"));

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][PHASE] TimerExpired Phase=%s"),
		MOSES_TAG_COMBAT_SV, *UEnum::GetValueAsString(CurrentPhase));

	AdvancePhase();
}

void AMosesMatchGameMode::AdvancePhase()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Phase", TEXT("Client attempted AdvancePhase"));

	switch (CurrentPhase)
	{
	case EMosesMatchPhase::WaitingForPlayers:
		SetMatchPhase(EMosesMatchPhase::Warmup);
		break;

	case EMosesMatchPhase::Warmup:
		SetMatchPhase(EMosesMatchPhase::Combat);
		break;

	case EMosesMatchPhase::Combat:
		SetMatchPhase(EMosesMatchPhase::Result);
		break;

	case EMosesMatchPhase::Result:
	default:
		UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][PHASE] ResultFinished -> TravelToLobby"),
			MOSES_TAG_COMBAT_SV);
		TravelToLobby();
		break;
	}
}

float AMosesMatchGameMode::GetPhaseDurationSeconds(EMosesMatchPhase Phase) const
{
	switch (Phase)
	{
	case EMosesMatchPhase::Warmup: return 30.0f;        // 요구사항 강제
	case EMosesMatchPhase::Combat: return CombatSeconds;
	case EMosesMatchPhase::Result: return ResultSeconds;
	default: break;
	}
	return 0.f;
}

FName AMosesMatchGameMode::GetExperienceNameForPhase(EMosesMatchPhase Phase)
{
	switch (Phase)
	{
	case EMosesMatchPhase::Warmup: return FName(TEXT("Exp_Match_Warmup"));
	case EMosesMatchPhase::Combat: return FName(TEXT("Exp_Match_Combat"));
	case EMosesMatchPhase::Result: return FName(TEXT("Exp_Match_Result"));
	default: break;
	}
	return NAME_None;
}

UMosesExperienceManagerComponent* AMosesMatchGameMode::GetExperienceManager() const
{
	return GameState ? GameState->FindComponentByClass<UMosesExperienceManagerComponent>() : nullptr;
}

void AMosesMatchGameMode::ServerSwitchExperienceByPhase(EMosesMatchPhase Phase)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Experience", TEXT("Client attempted ServerSwitchExperienceByPhase"));

	const FName ExpName = GetExperienceNameForPhase(Phase);
	if (ExpName.IsNone())
	{
		return;
	}

	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));
	const FPrimaryAssetId NewExperienceId(ExperienceType, ExpName);

	UMosesExperienceManagerComponent* ExperienceManagerComponent = GetExperienceManager();
	if (!ExperienceManagerComponent)
	{
		UE_LOG(LogMosesExp, Error, TEXT("[MatchGM][EXP] No ExperienceManagerComponent. Cannot switch Experience."));
		return;
	}

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][EXP] Switch -> %s (Phase=%s)"),
		MOSES_TAG_COMBAT_SV,
		*NewExperienceId.ToString(),
		*UEnum::GetValueAsString(Phase));

	ExperienceManagerComponent->ServerSetCurrentExperience(NewExperienceId);
}

AMosesMatchGameState* AMosesMatchGameMode::GetMatchGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
}

void AMosesMatchGameMode::SetMatchPhase(EMosesMatchPhase NewPhase)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Phase", TEXT("Client attempted SetMatchPhase"));

	if (CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	// 1) Phase에 맞춰 Experience 전환
	ServerSwitchExperienceByPhase(CurrentPhase);

	// 2) 기존 Phase 종료 타이머 정리
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	// 3) Duration 계산
	const float Duration = GetPhaseDurationSeconds(CurrentPhase);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PHASE][SV] -> %s Duration=%.2f GM=%s"),
		*UEnum::GetValueAsString(CurrentPhase), Duration, *GetNameSafe(this));

	// 4) MatchGameState에 Phase/Timer/Announcement 확정
	if (AMosesMatchGameState* GS = GetMatchGameState())
	{
		GS->ServerSetMatchPhase(CurrentPhase);

		const int32 DurationInt = (Duration > 0.f) ? FMath::CeilToInt(Duration) : 0;
		GS->ServerStartMatchTimer(DurationInt);

		if (CurrentPhase == EMosesMatchPhase::Warmup)
		{
			GS->ServerStartAnnouncementText(FText::FromString(TEXT("워밍업 시작")), 3);
		}
		else if (CurrentPhase == EMosesMatchPhase::Combat)
		{
			GS->ServerStartAnnouncementText(FText::FromString(TEXT("매치 시작")), 3);
		}
		else if (CurrentPhase == EMosesMatchPhase::Result)
		{
			GS->ServerStartAnnouncementCountdown(FText::FromString(TEXT("로비 복귀까지")), FMath::Max(1, DurationInt));

			// [MOD] Result 진입 즉시 서버 승패 확정
			ServerDecideResult_OnEnterResultPhase();
		}
	}

	// 5) Phase 종료 타이머(GameMode 결정)
	if (Duration > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			PhaseTimerHandle,
			this,
			&ThisClass::HandlePhaseTimerExpired,
			Duration,
			false);
	}
}

// ============================================================================
// PlayerStart
// ============================================================================

AActor* AMosesMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!HasAuthority() || !Player)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	if (const TWeakObjectPtr<APlayerStart>* Assigned = AssignedStartByController.Find(Player))
	{
		if (Assigned->IsValid())
		{
			return Assigned->Get();
		}
	}

	TArray<APlayerStart*> AllStarts;
	CollectMatchPlayerStarts(AllStarts);

	TArray<APlayerStart*> FreeStarts;
	FilterFreeStarts(AllStarts, FreeStarts);

	if (FreeStarts.Num() <= 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const int32 Index = FMath::RandRange(0, FreeStarts.Num() - 1);
	APlayerStart* Chosen = FreeStarts[Index];

	ReserveStartForController(Player, Chosen);
	return Chosen;
}

// ============================================================================
// Travel
// ============================================================================

void AMosesMatchGameMode::TravelToLobby()
{
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM] TravelToLobby BLOCKED"), MOSES_TAG_COMBAT_SV);
		return;
	}

	const FString URL = GetLobbyMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM] ServerTravel -> %s"), MOSES_TAG_COMBAT_SV, *URL);

	GetWorld()->ServerTravel(URL, false);
}

void AMosesMatchGameMode::HandleAutoReturn()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM] HandleAutoReturn -> TravelToLobby"), MOSES_TAG_COMBAT_SV);
	TravelToLobby();
}

bool AMosesMatchGameMode::CanDoServerTravel() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return false;
	}

	return HasAuthority();
}

FString AMosesMatchGameMode::GetLobbyMapURL() const
{
	return TEXT("/Game/Map/L_Lobby?Experience=Exp_Lobby");
}

void AMosesMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (!PC || !HasAuthority())
	{
		return;
	}

	Server_EnsureDefaultMatchLoadout(PC, TEXT("HandleSeamlessTravelPlayer"));

	if (!IsValid(PC->GetPawn()))
	{
		RestartPlayer(PC);
		Server_EnsureDefaultMatchLoadout(PC, TEXT("HandleSeamlessTravelPlayer:AfterRestart"));
	}
}

void AMosesMatchGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);
}

APawn* AMosesMatchGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	if (!HasAuthority() || !NewPlayer || !StartSpot)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	TSubclassOf<APawn> PawnClassToSpawn = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClassToSpawn)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	FActorSpawnParameters Params;
	Params.Owner = NewPlayer;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	return GetWorld()->SpawnActor<APawn>(PawnClassToSpawn, StartSpot->GetActorTransform(), Params);
}

UClass* AMosesMatchGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const AMosesPlayerState* PS = InController ? InController->GetPlayerState<AMosesPlayerState>() : nullptr;
	const int32 SelectedId = PS ? PS->GetSelectedCharacterId() : 0;

	if (UClass* Resolved = ResolvePawnClassFromSelectedId(SelectedId))
	{
		return Resolved;
	}

	if (FallbackPawnClass)
	{
		return FallbackPawnClass.Get();
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

UClass* AMosesMatchGameMode::ResolvePawnClassFromSelectedId(int32 SelectedId) const
{
	if (!CharacterCatalog)
	{
		return nullptr;
	}

	const int32 SafeSelectedId = FMath::Clamp(SelectedId, 1, 2);

	FMSCharacterEntry Entry;
	if (!CharacterCatalog->FindByIndex(SafeSelectedId, Entry))
	{
		return nullptr;
	}

	return Entry.PawnClass.LoadSynchronous();
}

// ============================================================================
// Default Loadout (SSOT=PlayerState)
// ============================================================================

void AMosesMatchGameMode::Server_EnsureDefaultMatchLoadout(APlayerController* PC, const TCHAR* FromWhere)
{
	if (!HasAuthority() || !PC)
	{
		return;
	}

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV][MatchGM] EnsureDefaultLoadout FAIL (NoPS) From=%s PC=%s"),
			FromWhere ? FromWhere : TEXT("None"),
			*GetNameSafe(PC));
		return;
	}

	PS->ServerEnsureMatchDefaultLoadout();

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV][MatchGM] EnsureDefaultLoadout OK From=%s PC=%s PS=%s"),
		FromWhere ? FromWhere : TEXT("None"),
		*GetNameSafe(PC),
		*GetNameSafe(PS));
}

// ============================================================================
// Respawn (DAY11)
// ============================================================================

void AMosesMatchGameMode::ServerScheduleRespawn(AController* Controller, float DelaySeconds)
{
	if (!HasAuthority() || !Controller)
	{
		return;
	}

	FTimerHandle Tmp;
	GetWorldTimerManager().SetTimer(
		Tmp,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleRespawnTimerExpired, Controller),
		FMath::Max(0.1f, DelaySeconds),
		false);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[RESPAWN][SV] Scheduled Delay=%.2f Controller=%s"),
		DelaySeconds, *GetNameSafe(Controller));
}

void AMosesMatchGameMode::HandleRespawnTimerExpired(AController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return;
	}

	RestartPlayer(Controller);

	AMosesPlayerState* PS = Controller->GetPlayerState<AMosesPlayerState>();
	if (PS)
	{
		// NOTE:
		// PS에 “bIsDead/RespawnEndServerTime”이 private이면 반드시 PS 함수로 클리어해야 한다.
		// DAY11에서 PS에 아래 같은 함수를 하나 두는게 정답:
		//   void ServerClearDeadAfterRespawn();
		PS->ServerClearDeadAfterRespawn(); // [MOD] (PS에 구현 필요)

		UE_LOG(LogMosesSpawn, Warning, TEXT("[RESPAWN][SV] Restart OK PS=%s"), *GetNameSafe(PS));
	}
}

// ============================================================================
// Result Decide (DAY11)
// ============================================================================

void AMosesMatchGameMode::ServerDecideResult_OnEnterResultPhase()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesMatchGameState* MGS = GetMatchGameState();
	if (!MGS)
	{
		return;
	}

	TArray<AMosesPlayerState*> Players;
	if (GameState)
	{
		for (APlayerState* Raw : GameState->PlayerArray)
		{
			if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(Raw))
			{
				Players.Add(PS);
			}
		}
	}

	AMosesPlayerState* WinnerPS = nullptr;
	EMosesResultReason Reason = EMosesResultReason::None;

	// WinnerPid는 TryChoose에서 채우되, 실제 WinnerPS도 잡아준다.
	FString WinnerPid;
	const bool bHasWinner = TryChooseWinnerByReason_Server(Players, WinnerPid, Reason);

	if (bHasWinner)
	{
		for (AMosesPlayerState* PS : Players)
		{
			if (!PS)
			{
				continue;
			}

			const FString Pid = PS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens);
			if (Pid == WinnerPid)
			{
				WinnerPS = PS;
				break;
			}
		}
	}

	// Reason -> String 변환(저장/표시용)
	auto ReasonToString = [](EMosesResultReason InReason) -> FString
		{
			switch (InReason)
			{
			case EMosesResultReason::Captures:    return TEXT("Captures");
			case EMosesResultReason::PvPKills:    return TEXT("PvPKills");
			case EMosesResultReason::ZombieKills: return TEXT("ZombieKills");
			case EMosesResultReason::Headshots:   return TEXT("Headshots");
			case EMosesResultReason::Draw:        return TEXT("Draw");
			default:                               return TEXT("None");
			}
		};

	FMosesMatchResultState RS;
	RS.bIsResult = true;

	if (!bHasWinner || !WinnerPS)
	{
		RS.bIsDraw = true;
		RS.WinnerPersistentId = TEXT("");
		RS.WinnerNickname = TEXT("");
		RS.ResultReason = TEXT("Draw");

		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] DRAW (All tied or WinnerPS missing)"));
	}
	else
	{
		RS.bIsDraw = false;
		RS.WinnerPersistentId = WinnerPid;
		RS.WinnerNickname = WinnerPS->GetPlayerNickName();
		RS.ResultReason = ReasonToString(Reason);

		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] WinnerPid=%s Nick=%s Reason=%s"),
			*RS.WinnerPersistentId,
			*RS.WinnerNickname,
			*RS.ResultReason);
	}

	MGS->ServerSetResultState(RS);

	// 중앙 방송(데모)
	if (RS.bIsDraw)
	{
		MGS->ServerPushAnnouncement(TEXT("DRAW"), 4.0f);
	}
	else
	{
		MGS->ServerPushAnnouncement(TEXT("RESULT"), 4.0f);
	}

	// ✅ [PERSIST][SV] Result 진입 순간 저장 1회 (반드시 여기서 호출)
	ServerSaveRecord_Once_OnEnterResult();
}

bool AMosesMatchGameMode::TryChooseWinnerByReason_Server(
	const TArray<AMosesPlayerState*>& Players,
	FString& OutWinnerId,
	EMosesResultReason& OutReason) const
{
	struct FRule
	{
		EMosesResultReason Reason;
		TFunction<int32(const AMosesPlayerState*)> Getter;
	};

	const TArray<FRule> Rules =
	{
		{ EMosesResultReason::Captures,    [](const AMosesPlayerState* PS) { return PS->GetCaptures(); } },
		{ EMosesResultReason::PvPKills,    [](const AMosesPlayerState* PS) { return PS->GetPvPKills(); } },
		{ EMosesResultReason::ZombieKills, [](const AMosesPlayerState* PS) { return PS->GetZombieKills(); } },
		{ EMosesResultReason::Headshots,   [](const AMosesPlayerState* PS) { return PS->GetHeadshots(); } },
	};

	for (const FRule& Rule : Rules)
	{
		int32 MaxValue = 0;
		const int32 Count = FindMaxAndCount(Players, Rule.Getter, MaxValue);

		if (Players.Num() > 0 && Count == 1)
		{
			for (const AMosesPlayerState* PS : Players)
			{
				if (Rule.Getter(PS) == MaxValue)
				{
					// ✅ [FIX] OnlineSubsystem 심볼(FUniqueNetIdWrapper) 제거
					OutWinnerId = PS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens);
					OutReason = Rule.Reason;

					UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] Decide WinnerPid=%s Reason=%d Value=%d"),
						*OutWinnerId, (int32)OutReason, MaxValue);
					return true;
				}
			}
		}

		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] Tie Reason=%d Max=%d Count=%d"),
			(int32)Rule.Reason, MaxValue, Count);
	}

	return false;
}

// ============================================================================
// Debug helpers
// ============================================================================

void AMosesMatchGameMode::DumpPlayerStates(const TCHAR* Prefix) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("%s PlayerArray Num=%d"), Prefix, GS->PlayerArray.Num());
}

void AMosesMatchGameMode::DumpAllDODPlayerStates(const TCHAR* Where) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		return;
	}

	for (APlayerState* RawPS : GS->PlayerArray)
	{
		if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(RawPS))
		{
			PS->DOD_PS_Log(this, Where);
		}
	}
}

void AMosesMatchGameMode::CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const
{
	OutStarts.Reset();

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Found);

	for (AActor* A : Found)
	{
		if (APlayerStart* PS = Cast<APlayerStart>(A))
		{
			OutStarts.Add(PS);
		}
	}
}

void AMosesMatchGameMode::FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const
{
	OutFree.Reset();

	for (APlayerStart* PS : InAll)
	{
		if (!PS)
		{
			continue;
		}

		if (ReservedPlayerStarts.Contains(PS))
		{
			continue;
		}

		OutFree.Add(PS);
	}
}

void AMosesMatchGameMode::ReserveStartForController(AController* Player, APlayerStart* Start)
{
	if (!Player || !Start)
	{
		return;
	}

	ReservedPlayerStarts.Add(Start);
	AssignedStartByController.Add(Player, Start);
}

void AMosesMatchGameMode::ReleaseReservedStart(AController* Player)
{
	if (!Player)
	{
		return;
	}

	TWeakObjectPtr<APlayerStart>* FoundStart = AssignedStartByController.Find(Player);
	if (!FoundStart)
	{
		return;
	}

	if (FoundStart->IsValid())
	{
		ReservedPlayerStarts.Remove(*FoundStart);
	}

	AssignedStartByController.Remove(Player);
}

void AMosesMatchGameMode::DumpReservedStarts(const TCHAR* Where) const
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[StartPick][Dump][%s] Reserved=%d Assigned=%d"),
		Where,
		ReservedPlayerStarts.Num(),
		AssignedStartByController.Num());
}

// ============================================================================
// Experience Ready Poll (Tick 금지)
// ============================================================================

void AMosesMatchGameMode::StartWarmup_WhenExperienceReady()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Phase", TEXT("Client attempted StartWarmup_WhenExperienceReady"));

	if (ExperienceReadyPollHandle.IsValid())
	{
		return;
	}

	ExperienceReadyPollCount = 0;

	GetWorldTimerManager().SetTimer(
		ExperienceReadyPollHandle,
		this,
		&ThisClass::PollExperienceReady_AndStartWarmup,
		ExperienceReadyPollInterval,
		true);

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM] Experience READY Poll START Interval=%.2f"),
		MOSES_TAG_COMBAT_SV,
		ExperienceReadyPollInterval);
}

void AMosesMatchGameMode::PollExperienceReady_AndStartWarmup()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Phase", TEXT("Client attempted PollExperienceReady_AndStartWarmup"));

	ExperienceReadyPollCount++;

	UMosesExperienceManagerComponent* ExpMgr = GetExperienceManager();
	if (!ExpMgr)
	{
		if (ExperienceReadyPollCount >= ExperienceReadyPollMaxCount)
		{
			GetWorldTimerManager().ClearTimer(ExperienceReadyPollHandle);
			UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM] Poll GIVEUP (No ExpMgr)"),
				MOSES_TAG_COMBAT_SV);
		}
		return;
	}

	const UMosesExperienceDefinition* CurrentExp = ExpMgr->GetCurrentExperienceOrNull();
	if (!CurrentExp)
	{
		if (ExperienceReadyPollCount >= ExperienceReadyPollMaxCount)
		{
			GetWorldTimerManager().ClearTimer(ExperienceReadyPollHandle);
			UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM] Poll GIVEUP (Experience not ready)"),
				MOSES_TAG_COMBAT_SV);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(ExperienceReadyPollHandle);

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM] Experience READY -> StartMatchFlow Exp=%s"),
		MOSES_TAG_COMBAT_SV,
		*GetNameSafe(CurrentExp));

	StartMatchFlow_AfterExperienceReady();
}

void AMosesMatchGameMode::ServerSaveRecord_Once_OnEnterResult()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bRecordSavedThisMatch)
	{
		return;
	}

	AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
	if (!MGS)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save SKIP (NoMatchGameState)"));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save FAIL (NoGameInstance)"));
		return;
	}

	UMosesMatchRecordStorageSubsystem* Storage = GI->GetSubsystem<UMosesMatchRecordStorageSubsystem>();
	if (!Storage)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save FAIL (NoStorageSubsystem)"));
		return;
	}

	bRecordSavedThisMatch = true;

	const bool bOk = Storage->SaveMatchRecord_OnResult_Server(MGS);
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] SaveOnce Result=%s"), bOk ? TEXT("OK") : TEXT("FAIL"));
}