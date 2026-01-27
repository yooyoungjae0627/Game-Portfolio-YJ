#include "MosesWeaponRegistrySubsystem.h"

#include "MosesWeaponData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "AssetRegistry/AssetRegistryModule.h"         // [MOD]
#include "AssetRegistry/IAssetRegistry.h"              // [MOD]
#include "UObject/SoftObjectPath.h"                    // [MOD]
#include "UObject/SoftObjectPtr.h"                     // [MOD]

void UMosesWeaponRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 개발 편의:
	// - 인덱스는 “처음 Resolve” 때 만들어도 되지만,
	//   개발 중에는 여기서 한번 만들어 두면 FAIL 원인 찾기가 쉬움.
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
	// 1) 1차: AssetManager PrimaryAssetIdList (설정이 잘 되어 있으면 가장 깔끔)
	// ---------------------------------------------------------------------
	UAssetManager& AM = UAssetManager::Get();

	TArray<FPrimaryAssetId> AssetIds;
	AM.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("MosesWeaponData")), AssetIds);

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

			// 개발 편의상 동기 로드
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
	// - GameFeature 데이터 플러그인 폴더에 DataAsset이 있어도,
	//   AssetManager 액션/스캔이 누락되면 PrimaryAssetIdList가 0이 될 수 있음.
	// - 이 fallback은 “플러그인 폴더에 있는 DataAsset을 그냥 읽자”를 코드로 구현한다.
	// ---------------------------------------------------------------------

	// [MOD] 너 스샷 기준 플러그인 가상 경로는 보통 아래처럼 잡힌다.
	// - Plugins/GF_Combat_Data/Data/Weapons  ->  /GF_Combat_Data/Data/Weapons
	static const FName CombatDataWeaponsPath(TEXT("/GF_Combat_Data/Data/Weapons"));

	BuildIndexFromAssetRegistry_PathScan(CombatDataWeaponsPath);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON] Registry IndexBuild Done via AssetRegistry Count=%d"), CachedById.Num());
}

void UMosesWeaponRegistrySubsystem::BuildIndexFromAssetRegistry_PathScan(const FName& RootPath) const
{
	// AssetRegistry 가져오기
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// [MOD] 에디터/런타임 모두에서 경로 스캔을 안전하게 하기 위해 동기 스캔을 한 번 호출한다.
	// - Cook/패키징 환경에서는 이미 레지스트리가 구성되어 있을 수 있지만,
	//   에디터 PIE에서 플러그인 경로가 늦게 인식되는 케이스를 방어한다.
	{
		TArray<FString> PathsToScan;
		PathsToScan.Add(RootPath.ToString());
		AssetRegistry.ScanPathsSynchronous(PathsToScan, /*bForceRescan*/ false);
	}

	// [MOD] 클래스 필터: UMosesWeaponData
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(RootPath);

	// UE5: ClassPaths 사용
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
		// AssetData -> ObjectPath로 로드
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

	UObject* LoadedObj = Streamable.LoadSynchronous(ObjectPath, /*bManageActiveHandle*/ false);
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
