#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"

UMosesWeaponData::UMosesWeaponData()
{
}

FPrimaryAssetId UMosesWeaponData::GetPrimaryAssetId() const
{
	static const FName AssetTypeName(TEXT("MosesWeaponData"));
	return FPrimaryAssetId(AssetTypeName, GetFName());
}
