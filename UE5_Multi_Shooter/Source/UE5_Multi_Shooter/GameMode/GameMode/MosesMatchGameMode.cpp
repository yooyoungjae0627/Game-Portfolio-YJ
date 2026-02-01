#include "UE5_Multi_Shooter/GameMode/GameMode/MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"

#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h" // Match GS

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

AMosesMatchGameMode::AMosesMatchGameMode()
{
	bUseSeamlessTravel = true;

	PlayerStateClass = AMosesPlayerState::StaticClass();
	PlayerControllerClass = AMosesPlayerController::StaticClass();

	DefaultPawnClass = nullptr;
	FallbackPawnClass = nullptr;

	// [중요] 매치 레벨에서는 MatchGameState를 사용한다.
	GameStateClass = AMosesMatchGameState::StaticClass();

	// [MOD] Warmup 30초 요구사항 강제 기본값(단, BP/ini가 덮어쓸 수 있으므로 BeginPlay에서도 한번 더 강제한다)
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

	// [MOD] BP/ini가 WarmupSeconds를 120으로 덮어썼을 가능성이 높다.
	//       요구사항은 "워밍업 30초"이므로 서버에서 강제 고정한다.
	if (HasAuthority())
	{
		const float Before = WarmupSeconds;
		WarmupSeconds = 30.0f;

		UE_LOG(LogMosesPhase, Warning,
			TEXT("[PHASE][SV] WarmupSeconds FORCE 30 (Before=%.2f After=%.2f) GM=%s"),
			Before, WarmupSeconds, *GetNameSafe(this));
	}

	UE_LOG(LogMosesExp, Warning, TEXT("%s [DOD][Travel] MatchGM Seamless=%d"),
		MOSES_TAG_COMBAT_SV, bUseSeamlessTravel ? 1 : 0);

	UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM] BeginPlay Catalog=%s Fallback=%s"),
		MOSES_TAG_COMBAT_SV, *GetNameSafe(CharacterCatalog), *GetNameSafe(FallbackPawnClass));

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

	// ---------------------------------------------------------------------
	// [MOD] MatchLevel 진입 기본 로드아웃 보장: Rifle + 30/90
	// - PostLogin에서 한 번 보장
	// - 실제 Spawn/Restart 경로에서도 다시 보장하지만,
	//   SSOT에서 idempotent 이므로 중복 호출은 안전하다.
	// ---------------------------------------------------------------------
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

	// Experience Ready 시점에 Pawn이 없는 플레이어는 서버가 Spawn 확정
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		// [MOD] MatchLevel 기본 로드아웃은 "Pawn 유무와 무관"하게 SSOT에 먼저 보장해도 된다.
		// (Pawn은 코스메틱이며, PS/CombatComponent가 단일 진실)
		Server_EnsureDefaultMatchLoadout(PC, TEXT("READY:BeforeRestart"));

		if (!IsValid(PC->GetPawn()))
		{
			UE_LOG(LogMosesSpawn, Warning, TEXT("%s [MatchGM][READY] RestartPlayer (NoPawn) PC=%s"),
				MOSES_TAG_RESPAWN_SV, *GetNameSafe(PC));
			RestartPlayer(PC);

			// [MOD] Restart 이후에도 한 번 더 보장(Spawn 이후 OnRep 순서/타이밍 이슈 차단)
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
		// [MOD] 요구사항: Warmup은 무조건 30초 (BP/ini/실수로 120이 들어와도 여기서 차단)
	case EMosesMatchPhase::Warmup: return 30.0f;

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

	UE_LOG(LogMosesPhase, Warning, TEXT("[PHASE][SV] SetMatchPhase CALLED New=%s World=%s GM=%s"),
		*UEnum::GetValueAsString(NewPhase),
		*GetNameSafe(GetWorld()),
		*GetNameSafe(this));

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
	PhaseEndTimeSeconds = (Duration > 0.f) ? (GetWorld()->GetTimeSeconds() + Duration) : 0.0;

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][PHASE] -> %s (Duration=%.2f EndTime=%.2f)"),
		MOSES_TAG_COMBAT_SV,
		*UEnum::GetValueAsString(CurrentPhase),
		Duration,
		PhaseEndTimeSeconds);

	// 4) MatchGameState에 Phase/Timer/Announcement 확정
	if (AMosesMatchGameState* GS = GetMatchGameState())
	{
		GS->ServerSetMatchPhase(CurrentPhase);

		const int32 DurationInt = (Duration > 0.f) ? FMath::CeilToInt(Duration) : 0;

		UE_LOG(LogMosesPhase, Warning, TEXT("[PHASE][SV] ApplyTimer Phase=%s DurationInt=%d (Warmup must be 30)"),
			*UEnum::GetValueAsString(CurrentPhase), DurationInt);

		// ✅ RemainingSeconds 감소는 MatchGameState의 서버 타이머가 담당한다(단일 진실).
		GS->ServerStartMatchTimer(DurationInt);

		// 방송(증거/데모) - [MOD] 한글/색상 정책은 HUD에서 처리, 여기선 "텍스트"만
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

void AMosesMatchGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);
}

void AMosesMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (!PC || !HasAuthority())
	{
		return;
	}

	// ---------------------------------------------------------------------
	// [MOD] SeamlessTravel로 넘어온 플레이어도 기본 로드아웃 보장
	// ---------------------------------------------------------------------
	Server_EnsureDefaultMatchLoadout(PC, TEXT("HandleSeamlessTravelPlayer"));

	if (!IsValid(PC->GetPawn()))
	{
		RestartPlayer(PC);
		// Restart 이후에도 보장
		Server_EnsureDefaultMatchLoadout(PC, TEXT("HandleSeamlessTravelPlayer:AfterRestart"));
	}

	if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
	{
		PS->DOD_PS_Log(this, TEXT("GM:HandleSeamlessTravelPlayer"));
	}
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

void AMosesMatchGameMode::StartWarmup_WhenExperienceReady()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Phase", TEXT("Client attempted StartWarmup_WhenExperienceReady"));

	// 중복 방지
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

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][MOD] StartWarmup_WhenExperienceReady -> Poll START Interval=%.2f"),
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
			UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][MOD] Poll GIVEUP (No ExpMgr)"),
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
			UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][MOD] Poll GIVEUP (Experience not ready)"),
				MOSES_TAG_COMBAT_SV);
		}
		return;
	}

	// Ready 확인됨 → 폴링 종료
	GetWorldTimerManager().ClearTimer(ExperienceReadyPollHandle);

	UE_LOG(LogMosesExp, Warning, TEXT("%s [MatchGM][MOD] Experience READY -> StartMatchFlow Exp=%s"),
		MOSES_TAG_COMBAT_SV,
		*GetNameSafe(CurrentExp));

	StartMatchFlow_AfterExperienceReady();
}

// ============================================================================
// [MOD] Match default loadout (Server only)
// - MatchLevel 진입 시점(여러 경로)에서 "Rifle + 30/90" 보장
// - 실제 지급은 SSOT(PlayerState)로 위임
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

	// ✅ 핵심: PS(SSOT)에서 “한 번만” 보장 (중복 호출 안전)
	PS->ServerEnsureMatchDefaultLoadout();

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV][MatchGM] EnsureDefaultLoadout OK From=%s PC=%s PS=%s"),
		FromWhere ? FromWhere : TEXT("None"),
		*GetNameSafe(PC),
		*GetNameSafe(PS));
}
