#pragma once
#include "CoreMinimal.h"
#include "MosesLobbyChatTypes.generated.h"

USTRUCT(BlueprintType)
struct FLobbyChatMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SenderName;

    UPROPERTY(BlueprintReadOnly)
    FGuid SenderPid;

    UPROPERTY(BlueprintReadOnly)
    FString Message;

    UPROPERTY(BlueprintReadOnly)
    int64 ServerUnixTimeMs = 0;
};
