#pragma once

#include "CoreMinimal.h"
#include "MosesMatchPhase.generated.h"

/**
 * EMosesMatchPhase
 *
 * - 매치 내부 Phase.
 * - GameMode / GameState / UI에서 공통으로 사용되므로 별도 헤더로 분리한다.
 * - include 순환(1024) 방지 목적.
 */
UENUM(BlueprintType)
enum class EMosesMatchPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "WaitingForPlayers"),
	Warmup            UMETA(DisplayName = "Warmup"),
	Combat            UMETA(DisplayName = "Combat"),
	Result            UMETA(DisplayName = "Result"),
};
