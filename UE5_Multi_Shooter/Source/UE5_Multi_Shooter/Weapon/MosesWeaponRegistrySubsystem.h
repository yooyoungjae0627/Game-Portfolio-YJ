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
	void BuildIndexIfNeeded() const;
	void BuildIndexFromAssetRegistry_PathScan(const FName& RootPath) const;
	void TryRegisterWeaponDataFromObjectPath(const FSoftObjectPath& ObjectPath) const;

private:
	mutable TMap<FGameplayTag, TObjectPtr<const UMosesWeaponData>> CachedById;
	mutable bool bIndexBuilt = false;
};
