#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"

FPrimaryAssetId UMosesWeaponData::GetPrimaryAssetId() const
{
	// 개발자 주석:
	// - PrimaryAssetType은 프로젝트 정책에 맞게 고정 문자열을 사용한다.
	// - AssetManager에서 이 타입을 스캔/관리하도록 설정할 수 있다.
	//
	// - WeaponId가 유효하면 그 문자열을 사용하고,
	//   그렇지 않으면 에셋 이름을 fallback으로 사용한다.

	static const FPrimaryAssetType WeaponDataType(TEXT("WeaponData"));

	const FString NameStr = WeaponId.IsValid() ? WeaponId.ToString() : GetName();
	return FPrimaryAssetId(WeaponDataType, FName(*NameStr));
}
