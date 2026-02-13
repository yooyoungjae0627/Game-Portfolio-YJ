// ============================================================================
// UE5_Multi_Shooter/MosesLogChannels.h
// ============================================================================

#pragma once

#include "CoreMinimal.h"

/**
 * MosesLogChannels
 *
 * 프로젝트 전역 로그 카테고리와 공통 로그 유틸을 한 곳에서 관리한다.
 *
 * - .h : DECLARE_LOG_CATEGORY_EXTERN(...)
 * - .cpp: DEFINE_LOG_CATEGORY(...)
 *
 * 또한 로그 포맷을 통일하기 위한 표준 태그(TEXT 매크로)와,
 * World/NetMode 정보를 안전하게 얻는 헬퍼를 제공한다.
 */

 // ============================================================================
 // Log Categories
 // ============================================================================

 // Core
DECLARE_LOG_CATEGORY_EXTERN(LogMosesSpawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesMove, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPickup, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGAS, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAI, Log, All);

// Combat / Camera
DECLARE_LOG_CATEGORY_EXTERN(LogMosesCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesCamera, Log, All);

// Experience / GameFeature
DECLARE_LOG_CATEGORY_EXTERN(LogMosesExp, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGF, Log, All);

// Player / Auth / Phase / Room
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPlayer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAuth, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPhase, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRoom, Log, All);

// Weapon / Grenade / HP
DECLARE_LOG_CATEGORY_EXTERN(LogMosesWeapon, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGrenade, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesHP, Log, All);

// Asset
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAsset, Log, All);

// Match / Flag
DECLARE_LOG_CATEGORY_EXTERN(LogMosesMatch, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesFlag, Log, All);

// Optional: fine-grained categories
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAmmo, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesDeath, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRespawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesScore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesHUD, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesZombie, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAnnounce, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesKillFeed, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesPerf, Log, All);

// ============================================================================
// Standard Tags (Prefix)
// ============================================================================

#define MOSES_TAG_COMBAT_SV       TEXT("[COMBAT][SV]")
#define MOSES_TAG_COMBAT_CL       TEXT("[COMBAT][CL]")

#define MOSES_TAG_AMMO_SV         TEXT("[AMMO][SV]")
#define MOSES_TAG_AMMO_CL         TEXT("[AMMO][CL]")

#define MOSES_TAG_PS_SV           TEXT("[PS][SV]")
#define MOSES_TAG_PS_CL           TEXT("[PS][CL]")

#define MOSES_TAG_DEATH_SV        TEXT("[DEATH][SV]")
#define MOSES_TAG_DEATH_CL        TEXT("[DEATH][CL]")

#define MOSES_TAG_RESPAWN_SV      TEXT("[RESPAWN][SV]")
#define MOSES_TAG_RESPAWN_CL      TEXT("[RESPAWN][CL]")

#define MOSES_TAG_SCORE_SV        TEXT("[SCORE][SV]")
#define MOSES_TAG_SCORE_CL        TEXT("[SCORE][CL]")

#define MOSES_TAG_HUD_SV          TEXT("[HUD][SV]")
#define MOSES_TAG_HUD_CL          TEXT("[HUD][CL]")

#define MOSES_TAG_PICKUP_SV       TEXT("[PICKUP][SV]")
#define MOSES_TAG_PICKUP_CL       TEXT("[PICKUP][CL]")

#define MOSES_TAG_GRENADE_SV      TEXT("[GRENADE][SV]")
#define MOSES_TAG_GRENADE_CL      TEXT("[GRENADE][CL]")

#define MOSES_TAG_AUTH_SV         TEXT("[AUTH][SV]")
#define MOSES_TAG_AUTH_CL         TEXT("[AUTH][CL]")

#define MOSES_TAG_ASSET_SV        TEXT("[ASSET][SV]")
#define MOSES_TAG_ASSET_CL        TEXT("[ASSET][CL]")

#define MOSES_TAG_BLOCKED_CL      TEXT("[BLOCK][CL]")

#define MOSES_TAG_FLAG_SV         TEXT("[FLAG][SV]")
#define MOSES_TAG_FLAG_CL         TEXT("[FLAG][CL]")

#define MOSES_TAG_MATCH_SV        TEXT("[MATCH][SV]")
#define MOSES_TAG_MATCH_CL        TEXT("[MATCH][CL]")

#define MOSES_TAG_BROADCAST_SV    TEXT("[BROADCAST][SV]")
#define MOSES_TAG_BROADCAST_CL    TEXT("[BROADCAST][CL]")

// ============================================================================
// Log Helpers
// ============================================================================

namespace MosesLog
{
	/** WorldContext로부터 World 이름을 안전하게 얻는다. (NULL 안전) */
	const TCHAR* GetWorldNameSafe(const UObject* WorldContext);

	/** WorldContext로부터 NetMode 이름을 안전하게 얻는다. (NULL 안전) */
	const TCHAR* GetNetModeNameSafe(const UObject* WorldContext);
}
