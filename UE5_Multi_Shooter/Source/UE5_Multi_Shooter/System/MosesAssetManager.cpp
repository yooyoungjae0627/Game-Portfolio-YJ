#include "UE5_Multi_Shooter/System/MosesAssetManager.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h"

#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

	// ini 설정이 잘못되면 여기로 떨어진다.
	checkf(false, TEXT("AssetManagerClassName is not UMosesAssetManager. Check DefaultEngine.ini"));
	// 도달 불가지만 컴파일러 경고 방지
	return *NewObject<UMosesAssetManager>();
}

void UMosesAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	InitializeMosesNativeTags();

	// ----------------------------
	// Experience 스캔 검증 로그
	// ----------------------------
	TArray<FPrimaryAssetId> FoundAssets;
	GetPrimaryAssetIdList(ExperienceType(), FoundAssets);

	UE_LOG(LogMosesExp, Log, TEXT("[AssetManager] Scanned Experience assets: %d"), FoundAssets.Num());
	for (const FPrimaryAssetId& Id : FoundAssets)
	{
		UE_LOG(LogMosesExp, Log, TEXT("  - %s"), *Id.ToString());
	}
}

bool UMosesAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

UObject* UMosesAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return nullptr;
	}

	const double Start = FPlatformTime::Seconds();

	// [MOD] UE5에서는 IsValid 대신 IsInitialized 권장
	checkf(UAssetManager::IsInitialized(), TEXT("AssetManager must be initialized when loading assets: %s"), *AssetPath.ToString());

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

void UMosesAssetManager::InitializeMosesNativeTags()
{
	// 프로젝트에 네이티브 태그 초기화가 있다면 유지
	FMosesGameplayTags::InitializeNativeTags();
}
