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

	DefaultPawnClass = nullptr;
	FallbackPawnClass = nullptr;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] DefaultPawnClass is NOT hard-fixed. Using SpawnDefaultPawnFor override."));
}

// =========================================================
// Engine
// =========================================================

void AMosesMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
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

	UE_LOG(LogMosesSpawn, Warning, TEXT("[TEST] MatchGM BeginPlay World=%s"), *GetNameSafe(GetWorld()));

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

	if (HasAuthority())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				SpawnAndPossessMatchPawn(PC);
			}
		}
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

	if (HasAuthority())
	{
		SpawnAndPossessMatchPawn(NewPlayer);
	}
}

void AMosesMatchGameMode::Logout(AController* Exiting)
{
	ReleaseReservedStart(Exiting);
	Super::Logout(Exiting);
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
	const bool bIsClient = (NM == NM_Client);

	if (bIsClient)
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
	return TEXT("/Game/Map/L_Lobby");
}

void AMosesMatchGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[MatchGM] GetSeamlessTravelActorList bToTransition=%d ActorListNum=%d"),
		bToTransition, ActorList.Num());
}

APawn* AMosesMatchGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// 1) 기본 방어
	if (!HasAuthority() || !NewPlayer || !StartSpot)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	// [DAY2-MOD] ✅ 핵심: "선택된 캐릭터 PawnClass"로 스폰한다.
	TSubclassOf<APawn> PawnClassToSpawn = GetDefaultPawnClassForController(NewPlayer);

	if (!PawnClassToSpawn)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM][DAY2] SpawnDefaultPawnFor fallback -> Super (No PawnClass resolved)"));
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

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM][DAY2] Spawned Pawn=%s Class=%s Start=%s"),
		*GetNameSafe(NewPawn),
		*GetNameSafe(PawnClassToSpawn),
		*GetNameSafe(StartSpot));

	return NewPawn;
}

void AMosesMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (PC)
	{
		SpawnAndPossessMatchPawn(PC);

		if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
		{
			PS->DOD_PS_Log(this, TEXT("GM:HandleSeamlessTravelPlayer"));
		}
	}
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

bool AMosesMatchGameMode::ShouldRespawnPawn(APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}

	APawn* ExistingPawn = PC->GetPawn();
	if (!IsValid(ExistingPawn))
	{
		return true;
	}

	return false;
}

void AMosesMatchGameMode::SpawnAndPossessMatchPawn(APlayerController* PC)
{
	if (!HasAuthority() || !PC)
	{
		return;
	}

	if (!ShouldRespawnPawn(PC))
	{
		UE_LOG(LogMosesSpawn, Verbose, TEXT("[MatchGM] SpawnAndPossess SKIP (AlreadyHasPawn) PC=%s Pawn=%s"),
			*GetNameSafe(PC), *GetNameSafe(PC->GetPawn()));
		return;
	}

	if (APawn* OldPawn = PC->GetPawn())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] UnPossess old pawn PC=%s OldPawn=%s"),
			*GetNameSafe(PC), *GetNameSafe(OldPawn));

		PC->UnPossess();
		OldPawn->Destroy();
	}

	AActor* StartSpot = ChoosePlayerStart(PC);
	if (!StartSpot)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] Spawn FAIL (NoStartSpot) PC=%s"), *GetNameSafe(PC));
		return;
	}

	APawn* NewPawn = SpawnDefaultPawnFor(PC, StartSpot);
	if (!NewPawn)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[MatchGM] Spawn FAIL (SpawnDefaultPawnFor returned null) PC=%s"),
			*GetNameSafe(PC));
		return;
	}

	PC->Possess(NewPawn);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] Possess OK PC=%s Pawn=%s Start=%s"),
		*GetNameSafe(PC),
		*GetNameSafe(NewPawn),
		*GetNameSafe(StartSpot));
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

	// [ADD] 안전장치: 선택값이 0/쓰레기값이면 1로 고정
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

