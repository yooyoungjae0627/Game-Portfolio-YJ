// MosesPerfEvidenceHelper.h
#pragma once

#include "CoreMinimal.h"
#include "MosesPerfTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MosesPerfEvidenceHelper.generated.h"

/**
 * Evidence Helper
 * - stat 명령 실행 프리셋 제공
 * - 파일명 규칙 문자열 생성(로그로 출력) -> 사람이 그대로 저장
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesPerfEvidenceHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 공통 캡처 프리셋: stat unit/game/anim/net */
	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Evidence")
	static void RunCapturePreset_Base(UObject* WorldContextObject);

	/** 특정 stat 1개 실행 */
	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Evidence")
	static void RunStat(UObject* WorldContextObject, EMosesPerfStatKind Kind);

	/** Evidence 파일명 규칙 생성 */
	UFUNCTION(BlueprintPure, Category="Moses|Perf|Evidence")
	static FString MakeEvidenceFilename(
		int32 DayNumber,
		EMosesPerfScenarioId Scenario,
		bool bIsDedicatedServer,
		EMosesPerfStatKind StatKind,
		EMosesPerfBeforeAfter BeforeAfter);

	/** 사람이 저장할 파일명/조건을 로그로 출력 */
	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Evidence")
	static void LogEvidenceHint(
		UObject* WorldContextObject,
		int32 DayNumber,
		EMosesPerfScenarioId Scenario,
		bool bIsDedicatedServer,
		EMosesPerfBeforeAfter BeforeAfter);
};
