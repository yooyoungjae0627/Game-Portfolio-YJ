// MosesPlayerController.cpp
#include "MosesPlayerController.h"

#include "UE5_Multi_Shooter/GameMode/MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/GameMode/MosesMatchGameMode.h"

#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();
}

void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[PC] BeginPlay PC=%p PS=%s"),
		this,
		PS ? *PS->MakeDebugString() : TEXT("PS=None"));
}

void AMosesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[PC] OnPossess PC=%p Pawn=%s PS=%s"),
		this,
		*GetNameSafe(InPawn),
		PS ? *PS->MakeDebugString() : TEXT("PS=None"));
}

// --------------------
// Existing Server RPCs
// --------------------
void AMosesPlayerController::Server_SetReady_Implementation(bool bInReady)
{
	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->ServerSetReady(bInReady);
		UE_LOG(LogMosesSpawn, Warning, TEXT("[PC->Server] SetReady=%d %s"), bInReady ? 1 : 0, *PS->MakeDebugString());
	}
}

void AMosesPlayerController::Server_SetSelectedCharacterId_Implementation(int32 InId)
{
	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->ServerSetSelectedCharacterId(InId);
		UE_LOG(LogMosesSpawn, Warning, TEXT("[PC->Server] SetChar=%d %s"), InId, *PS->MakeDebugString());
	}
}

void AMosesPlayerController::YJ_SetReady(int32 InReady01)
{
	Server_SetReady(InReady01 != 0);
}

void AMosesPlayerController::YJ_SetChar(int32 InId)
{
	Server_SetSelectedCharacterId(InId);
}

// --------------------
// Travel (Exec -> Server RPC -> GM)
// --------------------
void AMosesPlayerController::TravelToMatch_Exec()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	// 서버에서 직접 호출 가능한 경우(리슨서버 PC 등)
	if (HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
		DoServerTravelToMatch();
		return;
	}

	// 클라에서 입력 -> 서버 RPC로 전달
	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] SEND TO SERVER (Client)"));
	Server_TravelToMatch();
}

void AMosesPlayerController::TravelToLobby_Exec()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	if (HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
		DoServerTravelToLobby();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] SEND TO SERVER (Client)"));
	Server_TravelToLobby();
}

void AMosesPlayerController::Server_TravelToMatch_Implementation()
{
	// 서버에서만 실행되어야 함
	if (!HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoAuthority)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
	DoServerTravelToMatch();
}

void AMosesPlayerController::Server_TravelToLobby_Implementation()
{
	if (!HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoAuthority)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
	DoServerTravelToLobby();
}

void AMosesPlayerController::DoServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoGameMode)"));
		return;
	}

	if (AMosesLobbyGameMode* LobbyGM = Cast<AMosesLobbyGameMode>(GM))
	{
		LobbyGM->TravelToMatch();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NotLobbyGM) GM=%s"), *GetNameSafe(GM));
}

void AMosesPlayerController::DoServerTravelToLobby()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoGameMode)"));
		return;
	}

	if (AMosesMatchGameMode* MatchGM = Cast<AMosesMatchGameMode>(GM))
	{
		MatchGM->TravelToLobby();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NotMatchGM) GM=%s"), *GetNameSafe(GM));
}
