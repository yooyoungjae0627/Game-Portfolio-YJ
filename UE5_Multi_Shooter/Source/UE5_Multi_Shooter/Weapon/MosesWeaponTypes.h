#pragma once

#include "CoreMinimal.h"
#include "MosesWeaponTypes.generated.h"

/**
 * EMosesWeaponType
 *
 * [역할]
 * - 무기 타입 분류(총기/근접/수류탄 등).
 * - WeaponData, UI, GameRule에서 분기 키로 사용한다.
 */
UENUM(BlueprintType)
enum class EMosesWeaponType : uint8
{
	Rifle	UMETA(DisplayName = "Rifle"),
	Pistol	UMETA(DisplayName = "Pistol"),
	Melee	UMETA(DisplayName = "Melee"),
	Grenade UMETA(DisplayName = "Grenade"),
};

/**
 * EMosesFireMode
 *
 * [역할]
 * - 발사 방식 분류.
 * - Auto: 누르고 있는 동안 연사
 * - Click: 클릭 1회당 1발
 */
UENUM(BlueprintType)
enum class EMosesFireMode : uint8
{
	Click,
	Auto
};

/**
 * FAmmoState
 *
 * [역할]
 * - 탄약 상태를 묶은 데이터 구조체.
 *
 * [주의]
 * - 현재 CombatComponent(SSOT)는 RepNotify/Delegate를 위해
 *   슬롯별 Mag/Reserve를 개별 int32로 복제한다.
 * - 이 구조체는 향후 GAS/데이터 통합 시 사용 가능성을 위해 유지한다.
 */
USTRUCT(BlueprintType)
struct FAmmoState
{
	GENERATED_BODY()

public:
	FAmmoState() = default;

	FAmmoState(int32 InMag, int32 InReserve, int32 InMaxMag, int32 InMaxReserve)
		: MagAmmo(InMag)
		, ReserveAmmo(InReserve)
		, MaxMag(InMaxMag)
		, MaxReserve(InMaxReserve)
	{
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MagAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ReserveAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxMag = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxReserve = 0;
};

/**
 * FWeaponSlotState
 *
 * [역할]
 * - 슬롯 단위 무기 상태를 묶은 데이터 구조체.
 *
 * [주의]
 * - 현재 SSOT(CombatComponent)는 GameplayTag 기반 WeaponId를 사용한다.
 * - 본 구조체의 WeaponId(int32)는 예전/타 시스템 호환용으로 남겨둘 수 있다.
 */
USTRUCT(BlueprintType)
struct FWeaponSlotState
{
	GENERATED_BODY()

public:
	FWeaponSlotState() = default;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 WeaponId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMosesWeaponType WeaponType = EMosesWeaponType::Rifle;
};
