#include "MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

// =========================================================
// AMosesMatchGameMode
// =========================================================

AMosesMatchGameMode::AMosesMatchGameMode()
{
	// 왕복 Travel + PS/PC 유지 검증을 위해 권장
	bUseSeamlessTravel = true;
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

void AMosesMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (PC)
	{
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
