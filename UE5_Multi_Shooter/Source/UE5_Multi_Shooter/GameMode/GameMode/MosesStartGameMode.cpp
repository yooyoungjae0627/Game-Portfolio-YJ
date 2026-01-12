#include "MosesStartGameMode.h"
#include "GameFramework/SpectatorPawn.h"

AMosesStartGameMode::AMosesStartGameMode()
{
	// Start 단계는 굳이 캐릭터 스폰 필요 없음 → 가벼운 Spectator
	DefaultPawnClass = ASpectatorPawn::StaticClass();

	// 원하면 아예 스폰 안 하게 막고 싶으면 Spectator 대신 아래 정책으로 갈 수도 있음.
	// bStartPlayersAsSpectators = true;
}
