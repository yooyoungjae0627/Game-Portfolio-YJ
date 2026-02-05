// MosesPerfTypes.h
#pragma once

#include "CoreMinimal.h"
#include "MosesPerfTypes.generated.h"

/**
 * PerfMode: Tier/Significance 기반 정책의 "중앙 스위치"
 * - DAY1에서는 "연결만" (실제 정책 적용은 DAY6+에서)
 */
UENUM(BlueprintType)
enum class EMosesPerfMode : uint8
{
	Mode0 UMETA(DisplayName = "PerfMode 0 (Highest)"),
	Mode1 UMETA(DisplayName = "PerfMode 1"),
	Mode2 UMETA(DisplayName = "PerfMode 2"),
	Mode3 UMETA(DisplayName = "PerfMode 3 (Lowest)"),
};

/** Baseline 시나리오 ID (DAY10까지 변경 금지) */
UENUM(BlueprintType)
enum class EMosesPerfScenarioId : uint8
{
	A1 UMETA(DisplayName="A1: DS + Players2 + Zombies50"),
	A2 UMETA(DisplayName="A2: DS + Players4 + Zombies50 + Pickups50"),
	A3 UMETA(DisplayName="A3: Zombies100"),
	A4 UMETA(DisplayName="A4: VFXSpam ON"),
	A5 UMETA(DisplayName="A5: ShellSpam ON"),
};

/** 카메라 고정 Marker */
UENUM(BlueprintType)
enum class EMosesPerfMarkerId : uint8
{
	Marker01 UMETA(DisplayName="Marker01 (Near Dense)"),
	Marker02 UMETA(DisplayName="Marker02 (Mid Many)"),
	Marker03 UMETA(DisplayName="Marker03 (Shadow/GPU)"),
};

/** 시나리오 스펙(문서 기준 재현 파라미터) */
USTRUCT(BlueprintType)
struct FMosesPerfScenarioSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ZombieCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PickupCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bForceAggro = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bVFXSpam = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAudioSpam = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShellSpam = false;
};

/** Evidence 파일명 규칙을 위한 캡처 종류 */
UENUM(BlueprintType)
enum class EMosesPerfStatKind : uint8
{
	Unit UMETA(DisplayName="stat unit"),
	Game UMETA(DisplayName="stat game"),
	Anim UMETA(DisplayName="stat anim"),
	Net  UMETA(DisplayName="stat net"),
	Gpu  UMETA(DisplayName="stat gpu"),
	Audio UMETA(DisplayName="stat audio"),
};

/** Before/After 구분 */
UENUM(BlueprintType)
enum class EMosesPerfBeforeAfter : uint8
{
	Before UMETA(DisplayName="before"),
	After  UMETA(DisplayName="after"),
};
