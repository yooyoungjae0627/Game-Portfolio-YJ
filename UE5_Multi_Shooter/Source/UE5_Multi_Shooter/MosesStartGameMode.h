#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/MosesGameModeBase.h"
#include "MosesStartGameMode.generated.h"

/**
 * Start 전용 GM
 * - 로컬 UI(닉네임 입력) 단계
 * - 전투/로비 기능 최소화
 * - Pawn은 Spectator로 가볍게
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesStartGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesStartGameMode();
	virtual void ServerTravelToLobby() override;

};