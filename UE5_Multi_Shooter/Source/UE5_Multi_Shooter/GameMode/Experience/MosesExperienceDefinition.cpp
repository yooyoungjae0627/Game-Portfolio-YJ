#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h" 

UMosesExperienceDefinition::UMosesExperienceDefinition()
{
}

FPrimaryAssetId UMosesExperienceDefinition::GetPrimaryAssetId() const
{
	// PrimaryAssetType 이름은 프로젝트 정책에 맞춰 고정.
	// (AssetManager 스캔/로딩에 사용)
	static const FPrimaryAssetType AssetType(TEXT("Experience"));
	return FPrimaryAssetId(AssetType, GetFName());
}

const UMosesPawnData* UMosesExperienceDefinition::GetDefaultPawnDataLoaded() const // [ADD]
{
	/**
	 * 개발자 주석:
	 * - SoftPtr을 실제 포인터로 변환하려면 LoadSynchronous()가 필요하다.
	 * - 이 함수는 Cast를 수행하므로, PawnData 타입이 "완전 정의" 상태여야 한다.
	 * - 그래서 헤더 인라인이 아니라 cpp에 둔다.
	 */
	return DefaultPawnData.IsNull() ? nullptr : DefaultPawnData.LoadSynchronous();
}

#if WITH_EDITORONLY_DATA
void UMosesExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();
}
#endif
