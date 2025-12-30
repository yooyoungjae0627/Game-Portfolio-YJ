// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Experience/AssetManager 관련
DECLARE_LOG_CATEGORY_EXTERN(LogMosesExp, Log, All);

// Spawn 관련 (GameMode/SpawnGate 등에서 사용 추천)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesSpawn, Log, All);

// Travel 관련 (SeamlessTravel 등에서 사용 추천)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesTravel, Log, All);

// AUTH 관련 (서버 부트, 인증 단계 등)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAuth, Log, All);

// ROOM 관련 (Lobby/Room 초기화, 룸 생성/참가/나가기)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRoom, Log, All);

// PHASE 관련 (Lobby/Match 등 상태 전환)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPhase, Log, All);
