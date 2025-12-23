// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesAssetManager.h"

//#include "Project_YJ_A/YJGameplayTags.h"
//#include "Project_YJ_A/YJLogChannels.h"

UMosesAssetManager::UMosesAssetManager()
{
}

/**
 * 전역 AssetManager 접근 함수
 * - Project Settings에서 AssetManagerClassName이
 *   UMosesAssetManager로 설정되어 있어야 정상 동작
 */
UMosesAssetManager& UMosesAssetManager::Get()
{
	check(GEngine);

	if (UMosesAssetManager* Singleton = Cast<UMosesAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	// 잘못된 설정이면 즉시 크래시
	//UE_LOG(LogYJ, Fatal, TEXT("AssetManagerClassName must be UMosesAssetManager"));
	return *NewObject<UMosesAssetManager>();
}

/**
 * 엔진 부팅 시 호출되는 AssetManager 초기화 지점
 * - 부모(UAssetManager)에서 PrimaryAsset 스캔 수행
 * - 이후 프로젝트 고유 초기화 실행
 */
void UMosesAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// ✅ Day3 DoD: AssetManager 스캔 확인 로그
	TArray<FPrimaryAssetId> FoundAssets;
	GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("YJExperienceDefinition")), FoundAssets);

	UE_LOG(LogTemp, Log, TEXT("[YJ][AssetManager] Scanned ExperienceDefinitions: %d"), FoundAssets.Num());
	for (const FPrimaryAssetId& Id : FoundAssets)
	{
		UE_LOG(LogTemp, Log, TEXT("  - %s"), *Id.ToString());
	}
}

/** 커맨드라인 옵션으로 에셋 로딩 로그 출력 여부 판단 */
bool UMosesAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

/**
 * SoftObjectPath 기반 동기 로딩
 * - AssetManager의 StreamableManager를 통해 로딩
 */
UObject* UMosesAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return nullptr;
	}

	// 로딩 시간 로그 옵션
	TUniquePtr<FScopeLogTime> LogTime;
	if (ShouldLogAssetLoads())
	{
		LogTime = MakeUnique<FScopeLogTime>(
			*FString::Printf(TEXT("Sync Load: %s"), *AssetPath.ToString()),
			nullptr,
			FScopeLogTime::ScopeLog_Seconds
		);
	}

	if (UAssetManager::IsValid())
	{
		return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath);
	}

	return AssetPath.TryLoad();
}

/**
 * 로딩된 에셋을 캐시에 등록
 * - GC 방지 목적
 * - Thread-safe
 */
void UMosesAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (!Asset)
	{
		return;
	}

	FScopeLock Lock(&SyncObject);
	LoadedAssets.Add(Asset);
}



