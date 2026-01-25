#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

/**
 * MosesAuthorityGuards
 *
 * [목표]
 * - 서버 권위 100%를 "말"이 아니라 "코드"로 봉쇄한다.
 * - 클라이언트에서 판정 로직(Trace/Damage/Ammo 변경 등)에 진입하면 즉시 차단하고,
 *   반드시 [BLOCK][CL] 로그를 남겨 증거로 사용한다.
 *
 * [사용 규칙]
 * - 서버 전용 로직 함수의 "첫 줄"에 아래 매크로를 넣는다.
 *   MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted trace (server-only)"));
 * - Server RPC 내부에서 인자 검증도 동일하게 가드한다.
 */

namespace MosesAuth
{
	// ------------------------------------------------------------
	// 서버 권위 판정(Null 안전)
	// - Standalone은 서버처럼 취급한다.
	// - Listen/Dedicated는 서버.
	// - Client는 차단 대상.
	// ------------------------------------------------------------
	inline bool IsServerLike(const UObject* WorldContext)
	{
		if (WorldContext == nullptr)
		{
			return false;
		}

		const UWorld* World = WorldContext->GetWorld();
		if (World == nullptr)
		{
			return false;
		}

		const ENetMode NetMode = World->GetNetMode();
		return (NetMode != NM_Client);
	}

	// ------------------------------------------------------------
	// BLOCK 로그 (표준 포맷 강제)
	// ------------------------------------------------------------
	inline void LogBlocked(const UObject* WorldContext, const FName& SystemName, const TCHAR* Reason)
	{
		UE_LOG(
			LogMosesAuth,
			Warning,
			TEXT("%s System=%s %s %s Reason=%s"),
			MOSES_TAG_BLOCKED_CL,
			*SystemName.ToString(),
			MosesLog::GetNetModeNameSafe(WorldContext),
			MosesLog::GetWorldNameSafe(WorldContext),
			(Reason != nullptr) ? Reason : TEXT("Unknown"));
	}

	inline void LogBlockedRpcArgs(const UObject* WorldContext, const FName& SystemName, const TCHAR* Reason)
	{
		UE_LOG(
			LogMosesAuth,
			Warning,
			TEXT("%s System=%s RPC_ARGS_INVALID %s %s Reason=%s"),
			MOSES_TAG_BLOCKED_CL,
			*SystemName.ToString(),
			MosesLog::GetNetModeNameSafe(WorldContext),
			MosesLog::GetWorldNameSafe(WorldContext),
			(Reason != nullptr) ? Reason : TEXT("Unknown"));
	}
}

// ============================================================
//  서버 권위 가드 (Void/Return)
// ============================================================

#define MOSES_GUARD_AUTHORITY_VOID(WorldContext, SystemNameLiteral, ReasonText) \
	do \
	{ \
		if (!MosesAuth::IsServerLike(WorldContext)) \
		{ \
			MosesAuth::LogBlocked(WorldContext, FName(TEXT(SystemNameLiteral)), ReasonText); \
			return; \
		} \
	} while (0)

#define MOSES_GUARD_AUTHORITY_RET(WorldContext, SystemNameLiteral, ReasonText, ReturnValue) \
	do \
	{ \
		if (!MosesAuth::IsServerLike(WorldContext)) \
		{ \
			MosesAuth::LogBlocked(WorldContext, FName(TEXT(SystemNameLiteral)), ReasonText); \
			return (ReturnValue); \
		} \
	} while (0)

// ============================================================
//  Server RPC 인자 검증 (Void/Return)
// ============================================================

#define MOSES_GUARD_RPC_ARGS_VOID(WorldContext, SystemNameLiteral, Condition, ReasonText) \
	do \
	{ \
		if (!(Condition)) \
		{ \
			MosesAuth::LogBlockedRpcArgs(WorldContext, FName(TEXT(SystemNameLiteral)), ReasonText); \
			return; \
		} \
	} while (0)

#define MOSES_GUARD_RPC_ARGS_RET(WorldContext, SystemNameLiteral, Condition, ReasonText, ReturnValue) \
	do \
	{ \
		if (!(Condition)) \
		{ \
			MosesAuth::LogBlockedRpcArgs(WorldContext, FName(TEXT(SystemNameLiteral)), ReasonText); \
			return (ReturnValue); \
		} \
	} while (0)
