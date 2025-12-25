#include "MosesExperienceDefinition.h"

FPrimaryAssetId UMosesExperienceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("Experience")), GetFName());
}

#if WITH_EDITORONLY_DATA
void UMosesExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	// ExperienceDefinition이 스캔되면, 여기에서 BundleData를 확장할 수 있다.
	// 아직 Actions 구조를 안 쓰면 비워둬도 OK.
}
#endif
