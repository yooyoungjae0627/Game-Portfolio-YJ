
#include "MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

AMosesMatchGameMode::AMosesMatchGameMode()
{
	bUseSeamlessTravel = true;

	PlayerStateClass = AMosesPlayerState::StaticClass();
	PlayerControllerClass = AMosesPlayerController::StaticClass();

	// [MOD] DefaultPawnClass를 하드 고정하지 않고,
	//       GetDefaultPawnClassForController(SelectedId->Catalog) + SpawnDefaultPawnFor 오버라이드로 결정한다.
	DefaultPawnClass = nullptr;
	FallbackPawnClass = nullptr;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] DefaultPawnClass is NOT hard-fixed. Using SpawnDefaultPawnFor override."));
}

// =========================================================
// Engine
// =========================================================

void AMosesMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// [유지] Match 레벨은 옵션에 Experience가 없으면 Exp_Match로 강제
	FString FinalOptions = Options;
	if (!UGameplayStatics::HasOption(Options, TEXT("Experience")))
	{
		FinalOptions += TEXT("?Experience=Exp_Match");
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

	// [MOD] 여기서 수동 Spawn/Possess를 하지 않는다.
	// 이유:
	// - Base(GameModeBase)가 Experience READY 후 SpawnGate를 해제하면서
	//   Super::HandleStartingNewPlayer → RestartPlayer → SpawnDefaultPawnAtTransform 흐름으로
	//   "정석 스폰 파이프라인"을 이미 보장한다.
	//
	// 즉, BeginPlay/PostLogin에서 직접 Spawn하면 "중복 스폰" 위험이 생긴다.

	// (선택) 자동 로비 복귀 테스트 타이머는 기존 유지
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

	// [MOD] 수동 Spawn/Possess 제거
	// - Experience READY 전에는 SpawnGate가 막고 있음
	// - READY 후에는 FlushPendingPlayers에서 정석 스폰됨
}

void AMosesMatchGameMode::Logout(AController* Exiting)
{
	ReleaseReservedStart(Exiting);
	Super::Logout(Exiting);
}

// =========================================================
// Experience READY Hook (Server authoritative match flow)
// =========================================================

void AMosesMatchGameMode::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	Super::HandleDoD_AfterExperienceReady(CurrentExperience);

	// [MOD] ExperienceDefinition 타입이 UObject가 아닐 수도 있으므로 이름 출력 생략(빌드 안정 우선)
	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][DOD] AfterExperienceReady -> StartMatchFlow"));


	// [ADD] Experience READY가 된 안전 시점에서만 매치 흐름을 시작한다.
	StartMatchFlow_AfterExperienceReady();
}

void AMosesMatchGameMode::StartMatchFlow_AfterExperienceReady()
{
	if (!HasAuthority())
	{
		return;
	}

	// [ADD] 혹시 SeamlessTravel/특수 케이스로 Pawn이 없는 PC가 있으면 RestartPlayer로 보정
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		// Pawn이 없으면 서버 권위로 다시 스폰(베이스 GM이 READY를 이미 보장한 상태)
		if (!IsValid(PC->GetPawn()))
		{
			UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM][READY] RestartPlayer (NoPawn) PC=%s"), *GetNameSafe(PC));
			RestartPlayer(PC);
		}
	}

	// [ADD] 페이즈 시작: Warmup
	SetMatchPhase(EMosesMatchPhase::Warmup);
}

void AMosesMatchGameMode::SetMatchPhase(EMosesMatchPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	// 같은 페이즈 중복 진입 방지
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	// [ADD] 타이머 초기화
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	const float Duration = GetPhaseDurationSeconds(CurrentPhase);
	PhaseEndTimeSeconds = (Duration > 0.f) ? (GetWorld()->GetTimeSeconds() + Duration) : 0.0;

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][PHASE] -> %s (Duration=%.2f EndTime=%.2f)"),
		*UEnum::GetValueAsString(CurrentPhase),
		Duration,
		PhaseEndTimeSeconds);

	// [ADD] 엔진 MatchState와의 연결(원하면 더 엄격히 분리해도 됨)
	// - Warmup: 아직 StartMatch() 안 함
	// - Combat: StartMatch()
	// - Result: EndMatch()
// [MOD] AGameModeBase에는 StartMatch/EndMatch가 없으므로 로그만 남긴다.
	if (CurrentPhase == EMosesMatchPhase::Combat)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][PHASE] Combat START"));
	}
	else if (CurrentPhase == EMosesMatchPhase::Result)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[MatchGM][PHASE] Result START"));
	}


	// [확장 포인트]
	// - "클라 HUD 표시"는 GameState에 Replicated Phase/RemainingTime을 내려주는 방식이 정석
	// - 예: AMosesGameState에 SetMatchPhase(CurrentPhase, PhaseEndTimeSeconds) 같은 API 추가

	// [ADD] 타이머 예약(WaitingForPlayers는 타이머 없음)
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
		// [ADD] 결과 페이즈 종료 = 로비 복귀
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
	case EMosesMatchPhase::Combat:  return CombatSeconds;
	case EMosesMatchPhase::Result:  return ResultSeconds;
	default: break;
	}

	return 0.f;
}

// =========================================================
// Spawning (duplicate prevention)
// =========================================================

AActor* AMosesMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!HasAuthority() || !Player)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// 이미 할당된 Start가 있으면 그대로 재사용(중복 방지)
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

// =========================================================
// Travel
// =========================================================

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
	// [유지] 필요시 "?listen?Experience=Exp_Lobby" 같은 옵션을 추가해도 됨
	return TEXT("/Game/Map/L_Lobby");
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

	// [MOD] SeamlessTravel 직후 "Pawn이 없는 경우"만 보정
	// - 중복 Spawn을 막기 위해 조건적으로만 RestartPlayer
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
	// 서버만 스폰
	if (!HasAuthority() || !NewPlayer || !StartSpot)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	// ✅ 핵심: "선택된 캐릭터 PawnClass"로 스폰한다.
	TSubclassOf<APawn> PawnClassToSpawn = GetDefaultPawnClassForController(NewPlayer);

	if (!PawnClassToSpawn)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] SpawnDefaultPawnFor fallback -> Super (No PawnClass resolved)"));
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	const FTransform SpawnTM = StartSpot->GetActorTransform();

	FActorSpawnParameters Params;
	Params.Owner = NewPlayer;
	Params.Instigator = Cast<APawn>(NewPlayer->GetPawn());
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

	// [유지/주의] 지금은 1~2만 존재한다고 가정해서 Clamp
	// - 캐릭터가 늘어나면 Catalog 크기에 맞춰 Clamp 범위를 바꿔야 한다.
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

// =========================================================
// Debug dumps
// =========================================================

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

// =========================================================
// PlayerStart helpers
// =========================================================

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
