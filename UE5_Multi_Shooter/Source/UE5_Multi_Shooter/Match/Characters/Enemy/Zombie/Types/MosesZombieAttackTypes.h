// MosesZombieAttackTypes.h (NEW)
// - Shared enums/types for Zombie combat
#pragma once

#include "CoreMinimal.h"
#include "MosesZombieAttackTypes.generated.h"

// [MOD] enum은 "단 한 곳"에만 UENUM으로 선언한다.
UENUM(BlueprintType)
enum class EMosesZombieAttackHand : uint8
{
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right"),
	Both	UMETA(DisplayName = "Both"),
};
