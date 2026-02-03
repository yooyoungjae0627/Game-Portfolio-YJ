#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UE5_Multi_Shooter/Lobby/UI/MosesLobbyChatTypes.h"
#include "MosesLobbyChatRowItem.generated.h"

/**
 * UMosesLobbyChatRowItem
 * - ChatListView에 넣는 UI 전용 ItemData UObject
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesLobbyChatRowItem : public UObject
{
	GENERATED_BODY()

public:
	static UMosesLobbyChatRowItem* Make(UObject* Outer, const FLobbyChatMessage& InMsg);

public:
	UPROPERTY(BlueprintReadOnly, Category="Chat")
	FString SenderName;

	UPROPERTY(BlueprintReadOnly, Category="Chat")
	FGuid SenderPid;

	UPROPERTY(BlueprintReadOnly, Category="Chat")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category="Chat")
	int64 ServerUnixTimeMs = 0;
};
