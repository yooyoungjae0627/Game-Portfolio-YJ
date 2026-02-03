#include "MosesStartGameMode.h"
#include "UE5_Multi_Shooter/MosesPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

AMosesStartGameMode::AMosesStartGameMode()
{
	// Start 단계는 굳이 캐릭터 스폰 필요 없음 → 가벼운 Spectator
	DefaultPawnClass = ASpectatorPawn::StaticClass();

	PlayerControllerClass = AMosesPlayerController::StaticClass();

	// ✅ Start -> Lobby 이동도 Seamless로 강제
	bUseSeamlessTravel = true;
}

void AMosesStartGameMode::ServerTravelToLobby()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bIsDedicated = (World->GetNetMode() == NM_DedicatedServer);

	// ✅ 핵심: game= 을 로비 게임모드로 명시해서 StartGameMode가 딸려가지 않게 함
	const TCHAR* BaseURL = bIsDedicated
		? TEXT("/Game/Map/LobbyLevel?game=/Script/UE5_Multi_Shooter.MosesLobbyGameMode?Experience=Exp_Lobby")
		: TEXT("/Game/Map/LobbyLevel?listen?game=/Script/UE5_Multi_Shooter.MosesLobbyGameMode?Experience=Exp_Lobby");

	World->ServerTravel(BaseURL, /*bAbsolute*/ true);
}
