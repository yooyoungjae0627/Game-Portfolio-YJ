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
	Click	UMETA(DisplayName = "Click"),
	Auto	UMETA(DisplayName = "Auto"),
};

/**
 * EMosesWeaponSlot
 *
 * [중요]
 * - UHT 경고 방지: 반드시 0 엔트리(None)를 둔다.
 */
UENUM(BlueprintType)
enum class EMosesWeaponSlot : uint8
{
	None = 0 UMETA(DisplayName = "None"),

	Slot1 = 1 UMETA(DisplayName = "Slot1"),
	Slot2 = 2 UMETA(DisplayName = "Slot2"),
	Slot3 = 3 UMETA(DisplayName = "Slot3"),
	Slot4 = 4 UMETA(DisplayName = "Slot4"),
};

/**
 * EMosesAmmoType  ✅ [NEW]
 *
 * [역할]
 * - 무기별 탄약을 분리하기 위한 타입.
 * - RifleAmmo != ShotgunAmmo != SniperAmmo != GrenadeAmmo
 *
 * [주의]
 * - UENUM 기본값(0) 안전을 위해 None을 둔다.
 */
UENUM(BlueprintType)
enum class EMosesAmmoType : uint8
{
	None = 0 UMETA(DisplayName = "None"),

	Rifle = 1 UMETA(DisplayName = "Rifle"),
	Shotgun = 2 UMETA(DisplayName = "Shotgun"),
	Sniper = 3 UMETA(DisplayName = "Sniper"),
	Grenade = 4 UMETA(DisplayName = "Grenade"),
};

/**
 * FAmmoState
 *
 * [역할]
 * - 탄약 상태를 묶은 데이터 구조체.
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
