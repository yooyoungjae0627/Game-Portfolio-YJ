// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"
#include "MosesExperienceDefinition.generated.h"

//class UGameFeatureAction;
class UMosesPawnData;

/**
 * ExperienceDefinition = "로비/매치 같은 게임 상태의 '구성 데이터'"
 * - PrimaryDataAsset 이므로 AssetManager 스캔/로딩 대상으로 취급된다.
 * - GameFeature(플러그인), 기본 PawnData 등을 여기에 묶어서 "상태 단위"로 교체한다.
 */

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UMosesExperienceDefinition(const FObjectInitializer& ObjectInitializer);

	/** (선택) Experience에서 활성화할 GameFeature Plugin 이름 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeatures")
	TArray<FString> GameFeaturesToEnable;

	/** (선택) Experience에서 적용할 GameFeatureAction들 */
	//UPROPERTY(EditDefaultsOnly, Instanced, Category = "GameFeatures")
	//TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** (선택) 이 Experience의 기본 PawnData */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TObjectPtr<const UMosesPawnData> DefaultPawnData;

#if WITH_EDITORONLY_DATA
	/**
	 * 에디터에서 BundleData를 업데이트한다.
	 * - Action이 추가적으로 필요한 번들 에셋을 AssetBundleData에 등록하게 해서
	 *   "Experience 하나를 로드하면 관련 리소스가 같이 로드"되도록 만든다.
	 */
	virtual void UpdateAssetBundleData() override;
#endif
	
	
};
