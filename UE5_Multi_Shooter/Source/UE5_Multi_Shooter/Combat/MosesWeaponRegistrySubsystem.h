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

	// WeaponId로 Data를 resolve (서버/클라 모두 호출 가능하지만, Day2는 서버 로그로 증명)
	const UMosesWeaponData* ResolveWeaponData(const FGameplayTag& WeaponId) const;

private:
	void BuildIndexIfNeeded() const;

private:
	// 캐시: WeaponId -> DataAsset (로드된 것만 보관)
	mutable TMap<FGameplayTag, TObjectPtr<const UMosesWeaponData>> CachedById;

	// 한 번만 인덱스 빌드
	mutable bool bIndexBuilt = false;
};
