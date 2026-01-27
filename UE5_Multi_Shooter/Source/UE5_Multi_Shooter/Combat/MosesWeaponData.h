#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MosesWeaponData.generated.h"

class USkeletalMesh;
class USoundBase;
class UParticleSystem;

/**
 * UMosesWeaponData
 *
 * [역할]
 * - 무기별 표시/코스메틱 데이터를 DataAsset로 관리한다.
 * - 서버는 승인(WeaponId 확정)만 하고, 클라는 WeaponData로 코스메틱을 재생한다.
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMosesWeaponData() = default;

	// =========================================================================
	// AssetManager (Primary Asset)
	// =========================================================================
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	// =========================================================================
	// Weapon Key
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FGameplayTag WeaponId;

public:
	// =========================================================================
	// Cosmetic: Mesh
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Mesh")
	TSoftObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

public:
	// =========================================================================
	// Cosmetic: Fire SFX/VFX (총마다 다르게)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|SFX")
	TSoftObjectPtr<USoundBase> FireSound;

	/** ✅ Cascade ParticleSystem */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|VFX")
	TSoftObjectPtr<UParticleSystem> MuzzleFlashFX;

public:
	// =========================================================================
	// Socket policy (무기마다 다를 수 있음)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName MuzzleSocketName = TEXT("MuzzleFlash");
};
