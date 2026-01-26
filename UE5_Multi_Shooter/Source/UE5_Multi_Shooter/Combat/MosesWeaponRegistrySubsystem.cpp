#include "MosesWeaponRegistrySubsystem.h"

#include "MosesWeaponData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void UMosesWeaponRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 인덱스는 “처음 Resolve” 때 만들어도 되지만,
	// 개발 중에는 여기서 한번 만들어 두면 FAIL 원인 찾기가 쉬움.
	BuildIndexIfNeeded();
}

const UMosesWeaponData* UMosesWeaponRegistrySubsystem::ResolveWeaponData(const FGameplayTag& WeaponId) const
{
	if (!WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] ResolveData FAIL WeaponId=INVALID"));
		return nullptr;
	}

	BuildIndexIfNeeded();

	if (const TObjectPtr<const UMosesWeaponData>* Found = CachedById.Find(WeaponId))
	{
		return Found ? Found->Get() : nullptr;
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] ResolveData FAIL Weapon=%s (NotIndexed)"), *WeaponId.ToString());
	return nullptr;
}

void UMosesWeaponRegistrySubsystem::BuildIndexIfNeeded() const
{
	if (bIndexBuilt)
	{
		return;
	}
	bIndexBuilt = true;

	UAssetManager& AM = UAssetManager::Get();

	// GetPrimaryAssetIdList는 “스캔 설정(AssetManagerSettings)”이 되어 있어야 결과가 나온다.
	TArray<FPrimaryAssetId> AssetIds;
	AM.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("MosesWeaponData")), AssetIds);

	if (AssetIds.Num() == 0)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] Registry IndexBuild: 0 assets (AssetManager scan / Cook 누락 가능)"));
		return;
	}

	FStreamableManager& Streamable = AM.GetStreamableManager();

	for (const FPrimaryAssetId& Id : AssetIds)
	{
		const FSoftObjectPath Path = AM.GetPrimaryAssetPath(Id);
		if (!Path.IsValid())
		{
			continue;
		}

		// 개발 편의상 동기 로드 (데디/패키징에서 누락되면 여기서 바로 0개/실패로 걸림)
		UObject* LoadedObj = Streamable.LoadSynchronous(Path, false);
		const UMosesWeaponData* Data = Cast<UMosesWeaponData>(LoadedObj);
		if (!Data || !Data->WeaponId.IsValid())
		{
			continue;
		}

		if (CachedById.Contains(Data->WeaponId))
		{
			UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] Registry Duplicate WeaponId=%s Asset=%s"),
				*Data->WeaponId.ToString(), *GetNameSafe(Data));
			continue;
		}

		CachedById.Add(Data->WeaponId, Data);

		UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON] Registry Indexed Weapon=%s Asset=%s"),
			*Data->WeaponId.ToString(), *GetNameSafe(Data));
	}
}
