#include "MosesAssetManager.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesAssetManager::UMosesAssetManager()
{
}

UMosesAssetManager& UMosesAssetManager::Get()
{
	check(GEngine);

	if (UMosesAssetManager* Singleton = Cast<UMosesAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	// 여기로 오면 ini 설정이 잘못된 것.
	// 개발 단계에서 빨리 죽는 게 원인 파악이 쉬움.
	checkf(false, TEXT("AssetManagerClassName is not UMosesAssetManager. Check DefaultEngine.ini"));
	return *NewObject<UMosesAssetManager>();
}

void UMosesAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// 스캔 확인 로그 (A 검증용)
	TArray<FPrimaryAssetId> FoundAssets;
	GetPrimaryAssetIdList(ExperienceType(), FoundAssets);

	UE_LOG(LogMosesExp, Log, TEXT("[AssetManager] Scanned ExperienceDefinitions: %d"), FoundAssets.Num());
	for (const FPrimaryAssetId& Id : FoundAssets)
	{
		UE_LOG(LogMosesExp, Log, TEXT("  - %s"), *Id.ToString());
	}
}

bool UMosesAssetManager::ShouldLogAssetLoads()
{
	// 커맨드라인에 -LogAssetLoads 가 있으면 true
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

UObject* UMosesAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return nullptr;
	}

	// 옵션: 로딩 시간 측정 로그
	const double Start = FPlatformTime::Seconds();

	checkf(UAssetManager::IsValid(), TEXT("AssetManager must be valid when loading assets: %s"), *AssetPath.ToString());

	UObject* Loaded = UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath);

	if (ShouldLogAssetLoads())
	{
		const double End = FPlatformTime::Seconds();
		UE_LOG(LogMosesExp, Log, TEXT("[AssetLoad] SyncLoad %s (%.3f sec)"),
			*AssetPath.ToString(),
			(End - Start));
	}

	return Loaded;
}

void UMosesAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (!Asset)
	{
		return;
	}

	FScopeLock Lock(&SyncObject);
	LoadedAssets.Add(Asset);
}
