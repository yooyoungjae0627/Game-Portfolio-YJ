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
 * - 지금 너 빌드 에러는 "로그 카테고리 미선언"이 원인이다.
 */

 // 기존에 쓰는 것으로 확인된 카테고리들
DECLARE_LOG_CATEGORY_EXTERN(LogMosesSpawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesMove, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPickup, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGAS, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAI, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesCamera, Log, All);

// [ADDED] Experience / GameFeature
DECLARE_LOG_CATEGORY_EXTERN(LogMosesExp, Log, All);

// [ADDED] Player / Auth / Phase / Room (로그에서 실제 사용 중)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPlayer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAuth, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPhase, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRoom, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogMosesGF, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesGrenade, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesHP, Log, All);