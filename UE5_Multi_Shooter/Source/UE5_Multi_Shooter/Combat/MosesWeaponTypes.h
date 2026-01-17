#pragma once

#include "CoreMinimal.h"
#include "MosesWeaponTypes.generated.h"

UENUM(BlueprintType)
enum class EMosesWeaponType : uint8
{
	Rifle	UMETA(DisplayName = "Rifle"),
	Pistol	UMETA(DisplayName = "Pistol"),
	Melee	UMETA(DisplayName = "Melee"),
	Grenade UMETA(DisplayName = "Grenade"),
};

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
