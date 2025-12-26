#include "MosesLobbyGameMode.h"

#include "MosesGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	// SeamlessTravel 권장(로비/매치 둘 다 켜두는 게 디버깅/왕복에서 안정적)
	bUseSeamlessTravel = true;
}

void AMosesLobbyGameMode::HandleStartGameRequest(AMosesPlayerController* RequestPC)
{
}

void AMosesLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	FString FinalOptions = Options;
	if (UGameplayStatics::HasOption(Options, TEXT("Experience")) == false)
	{
		FinalOptions += TEXT("?Experience=Exp_Lobby");
	}

	Super::InitGame(MapName, FinalOptions, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[LobbyGM][InitGame] Map=%s Options=%s"), *MapName, *FinalOptions);
}

void AMosesLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMosesExp, Warning, TEXT("[DOD][Travel] LobbyGM Seamless=%d"), bUseSeamlessTravel ? 1 : 0);

	// ✅ G2: 복귀 후 로비에서도 자동으로 찍히게
	DumpAllDODPlayerStates(TEXT("LobbyGM:BeginPlay_AfterReturn"));

	// 기존 덤프(PS 포인터/이름만)
	DumpPlayerStates(TEXT("[LobbyGM][BeginPlay]"));
}

void AMosesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// ✅ G3: 복귀 후 정책 적용(Ready 초기화) + 로그 증명
	ApplyReturnPolicy(NewPlayer);
}

void AMosesLobbyGameMode::ApplyReturnPolicy(APlayerController* PC)
{
	if (!PC) return;

	if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
	{
		// 정책(추천): 로비(복귀 포함)에서는 Ready를 0으로 초기화
		PS->bReady = false;

		// SelectedCharacterId 유지 정책(편의). 리셋 원하면 아래 주석 해제
		// PS->SelectedCharacterId = 0;

		PS->DOD_PS_Log(this, TEXT("Lobby:Policy_ResetReadyOnReturn"));
	}
}

int32 AMosesLobbyGameMode::GetPlayerCount() const
{
	return int32();
}

int32 AMosesLobbyGameMode::GetReadyCount() const
{
	return int32();
}

bool AMosesLobbyGameMode::CanDoServerTravel() const
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

FString AMosesLobbyGameMode::GetMatchMapURL() const
{
	// 정책 고정: Dedicated Server가 이미 떠 있으므로 일반적으로 ?listen 불필요
	return TEXT("/Game/Map/L_Match");
}

void AMosesLobbyGameMode::DumpPlayerStates(const TCHAR* Prefix) const
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
		UE_LOG(LogMosesSpawn, Warning, TEXT("%s PS=%p Name=%s PlayerId=%d PlayerName=%s"),
			Prefix,
			PS,
			*GetNameSafe(PS),
			PS ? PS->GetPlayerId() : -1,
			PS ? *PS->GetPlayerName() : TEXT("None"));
	}
}

void AMosesLobbyGameMode::DumpAllDODPlayerStates(const TCHAR* Where) const
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

void AMosesLobbyGameMode::TravelToMatch()
{
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch BLOCKED"));
		return;
	}

	// ✅ F2(1) 필수: Travel 직전 DoD Dump
	DumpAllDODPlayerStates(TEXT("Lobby:BeforeTravelToMatch"));

	// 기존 덤프도 유지(원하면 빼도 됨)
	DumpPlayerStates(TEXT("[LobbyGM][BeforeTravelToMatch]"));

	const FString URL = GetMatchMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s"), *URL);

	GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
}

void AMosesLobbyGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] GetSeamlessTravelActorList bToTransition=%d ActorListNum=%d"),
		bToTransition, ActorList.Num());
}

void AMosesLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
		{
			// ✅ (선택) SeamlessTravel 인계 과정에서의 고정 포맷 1줄 로그
			PS->DOD_PS_Log(this, TEXT("GM:HandleSeamlessTravelPlayer"));
		}
	}
}
