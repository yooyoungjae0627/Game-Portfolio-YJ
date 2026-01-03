// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// ✅ 프로젝트 공용 로그 채널 선언만 한다. (정의는 .cpp에서)
DECLARE_LOG_CATEGORY_EXTERN(LogMosesExp, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesSpawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesTravel, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesAuth, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesRoom, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPhase, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMosesPlayer, Log, All);