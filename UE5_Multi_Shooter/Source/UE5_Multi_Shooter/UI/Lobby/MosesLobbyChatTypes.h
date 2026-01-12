#pragma once

#include "MosesLobbyChatTypes.generated.h"

/**
 * FLobbyChatMessage
 * - 로비 채팅 1건
 * - 서버에서만 생성/추가되고, GameState의 ChatHistory로 복제된다.
 * - LateJoin은 ChatHistory 복제만으로 최근 채팅을 복구한다.
 */
USTRUCT(BlueprintType)
struct FLobbyChatMessage
{
	GENERATED_BODY()

	/** 표시용 송신자 닉네임(=DebugName) */
	UPROPERTY(BlueprintReadOnly)
	FString SenderName;

	/** 송신자 고유 PID (PlayerState PersistentId) */
	UPROPERTY(BlueprintReadOnly)
	FGuid SenderPid;

	/** 메시지 본문 */
	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** 서버 UTC UnixTime (ms) */
	UPROPERTY(BlueprintReadOnly)
	int64 ServerUnixTimeMs = 0;
};
