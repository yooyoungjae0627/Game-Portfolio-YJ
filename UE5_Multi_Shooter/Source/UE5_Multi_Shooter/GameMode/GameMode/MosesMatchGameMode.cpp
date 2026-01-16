#include "MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h"
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

	// ✅ 매치에서도 PS/PC 클래스를 강제로 고정
	PlayerStateClass = AMosesPlayerState::StaticClass();
	PlayerControllerClass = AMosesPlayerController::StaticClass();

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

	// ✅ F2(2) 필수: MatchGM BeginPlay Dump
	DumpAllDODPlayerStates(TEXT("MatchGM:BeginPlay"));
	DumpPlayerStates(TEXT("[MatchGM][BeginPlay]"));

	UE_LOG(LogMosesSpawn, Warning, TEXT("[TEST] MatchGM BeginPlay World=%s"), *GetNameSafe(GetWorld()));

	// (선택) 자동 복귀
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

	// ✅ Travel로 들어온 경우, BeginPlay 시점에 이미 PC가 존재할 수 있다.
	// 이때도 Pawn이 없으면 붙여준다.
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

	// ✅ 여기서 Pawn/Possess 보장
	if (HasAuthority())
	{
		SpawnAndPossessMatchPawn(NewPlayer);
	}
}

void AMosesMatchGameMode::Logout(AController* Exiting)
{
	// ✅ 나갈 때 예약 해제 (중복 방지)
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

	// 0) 이미 배정된 Start가 있다면 그대로 유지(원하면 정책 변경 가능)
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

	// 1) MatchStart 수집(Tag=Match)
	TArray<APlayerStart*> AllStarts;
	CollectMatchPlayerStarts(AllStarts);

	// 2) 점유되지 않은 Start만 필터
	TArray<APlayerStart*> FreeStarts;
	FilterFreeStarts(AllStarts, FreeStarts);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick] PC=%s All=%d Free=%d Reserved=%d"),
		*GetNameSafe(Player),
		AllStarts.Num(),
		FreeStarts.Num(),
		ReservedPlayerStarts.Num());

	// 3) 후보가 없다면 fallback
	if (FreeStarts.Num() <= 0)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[StartPick][Fallback] No free Match PlayerStart. Use Super. PC=%s"),
			*GetNameSafe(Player));

		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// 4) 랜덤 선택 + 예약
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

	// ✅ 복귀 직전 DoD Dump
	DumpAllDODPlayerStates(TEXT("Match:BeforeTravelToLobby"));
	DumpPlayerStates(TEXT("[MatchGM][BeforeTravelToLobby]"));

	// 예약 상태도 확인(디버깅)
	DumpReservedStarts(TEXT("BeforeTravelToLobby"));

	const FString URL = GetLobbyMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] ServerTravel -> %s"), *URL);

	GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
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

// =========================================================
// Seamless Travel logs
// =========================================================

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

	// 2) PawnClass 결정: PawnData -> PawnClass (없으면 Super fallback)
	TSubclassOf<APawn> PawnClassToSpawn = nullptr;

	// ✅ 너 프로젝트 구조상 PawnData를 어디서 가져올지 1개로 고정해야 함.
	//    지금 가장 빠른 방법: GameMode에 UPROPERTY로 PawnData를 직접 넣는 것.
	if (MatchPawnData)
	{
		PawnClassToSpawn = MatchPawnData->PawnClass;
	}

	if (!PawnClassToSpawn)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] SpawnDefaultPawnFor fallback -> Super (No MatchPawnData or PawnClass)"));
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	// 3) 스폰 트랜스폼
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
// PlayerStart helper functions
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
			OutStarts.Add(PS); // ✅ Tag 조건 제거
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
	// 정책(최소):
	// - Pawn이 없으면 스폰 필요
	// - Pawn이 있는데 PendingKill/Destroyed면 스폰 필요
	// - Pawn이 있는데 Match용 캐릭터가 아니면(로비 Dummy 등) 스폰 필요
	if (!PC)
	{
		return false;
	}

	APawn* ExistingPawn = PC->GetPawn();
	if (!IsValid(ExistingPawn))
	{
		return true;
	}

	// 로비에서 DummyPawn을 쓴다면 여기서 타입 체크로 교체 가능
	// 예: if (!ExistingPawn->IsA(AMosesCharacter::StaticClass())) return true;

	return false;
}

void AMosesMatchGameMode::SpawnAndPossessMatchPawn(APlayerController* PC)
{
	if (!HasAuthority() || !PC)
	{
		return;
	}

	// 이미 유효한 Pawn이 있고 정책상 재스폰이 아니면 종료
	if (!ShouldRespawnPawn(PC))
	{
		UE_LOG(LogMosesSpawn, Verbose, TEXT("[MatchGM] SpawnAndPossess SKIP (AlreadyHasPawn) PC=%s Pawn=%s"),
			*GetNameSafe(PC), *GetNameSafe(PC->GetPawn()));
		return;
	}

	// 기존 Pawn이 있으면 정리(선택)
	if (APawn* OldPawn = PC->GetPawn())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] UnPossess old pawn PC=%s OldPawn=%s"),
			*GetNameSafe(PC), *GetNameSafe(OldPawn));

		PC->UnPossess();

		// 필요하면 Destroy까지 (프로젝트 정책에 따라)
		OldPawn->Destroy();
	}

	// ✅ PlayerStart 선택(네 ChoosePlayerStart_Implementation을 타게 됨)
	AActor* StartSpot = ChoosePlayerStart(PC);
	if (!StartSpot)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] Spawn FAIL (NoStartSpot) PC=%s"), *GetNameSafe(PC));
		return;
	}

	// ✅ 기본 Pawn 스폰(GameModeBase 내부 유틸 사용)
	APawn* NewPawn = SpawnDefaultPawnFor(PC, StartSpot);
	if (!NewPawn)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[MatchGM] Spawn FAIL (SpawnDefaultPawnFor returned null) PC=%s"),
			*GetNameSafe(PC));
		return;
	}

	// ✅ Possess (여기서 AMosesCharacter::PossessedBy() 호출됨)
	PC->Possess(NewPawn);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] Possess OK PC=%s Pawn=%s Start=%s"),
		*GetNameSafe(PC),
		*GetNameSafe(NewPawn),
		*GetNameSafe(StartSpot));
}
