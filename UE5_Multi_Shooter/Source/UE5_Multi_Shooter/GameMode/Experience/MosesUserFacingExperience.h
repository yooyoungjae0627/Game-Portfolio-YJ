#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "MosesUserFacingExperience.generated.h"

/**
 * UMosesUserFacingExperience
 *
 * [역할]
 * - 로비 UI에서 유저가 고르는 “플레이 항목(카드)”
 * - “어느 맵을 열지” + “어느 Experience를 적용할지”만 들고 있는 데이터다.
 *
 * [중요]
 * - ExperienceID는 FPrimaryAssetId 전체(Type:Name)를 유지해야 안전하다.
 *   예) Experience:Exp_Lobby
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesUserFacingExperience : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 열고 싶은 맵 (Primary Asset: Map) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UserFacing")
	FPrimaryAssetId MapID;

	/** 적용할 Experience (Primary Asset Type: Experience) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UserFacing", meta = (AllowedTypes = "Experience"))
	FPrimaryAssetId ExperienceID;
};
