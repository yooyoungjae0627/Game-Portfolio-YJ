#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "MosesWeaponRegistrySubsystem.generated.h"

class UMosesWeaponData;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesWeaponRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** WeaponId로 Data를 resolve (서버/클라 모두 호출 가능) */
	const UMosesWeaponData* ResolveWeaponData(const FGameplayTag& WeaponId) const;

private:
	/** 인덱스가 없으면 생성(한 번만). AssetManager 0개면 AssetRegistry 경로 스캔으로 fallback */
	void BuildIndexIfNeeded() const;

	/** [MOD] GF_Combat_Data 폴더를 직접 스캔해서 DataAsset을 인덱싱한다. */
	void BuildIndexFromAssetRegistry_PathScan(const FName& RootPath) const;

	/** [MOD] 단일 DataAsset을 로드해서 CachedById에 등록한다(중복/무효 방어 + 로그). */
	void TryRegisterWeaponDataFromObjectPath(const FSoftObjectPath& ObjectPath) const;

private:
	/** 캐시: WeaponId -> DataAsset(로드된 것만 보관) */
	mutable TMap<FGameplayTag, TObjectPtr<const UMosesWeaponData>> CachedById;

	/** 한 번만 인덱스 빌드 */
	mutable bool bIndexBuilt = false;
};
