#pragma once

#include "Engine/DataAsset.h"
#include "MosesExperienceDefinition.generated.h"

class UMosesPawnData;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Day3 핵심: PrimaryAssetType을 무조건 "Experience"로 고정 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** 이 Experience에서 활성화할 GameFeature Plugin 이름 목록 (Lyra 방식) */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeatures")
	TArray<FString> GameFeaturesToEnable;

	/** 이 Experience에서 기본으로 사용할 PawnData */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TObjectPtr<const UMosesPawnData> DefaultPawnData;

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
};
