// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesExperienceDefinition.h"

// #include "GameFeatureAction.h"

UMosesExperienceDefinition::UMosesExperienceDefinition(const FObjectInitializer& ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA
void UMosesExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	// ✅ Day3 포인트:
	// ExperienceDefinition이 스캔되면, 여기에서 Actions가 BundleData를 확장할 수 있다.
	// 즉 "Experience 하나를 root로 묶어서" 로딩 정책을 만들 수 있다.
//	for (UGameFeatureAction* Action : Actions)
//	{
//		if (Action)
//		{
//			Action->AddAdditionalAssetBundleData(AssetBundleData);
//		}
//	}
//}
#endif
}



