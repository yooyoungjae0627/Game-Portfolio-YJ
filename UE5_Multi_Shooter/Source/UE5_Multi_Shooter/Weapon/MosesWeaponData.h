#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "UE5_Multi_Shooter/Weapon/MosesWeaponTypes.h" 
#include "MosesWeaponData.generated.h"

class USkeletalMesh;
class USoundBase;
class UParticleSystem;
class UAnimMontage;

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMosesWeaponData() = default;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FGameplayTag WeaponId;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Type")
	EMosesWeaponType WeaponType = EMosesWeaponType::Rifle;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Fire")
	EMosesFireMode FireMode = EMosesFireMode::Auto;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Stats")
	float Damage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Ammo")
	int32 MagSize = 30;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Ammo")
	int32 MaxReserve = 90;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Reload")
	float ReloadSeconds = 1.8f;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Aim")
	float SpreadDegrees_Min = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Weapon|Aim")
	float SpreadDegrees_Max = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Mesh")
	TSoftObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

	// [MOD] PlayerCharacter가 사용하는 이름과 통일됨(FireSFX/MuzzleVFX)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|SFX")
	TSoftObjectPtr<USoundBase> FireSFX;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|VFX")
	TSoftObjectPtr<UParticleSystem> MuzzleVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName MuzzleSocketName = TEXT("MuzzleFlash");
};
