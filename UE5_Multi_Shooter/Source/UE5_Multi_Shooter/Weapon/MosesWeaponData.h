#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "UE5_Multi_Shooter/Weapon/MosesWeaponTypes.h"
#include "MosesWeaponData.generated.h"

class USkeletalMesh;
class USoundBase;
class UParticleSystem;

/**
 * UMosesWeaponData
 *
 * [역할]
 * - 무기 스펙/코스메틱을 데이터로 고정한다.
 * - PrimaryAsset으로 관리하여 GF_Combat_Data에서 데이터만 제공 가능하게 한다.
 *
 * [서버 권위]
 * - Damage / MagSize / MaxReserve / ReloadSeconds / FireMode / AmmoType는
 *   서버 판정(SSOT)의 기준 값이다.
 *
 * [코스메틱]
 * - FireSFX / MuzzleVFX는 서버 승인 후 Multicast로 클라에서 재생한다.
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMosesWeaponData() = default;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	/** 무기 고유 ID (GameplayTag) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FGameplayTag WeaponId;

	/** 무기 타입(분기 키) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Type")
	EMosesWeaponType WeaponType = EMosesWeaponType::Rifle;

	/** [NEW] 고정 슬롯(1~4) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Slot")
	EMosesWeaponSlot FixedSlot = EMosesWeaponSlot::Slot1;

	/** [NEW] 탄약 타입(무기별 분리) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	EMosesAmmoType AmmoType = EMosesAmmoType::Rifle;

	/** 발사 방식 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire")
	EMosesFireMode FireMode = EMosesFireMode::Auto;

	/** 데미지(히트스캔 기준). 유탄은 폭발 데미지의 기본값으로 사용 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Stats")
	float Damage = 20.0f;

	/** 탄창 크기 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	int32 MagSize = 30;

	/** 최대 보유 탄약(Reserve 최대치) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	int32 MaxReserve = 90;

	/** 리로드 시간(서버 타이머 기준) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Reload")
	float ReloadSeconds = 1.8f;

	/** Day7: 정지/이동 스프레드 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim")
	float SpreadDegrees_Min = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim")
	float SpreadDegrees_Max = 3.0f;

	/** [NEW] Projectile 기반 무기 여부(유탄) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile")
	bool bIsProjectileWeapon = false;

	/** 무기 메시(코스메틱) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Mesh")
	TSoftObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

	// PlayerCharacter가 사용하는 이름과 통일(FireSFX/MuzzleVFX)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|SFX")
	TSoftObjectPtr<USoundBase> FireSFX;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|VFX")
	TSoftObjectPtr<UParticleSystem> MuzzleVFX;

	/** 총구 소켓 이름(무기 메시 기준) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName MuzzleSocketName = TEXT("MuzzleFlash");
};
