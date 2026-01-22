#include "UE5_Multi_Shooter/GameMode/GameMode/MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

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

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] DefaultPawnClass not fixed. Using GetDefaultPawnClassForController + SpawnDefaultPawnFor."));
}

void AMosesMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	FString FinalOptions = Options;

	if (!UGameplayStatics::HasOption(Options, TEXT("Experience")))
	{
		FinalOptions += TEXT("?Experience=Exp_Match_Warmup");
	}

	Super::InitGame(MapName, FinalOptions, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][InitGame] Map=%s Options=%s"), *MapName, *FinalOptions);
}

void AMosesMatchGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMosesExp, Warning, TEXT("[DOD][Travel] MatchGM Seamless=%d"), bUseSeamlessTravel ? 1 : 0);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] BeginPlay Catalog=%s Fallback=%s"),
		*GetNameSafe(CharacterCatalog),
		*GetNameSafe(FallbackPawnClass));

	DumpAllDODPlayerStates(TEXT("MatchGM:BeginPlay"));
	DumpPlayerStates(TEXT("[MatchGM][BeginPlay]"));

	// BeginPlay/PostLogin에서 수동 Spawn 금지.
	// Base GM이 Experience READY 이후 SpawnGate를 통해 정석 스폰 파이프라인을 보장한다.

	if (AutoReturnToLobbySeconds > 0.f && HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			AutoReturnTimerHandle,
			this,
			&ThisClass::HandleAutoReturn,
			AutoReturnToLobbySeconds,
			false);

		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] AutoReturnToLobby in %.2f sec"), AutoReturnToLobbySeconds);
	}
}

void AMosesMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	APlayerState* PS = NewPlayer ? NewPlayer->PlayerState : nullptr;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[MatchGM][PostLogin] PC=%s PS=%p PSName=%s PlayerId=%d PlayerName=%s"),
		*GetNameSafe(NewPlayer),
		PS,
		*GetNameSafe(PS),
		PS ? PS->GetPlayerId() : -1,
		PS ? *PS->GetPlayerName() : TEXT("None"));

	// Experience READY 전에는 SpawnGate가 막고 있음
	// READY 후에는 FlushPendingPlayers에서 정석 스폰됨
}

void AMosesMatchGameMode::Logout(AController* Exiting)
{
	ReleaseReservedStart(Exiting);
	Super::Logout(Exiting);
}

void AMosesMatchGameMode::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	Super::HandleDoD_AfterExperienceReady(CurrentExperience);

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][DOD] AfterExperienceReady -> StartMatchFlow"));
	StartMatchFlow_AfterExperienceReady();
}

void AMosesMatchGameMode::StartMatchFlow_AfterExperienceReady()
{
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		if (!IsValid(PC->GetPawn()))
		{
			UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM][READY] RestartPlayer (NoPawn) PC=%s"), *GetNameSafe(PC));
			RestartPlayer(PC);
		}
	}

	SetMatchPhase(EMosesMatchPhase::Warmup);
}

void AMosesMatchGameMode::HandlePhaseTimerExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][PHASE] TimerExpired Phase=%s"), *UEnum::GetValueAsString(CurrentPhase));
	AdvancePhase();
}

void AMosesMatchGameMode::AdvancePhase()
{
	if (!HasAuthority())
	{
		return;
	}

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
		UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][PHASE] ResultFinished -> TravelToLobby"));
		TravelToLobby();
		break;
	}
}

float AMosesMatchGameMode::GetPhaseDurationSeconds(EMosesMatchPhase Phase) const
{
	switch (Phase)
	{
	case EMosesMatchPhase::Warmup: return WarmupSeconds;
	case EMosesMatchPhase::Combat: return CombatSeconds;
	case EMosesMatchPhase::Result: return ResultSeconds;
	default: break;
	}
	return 0.f;
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
			APlayerStart* Start = Assigned->Get();
			UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Reuse] PC=%s Start=%s Loc=%s Rot=%s"),
				*GetNameSafe(Player),
				*GetNameSafe(Start),
				*Start->GetActorLocation().ToString(),
				*Start->GetActorRotation().ToString());

			return Start;
		}
	}

	TArray<APlayerStart*> AllStarts;
	CollectMatchPlayerStarts(AllStarts);

	TArray<APlayerStart*> FreeStarts;
	FilterFreeStarts(AllStarts, FreeStarts);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick] PC=%s All=%d Free=%d Reserved=%d"),
		*GetNameSafe(Player),
		AllStarts.Num(),
		FreeStarts.Num(),
		ReservedPlayerStarts.Num());

	if (FreeStarts.Num() <= 0)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Fallback] No free Match PlayerStart. Use Super. PC=%s"),
			*GetNameSafe(Player));

		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const int32 Index = FMath::RandRange(0, FreeStarts.Num() - 1);
	APlayerStart* Chosen = FreeStarts[Index];

	ReserveStartForController(Player, Chosen);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Chosen] PC=%s Start=%s Loc=%s Rot=%s (Reserved=%d)"),
		*GetNameSafe(Player),
		*GetNameSafe(Chosen),
		*Chosen->GetActorLocation().ToString(),
		*Chosen->GetActorRotation().ToString(),
		ReservedPlayerStarts.Num());

	return Chosen;
}

void AMosesMatchGameMode::TravelToLobby()
{
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] TravelToLobby BLOCKED"));
		return;
	}

	DumpAllDODPlayerStates(TEXT("Match:BeforeTravelToLobby"));
	DumpPlayerStates(TEXT("[MatchGM][BeforeTravelToLobby]"));
	DumpReservedStarts(TEXT("BeforeTravelToLobby"));

	const FString URL = GetLobbyMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] ServerTravel -> %s"), *URL);

	GetWorld()->ServerTravel(URL, false);
}

void AMosesMatchGameMode::HandleAutoReturn()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] HandleAutoReturn -> TravelToLobby"));
	TravelToLobby();
}

bool AMosesMatchGameMode::CanDoServerTravel() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return false;
	}

	const ENetMode NM = World->GetNetMode();
	if (NM == NM_Client)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (Client)"));
		return false;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoAuthority)"));
		return false;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
	return true;
}

FString AMosesMatchGameMode::GetLobbyMapURL() const
{
	return TEXT("/Game/Map/L_Lobby?Experience=Exp_Lobby");
}

void AMosesMatchGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[MatchGM] GetSeamlessTravelActorList bToTransition=%d ActorListNum=%d"),
		bToTransition, ActorList.Num());
}

void AMosesMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (!PC || !HasAuthority())
	{
		return;
	}

	if (!IsValid(PC->GetPawn()))
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM][Seamless] RestartPlayer (NoPawn) PC=%s"), *GetNameSafe(PC));
		RestartPlayer(PC);
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
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] SpawnDefaultPawnFor fallback -> Super (No PawnClass resolved)"));
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	const FTransform SpawnTM = StartSpot->GetActorTransform();

	FActorSpawnParameters Params;
	Params.Owner = NewPlayer;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClassToSpawn, SpawnTM, Params);
	if (!NewPawn)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[MatchGM] SpawnActor FAILED PawnClass=%s"), *GetNameSafe(PawnClassToSpawn));
		return nullptr;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] Spawned Pawn=%s Class=%s Start=%s"),
		*GetNameSafe(NewPawn),
		*GetNameSafe(PawnClassToSpawn),
		*GetNameSafe(StartSpot));

	return NewPawn;
}

UClass* AMosesMatchGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const AMosesPlayerState* PS = InController ? InController->GetPlayerState<AMosesPlayerState>() : nullptr;
	const int32 SelectedId = PS ? PS->GetSelectedCharacterId() : 0;

	if (UClass* Resolved = ResolvePawnClassFromSelectedId(SelectedId))
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[MatchGM] ResolvePawnClass OK Controller=%s SelectedId=%d PawnClass=%s"),
			*GetNameSafe(InController),
			SelectedId,
			*GetNameSafe(Resolved));
		return Resolved;
	}

	if (FallbackPawnClass)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] ResolvePawnClass FAIL -> Fallback. Controller=%s SelectedId=%d Fallback=%s"),
			*GetNameSafe(InController),
			SelectedId,
			*GetNameSafe(FallbackPawnClass));
		return FallbackPawnClass.Get();
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] ResolvePawnClass FAIL -> Super. Controller=%s SelectedId=%d"),
		*GetNameSafe(InController),
		SelectedId);

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

UClass* AMosesMatchGameMode::ResolvePawnClassFromSelectedId(int32 SelectedId) const
{
	if (!CharacterCatalog)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[MatchGM] CharacterCatalog is NULL."));
		return nullptr;
	}

	// 현재는 1~2만 존재한다고 가정(필요시 확장)
	const int32 SafeSelectedId = FMath::Clamp(SelectedId, 1, 2);

	FMSCharacterEntry Entry;
	if (!CharacterCatalog->FindByIndex(SafeSelectedId, Entry))
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] Invalid SelectedId=%d (Catalog FindByIndex failed)"), SafeSelectedId);
		return nullptr;
	}

	UClass* PawnClass = Entry.PawnClass.LoadSynchronous();
	if (!PawnClass)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[MatchGM] PawnClass Load FAIL. SelectedId=%d CharacterId=%s"),
			SafeSelectedId,
			*Entry.CharacterId.ToString());
		return nullptr;
	}

	return PawnClass;
}

void AMosesMatchGameMode::DumpPlayerStates(const TCHAR* Prefix) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("%s GameState=None"), Prefix);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("%s PlayerArray Num=%d"), Prefix, GS->PlayerArray.Num());

	for (APlayerState* PS : GS->PlayerArray)
	{
		UE_LOG(LogMosesSpawn, Warning,
			TEXT("%s PS=%p Name=%s PlayerId=%d PlayerName=%s"),
			Prefix,
			PS,
			*GetNameSafe(PS),
			PS ? PS->GetPlayerId() : -1,
			PS ? *PS->GetPlayerName() : TEXT("None"));
	}
}

void AMosesMatchGameMode::DumpAllDODPlayerStates(const TCHAR* Where) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] GameState=None"), Where);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] ---- DumpAllPS Begin ----"), Where);

	for (APlayerState* RawPS : GS->PlayerArray)
	{
		if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(RawPS))
		{
			PS->DOD_PS_Log(this, Where);
		}
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] ---- DumpAllPS End ----"), Where);
}

void AMosesMatchGameMode::CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const
{
	OutStarts.Reset();

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Found);

	OutStarts.Reserve(Found.Num());

	for (AActor* A : Found)
	{
		if (APlayerStart* PS = Cast<APlayerStart>(A))
		{
			OutStarts.Add(PS);
		}
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick] Collect AllPlayerStarts=%d"), OutStarts.Num());
}

void AMosesMatchGameMode::FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const
{
	OutFree.Reset();
	OutFree.Reserve(InAll.Num());

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

	UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Release] PC=%s Reserved=%d"),
		*GetNameSafe(Player),
		ReservedPlayerStarts.Num());
}

void AMosesMatchGameMode::DumpReservedStarts(const TCHAR* Where) const
{
	int32 ValidCount = 0;
	for (const TWeakObjectPtr<APlayerStart>& It : ReservedPlayerStarts)
	{
		if (It.IsValid())
		{
			ValidCount++;
		}
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Dump][%s] Reserved=%d Valid=%d Assigned=%d"),
		Where,
		ReservedPlayerStarts.Num(),
		ValidCount,
		AssignedStartByController.Num());

	for (const TPair<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>>& Pair : AssignedStartByController)
	{
		AController* PC = Pair.Key.Get();
		APlayerStart* PS = Pair.Value.Get();

		UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Dump][%s] PC=%s -> Start=%s"),
			Where,
			*GetNameSafe(PC),
			*GetNameSafe(PS));
	}
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
	if (!HasAuthority())
	{
		return;
	}

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

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][EXP] Switch -> %s (Phase=%s)"),
		*NewExperienceId.ToString(),
		*UEnum::GetValueAsString(Phase));

	ExperienceManagerComponent->ServerSetCurrentExperience(NewExperienceId);
}

void AMosesMatchGameMode::SetMatchPhase(EMosesMatchPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	// Phase 진입 시 Experience 교체
	ServerSwitchExperienceByPhase(CurrentPhase);

	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	const float Duration = GetPhaseDurationSeconds(CurrentPhase);
	PhaseEndTimeSeconds = (Duration > 0.f) ? (GetWorld()->GetTimeSeconds() + Duration) : 0.0;

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][PHASE] -> %s (Duration=%.2f EndTime=%.2f)"),
		*UEnum::GetValueAsString(CurrentPhase),
		Duration,
		PhaseEndTimeSeconds);

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
