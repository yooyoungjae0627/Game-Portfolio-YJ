#include "MosesExperienceDefinition.h"

FPrimaryAssetId UMosesExperienceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("Experience")), GetFName());
}

#if WITH_EDITORONLY_DATA
void UMosesExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();
	// 필요해지면 번들 확장
}
#endif
