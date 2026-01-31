#pragma once

#include "CoreMinimal.h"

/**
 * MosesLogChannels
 *
 * [기능]
 * - 프로젝트 전체 UE_LOG 카테고리를 한 곳에서 선언/정의한다.
 *
 * [명세]
 * - .h: DECLARE_LOG_CATEGORY_EXTERN(...)
 * - .cpp: DEFINE_LOG_CATEGORY(...)
 *
 * [왜 필요한가?]
 * - UE_LOG(LogXXXX, ...)를 쓸 때 LogXXXX가 선언되어 있지 않으면 C2065가 난다.
 *
 * [공통 인프라 추가]
 * - 표준 태그(접두어) 매크로로 "[COMBAT][SV]" 같은 형식을 강제한다.
 * - 서버 권위 가드에서 사용할 World/NetMode 문자열 헬퍼를 제공한다.
 */

 // ============================================================
 // 기존에 쓰는 것으로 확인된 카테고리들 (유지)
 // ============================================================

DECLARE_LOG_CATEGORY_EXTERN(LogMosesSpawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesMove, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPickup, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGAS, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAI, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesCamera, Log, All);

// Experience / GameFeature
DECLARE_LOG_CATEGORY_EXTERN(LogMosesExp, Log, All);

// Player / Auth / Phase / Room (로그에서 실제 사용 중)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPlayer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAuth, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPhase, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRoom, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesGF, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGrenade, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesHP, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesAsset, Log, All);

// ============================================================
// [MOD] Day5 Flag/Capture & Match/Broadcast 카테고리 추가
// - 빌드 에러: LogMosesFlag / LogMosesMatch C2065 해결용
// - LogMosesMatch: BroadcastComponent, Match HUD 관련 로그
// - LogMosesFlag : FlagSpot, CaptureComponent, Capture UI 관련 로그
// ============================================================

DECLARE_LOG_CATEGORY_EXTERN(LogMosesMatch, Log, All);   // [MOD]
DECLARE_LOG_CATEGORY_EXTERN(LogMosesFlag, Log, All);    // [MOD]

// ============================================================
//  "세분화 카테고리"(선택)
// - 이미 LogMosesCombat/LogMosesHP/LogMosesSpawn로도 충분하지만,
//   면접/증거 로그 분리용으로 카테고리를 더 나누고 싶다면 사용.
// - 쓰기 싫으면 .cpp에서도 DEFINE를 빼고, 여기 선언도 삭제하면 됨.
// ============================================================

DECLARE_LOG_CATEGORY_EXTERN(LogMosesAmmo, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesDeath, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRespawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesScore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesHUD, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesWeapon, Log, All);

// ============================================================
// 표준 태그(접두어) 매크로 - "문자열 포맷 강제"
// - "[COMBAT][SV]" 형태로 통일한다.
// - UE_LOG 호출 시 첫 토큰으로 항상 이걸 붙이면 된다.
// ============================================================

#define MOSES_TAG_COMBAT_SV   TEXT("[COMBAT][SV]")
#define MOSES_TAG_COMBAT_CL   TEXT("[COMBAT][CL]")

#define MOSES_TAG_AMMO_SV     TEXT("[AMMO][SV]")
#define MOSES_TAG_AMMO_CL     TEXT("[AMMO][CL]")

#define MOSES_TAG_PS_SV       TEXT("[PS][SV]")
#define MOSES_TAG_PS_CL       TEXT("[PS][CL]")

#define MOSES_TAG_DEATH_SV    TEXT("[DEATH][SV]")
#define MOSES_TAG_DEATH_CL    TEXT("[DEATH][CL]")

#define MOSES_TAG_RESPAWN_SV  TEXT("[RESPAWN][SV]")
#define MOSES_TAG_RESPAWN_CL  TEXT("[RESPAWN][CL]")

#define MOSES_TAG_SCORE_SV    TEXT("[SCORE][SV]")
#define MOSES_TAG_SCORE_CL    TEXT("[SCORE][CL]")

#define MOSES_TAG_HUD_SV      TEXT("[HUD][SV]")
#define MOSES_TAG_HUD_CL      TEXT("[HUD][CL]")

#define MOSES_TAG_PICKUP_SV   TEXT("[PICKUP][SV]")
#define MOSES_TAG_PICKUP_CL   TEXT("[PICKUP][CL]")

#define MOSES_TAG_GRENADE_SV  TEXT("[GRENADE][SV]")
#define MOSES_TAG_GRENADE_CL  TEXT("[GRENADE][CL]")

#define MOSES_TAG_AUTH_SV     TEXT("[AUTH][SV]")
#define MOSES_TAG_AUTH_CL     TEXT("[AUTH][CL]")

// Asset 준비/소켓/애님 준비 증거 태그
#define MOSES_TAG_ASSET_SV    TEXT("[ASSET][SV]")
#define MOSES_TAG_ASSET_CL    TEXT("[ASSET][CL]")

// 서버 권위 가드 차단 로그(DoD 증거 핵심)
#define MOSES_TAG_BLOCKED_CL  TEXT("[BLOCK][CL]")

// ============================================================
// [MOD] Day5 Flag/Match 태그 추가 (선택이지만 로그 통일에 유용)
// ============================================================

#define MOSES_TAG_FLAG_SV     TEXT("[FLAG][SV]")        // [MOD]
#define MOSES_TAG_FLAG_CL     TEXT("[FLAG][CL]")        // [MOD]
#define MOSES_TAG_MATCH_SV    TEXT("[MATCH][SV]")       // [MOD]
#define MOSES_TAG_MATCH_CL    TEXT("[MATCH][CL]")       // [MOD]
#define MOSES_TAG_BROADCAST_SV TEXT("[BROADCAST][SV]")  // [MOD]
#define MOSES_TAG_BROADCAST_CL TEXT("[BROADCAST][CL]")  // [MOD]

// ============================================================
// 로그 부가정보 헬퍼 (Null 안전)
// - 서버 권위 가드/증거 로그에서 World/NetMode를 같이 찍고 싶을 때 사용.
// ============================================================

namespace MosesLog
{
	const TCHAR* GetWorldNameSafe(const UObject* WorldContext);
	const TCHAR* GetNetModeNameSafe(const UObject* WorldContext);
}
