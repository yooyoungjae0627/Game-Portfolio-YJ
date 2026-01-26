#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MosesWeaponData.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EMosesFireMode : uint8
{
	SemiAuto,
	FullAuto
};

/**
 * UMosesWeaponData
 *
 * Day2 기준: 무기 스펙의 단일 출처(SSOT for spec).
 * - 코스메틱은 Actor 스폰 대신, Character의 WeaponMeshComp에 StaticMesh를 지정하는 방식(간단/안정).
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMosesWeaponData();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	// -----------------------
	// Identity
	// -----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Id")
	FGameplayTag WeaponId;

	// -----------------------
	// Cosmetic (간단 버전)
	// -----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Cosmetic")
	TSoftObjectPtr<UStaticMesh> WeaponStaticMesh;

	// -----------------------
	// Combat Spec (Server Authority Source)
	// -----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec")
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec")
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec")
	EMosesFireMode FireMode = EMosesFireMode::SemiAuto;

	// 60/RPM
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec", meta = (ClampMin = "0.01"))
	float FireIntervalSec = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec", meta = (ClampMin = "1"))
	int32 MagSize = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec", meta = (ClampMin = "0"))
	int32 MaxReserve = 120;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spec", meta = (ClampMin = "0.0"))
	float ReloadTime = 1.6f;
};
