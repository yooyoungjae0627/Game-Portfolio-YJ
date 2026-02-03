#include "MosesWeaponRegistrySubsystem.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/SoftObjectPath.h"

void UMosesWeaponRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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

	// ---------------------------------------------------------------------
	// 1) 1차: AssetManager PrimaryAssetIdList
	//    [MOD] PrimaryAssetType을 WeaponData로 통일(UMosesWeaponData::GetPrimaryAssetId와 일치)
	// ---------------------------------------------------------------------
	UAssetManager& AM = UAssetManager::Get();

	TArray<FPrimaryAssetId> AssetIds;
	AM.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("WeaponData")), AssetIds);

	if (AssetIds.Num() > 0)
	{
		FStreamableManager& Streamable = AM.GetStreamableManager();

		for (const FPrimaryAssetId& Id : AssetIds)
		{
			const FSoftObjectPath Path = AM.GetPrimaryAssetPath(Id);
			if (!Path.IsValid())
			{
				continue;
			}

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

			UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON] Registry Indexed(AssetManager) Weapon=%s Asset=%s"),
				*Data->WeaponId.ToString(), *GetNameSafe(Data));
		}

		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] Registry IndexBuild Done via AssetManager Count=%d"), CachedById.Num());
		return;
	}

	UE_LOG(LogMosesWeapon, Warning,
		TEXT("[WEAPON] Registry IndexBuild: 0 assets from AssetManager. Fallback -> AssetRegistry path scan"));

	// ---------------------------------------------------------------------
	// 2) 2차: AssetRegistry 경로 스캔 fallback
	// ---------------------------------------------------------------------
	static const FName CombatDataWeaponsPath(TEXT("/GF_Combat_Data/Data/Weapons"));
	BuildIndexFromAssetRegistry_PathScan(CombatDataWeaponsPath);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] Registry IndexBuild Done via AssetRegistry Count=%d"), CachedById.Num());
}

void UMosesWeaponRegistrySubsystem::BuildIndexFromAssetRegistry_PathScan(const FName& RootPath) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	{
		TArray<FString> PathsToScan;
		PathsToScan.Add(RootPath.ToString());
		AssetRegistry.ScanPathsSynchronous(PathsToScan, false);
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(RootPath);
	Filter.ClassPaths.Add(UMosesWeaponData::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	if (Assets.Num() == 0)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] AssetRegistry Scan: 0 assets Path=%s"), *RootPath.ToString());
		return;
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] AssetRegistry Scan OK Path=%s Count=%d"), *RootPath.ToString(), Assets.Num());

	for (const FAssetData& AD : Assets)
	{
		const FSoftObjectPath ObjectPath(AD.ToSoftObjectPath());
		TryRegisterWeaponDataFromObjectPath(ObjectPath);
	}
}

void UMosesWeaponRegistrySubsystem::TryRegisterWeaponDataFromObjectPath(const FSoftObjectPath& ObjectPath) const
{
	if (!ObjectPath.IsValid())
	{
		return;
	}

	UAssetManager& AM = UAssetManager::Get();
	FStreamableManager& Streamable = AM.GetStreamableManager();

	UObject* LoadedObj = Streamable.LoadSynchronous(ObjectPath, false);
	const UMosesWeaponData* Data = Cast<UMosesWeaponData>(LoadedObj);
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] AssetRegistry Load FAIL Path=%s"), *ObjectPath.ToString());
		return;
	}

	if (!Data->WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] AssetRegistry Skip (Invalid WeaponId) Asset=%s Path=%s"),
			*GetNameSafe(Data), *ObjectPath.ToString());
		return;
	}

	if (CachedById.Contains(Data->WeaponId))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] Registry Duplicate WeaponId=%s Asset=%s"),
			*Data->WeaponId.ToString(), *GetNameSafe(Data));
		return;
	}

	CachedById.Add(Data->WeaponId, Data);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON] Registry Indexed(AssetRegistry) Weapon=%s Asset=%s"),
		*Data->WeaponId.ToString(), *GetNameSafe(Data));
}
