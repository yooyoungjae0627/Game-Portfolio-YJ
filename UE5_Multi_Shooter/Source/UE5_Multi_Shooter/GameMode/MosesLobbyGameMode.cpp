#include "MosesLobbyGameMode.h"

#include "MosesGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"


AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	// SeamlessTravel 권장(로비/매치 둘 다 켜두는 게 디버깅/왕복에서 안정적)
	bUseSeamlessTravel = true;
}

void AMosesLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	/**
	 * ✅ 여기서 로비 Experience를 강제하고 싶으면 Options에 경험치 파라미터를 추가해서 Super로 전달 가능.
	 * - 네 Base는 OptionsString에서 ?Experience= 를 읽고, 없으면 맵 이름으로 fallback도 함.
	 * - "항상 로비는 Exp_Lobby"를 강제하려면 아래처럼 박아두면 됨.
	 *
	 * 주의:
	 * - 이미 Options에 Experience가 들어올 수 있으니 중복 방지를 위해 "없을 때만" 붙이는 걸 추천.
	 */
	FString FinalOptions = Options;
	if (!UGameplayStatics::HasOption(Options, TEXT("Experience")))
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

	DumpPlayerStates(TEXT("[LobbyGM][BeginPlay]"));
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

	// 서버라도 혹시 권한이 이상하면 막기
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
	// 필요하면: return TEXT("/Game/Maps/L_Match?listen");
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

void AMosesLobbyGameMode::TravelToMatch()
{
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch BLOCKED"));
		return;
	}

	DumpPlayerStates(TEXT("[LobbyGM][BeforeTravelToMatch]"));

	const FString URL = GetMatchMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s"), *URL);

	GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
}

void AMosesLobbyGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	/**
	 * 기본적으로 PlayerController/PlayerState는 엔진에서 처리.
	 * 여기서는 추가 유지 Actor가 필요할 때만 넣는다.
	 */
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] GetSeamlessTravelActorList bToTransition=%d ActorListNum=%d"),
		bToTransition, ActorList.Num());
}

void AMosesLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	APlayerController* PC = Cast<APlayerController>(C);
	APlayerState* PS = PC ? PC->PlayerState : nullptr;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[LobbyGM] HandleSeamlessTravelPlayer PC=%p PS=%p PSName=%s PlayerId=%d PlayerName=%s"),
		PC,
		PS,
		*GetNameSafe(PS),
		PS ? PS->GetPlayerId() : -1,
		PS ? *PS->GetPlayerName() : TEXT("None"));
}
