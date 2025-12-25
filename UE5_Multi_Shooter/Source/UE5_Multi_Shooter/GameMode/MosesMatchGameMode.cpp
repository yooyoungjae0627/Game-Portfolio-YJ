#include "MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"

AMosesMatchGameMode::AMosesMatchGameMode()
{
	// 왕복 Travel + PS/PC 유지 검증을 위해 권장
	bUseSeamlessTravel = true;
}

void AMosesMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	/**
	 * 매치는 Exp_Match 강제(옵션에 없을 때만)
	 * - Base는 OptionsString을 읽어서 Experience를 선택한다.
	 * - 여기서 강제하면 “룰/모드 분리”가 더 명확해짐.
	 */
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

	DumpPlayerStates(TEXT("[MatchGM][BeginPlay]"));

	// (선택) 매치 시작 N초 후 자동 로비 복귀 테스트
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

	// ✅ UniqueId.ToString()는 OnlineSubsystem 의존성 문제로 빼자(너 프로젝트 빌드 깨짐 방지)
	APlayerState* PS = NewPlayer ? NewPlayer->PlayerState : nullptr;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[MatchGM][PostLogin] PC=%s PS=%p PSName=%s PlayerId=%d PlayerName=%s"),
		*GetNameSafe(NewPlayer),
		PS,
		*GetNameSafe(PS),
		PS ? PS->GetPlayerId() : -1,
		PS ? *PS->GetPlayerName() : TEXT("None"));
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

void AMosesMatchGameMode::TravelToLobby()
{
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] TravelToLobby BLOCKED"));
		return;
	}

	DumpPlayerStates(TEXT("[MatchGM][BeforeTravelToLobby]"));

	const FString URL = GetLobbyMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] ServerTravel -> %s"), *URL);

	GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
}

void AMosesMatchGameMode::HandleAutoReturn()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[MatchGM] HandleAutoReturn -> TravelToLobby"));
	TravelToLobby();
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
	APlayerState* PS = PC ? PC->PlayerState : nullptr;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[MatchGM] HandleSeamlessTravelPlayer PC=%p PS=%p PSName=%s PlayerId=%d PlayerName=%s"),
		PC,
		PS,
		*GetNameSafe(PS),
		PS ? PS->GetPlayerId() : -1,
		PS ? *PS->GetPlayerName() : TEXT("None"));
}
