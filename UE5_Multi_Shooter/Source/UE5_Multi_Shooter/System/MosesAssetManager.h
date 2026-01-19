#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "MosesAssetManager.generated.h"

/**
 * UMosesAssetManager
 *
 * [역할]
 * - 프로젝트 전역 커스텀 AssetManager
 * - PrimaryAsset 스캔/조회
 * - 공용 동기 로딩 유틸 제공(성능/로그 기준점)
 *
 * [필수 설정]
 * - DefaultEngine.ini에 다음이 있어야 함:
 *   [/Script/Engine.Engine]
 *   AssetManagerClassName=/Script/UE5_Multi_Shooter.MosesAssetManager
 *
 * - Experience 스캔 등록도 필요:
 *   [/Script/Engine.AssetManagerSettings]
 *   +PrimaryAssetTypesToScan=(PrimaryAssetType="Experience", AssetBaseClass=/Script/UE5_Multi_Shooter.MosesExperienceDefinition, ...)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UMosesAssetManager();

	/** 전역 접근 (GEngine->AssetManager 캐스팅) */
	static UMosesAssetManager& Get();

	/** 엔진 부팅 시 한 번 호출 */
	virtual void StartInitialLoading() override;

	/** 커맨드라인 -LogAssetLoads 옵션이 있으면 true */
	static bool ShouldLogAssetLoads();

	/**
	 * SoftObjectPath 동기 로딩 유틸
	 * - 내부적으로 StreamableManager를 사용하여 로딩 방식 통일
	 */
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);

	/** (옵션) 로딩한 에셋을 GC 방지용으로 캐시에 유지 */
	void AddLoadedAsset(const UObject* Asset);

	/** [MOD] Experience PrimaryAssetType 통일 */
	static FPrimaryAssetType ExperienceType()
	{
		// [MOD] ExperienceDefinition::GetPrimaryAssetId()와 반드시 동일해야 한다.
		return FPrimaryAssetType(TEXT("Experience"));
	}

private:
	void InitializeMosesNativeTags();

private:
	/** 로딩된 에셋을 잡아두는 캐시(옵션) */
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	FCriticalSection SyncObject;
};
