// ============================================================================
// UE5_Multi_Shooter/System/MosesAssetManager.cpp  (FULL - REORDERED)
// ============================================================================

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

	checkf(false, TEXT("AssetManagerClassName is not UMosesAssetManager. Check DefaultEngine.ini"));
	return *NewObject<UMosesAssetManager>();
}

// ============================================================================
// Engine boot
// ============================================================================

void UMosesAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	InitializeMosesNativeTags();

	// Experience 스캔 검증 로그
	TArray<FPrimaryAssetId> FoundAssets;
	GetPrimaryAssetIdList(ExperienceType(), FoundAssets);

	UE_LOG(LogMosesExp, Log, TEXT("[AssetManager] Scanned Experience assets: %d"), FoundAssets.Num());
	for (const FPrimaryAssetId& Id : FoundAssets)
	{
		UE_LOG(LogMosesExp, Log, TEXT("  - %s"), *Id.ToString());
	}
}

// ============================================================================
// Logging option
// ============================================================================

bool UMosesAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

// ============================================================================
// Load utilities
// ============================================================================

UObject* UMosesAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return nullptr;
	}

	const double Start = FPlatformTime::Seconds();

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

// ============================================================================
// Internal init
// ============================================================================

void UMosesAssetManager::InitializeMosesNativeTags()
{
	FMosesGameplayTags::InitializeNativeTags();
}
