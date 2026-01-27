#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Mesh")
	TSoftObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|SFX")
	TSoftObjectPtr<USoundBase> FireSound;

	/** (Cascade 기준) 머즐 FX */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|VFX")
	TSoftObjectPtr<UParticleSystem> MuzzleFlashFX;

	/** 무기별 머즐 소켓 이름(예: MuzzleFlash / AmmoSocket) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName MuzzleSocketName = TEXT("MuzzleFlash");

	/**
	 * ✅ 서버 쿨다운(=재발사 간격) 계산용 몽타주
	 * - 서버가 이 몽타주를 로드해서 GetPlayLength()를 사용한다.
	 * - 무기마다 다른 발사 애니 길이에 맞춰 "애니가 끝나야 다음 발사"를 보장한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Timing")
	TSoftObjectPtr<UAnimMontage> FireMontage;
};
