#pragma once

#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/PrimaryAssetId.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "MosesUserFacingExperience.generated.h"

class UMosesExperienceDefinition;

/**
 * UMosesUserFacingExperience
 *
 * [역할]
 * - 로비 UI에서 유저가 고르는 “플레이 항목(카드)”
 * - “어느 맵을 열지” + “어느 Experience를 적용할지”만 들고 있는 데이터다.
 *
 * [주의]
 * - "고정 Experience 1개”로도 충분하다.
 * - 하지만 포폴 설득(확장성) 목적이면 이 구조가 깔끔하다.
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesUserFacingExperience : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FText& GetDisplayName() const { return DisplayName; }
	UMosesExperienceDefinition* GetExperience() const { return Experience.Get(); }
	TSoftObjectPtr<UMosesExperienceDefinition> GetExperiencePtr() const { return Experience; }

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|UserFacing")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|UserFacing")
	TSoftObjectPtr<UMosesExperienceDefinition> Experience;

};
