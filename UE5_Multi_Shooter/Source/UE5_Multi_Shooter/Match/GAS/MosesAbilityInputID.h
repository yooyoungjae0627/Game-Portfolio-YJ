#pragma once

#include "CoreMinimal.h"
#include "MosesAbilityInputID.generated.h"

UENUM(BlueprintType)
enum class EMosesAbilityInputID : uint8
{
	None UMETA(DisplayName="None"),

	Fire UMETA(DisplayName="Fire"),
	Reload UMETA(DisplayName="Reload"),
};
