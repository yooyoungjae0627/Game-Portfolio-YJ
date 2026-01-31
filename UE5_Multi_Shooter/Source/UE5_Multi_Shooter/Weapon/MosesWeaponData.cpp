#include "UE5_Multi_Shooter/Weapon/MosesWeaponData.h"

FPrimaryAssetId UMosesWeaponData::GetPrimaryAssetId() const
{
	static const FPrimaryAssetType WeaponDataType(TEXT("WeaponData"));

	const FString NameStr = WeaponId.IsValid() ? WeaponId.ToString() : GetName();
	return FPrimaryAssetId(WeaponDataType, FName(*NameStr));
}
