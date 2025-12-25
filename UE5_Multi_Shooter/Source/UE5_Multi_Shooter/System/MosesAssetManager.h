#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "MosesAssetManager.generated.h"

/**
 * UMosesAssetManager
 * - 프로젝트 전역 커스텀 AssetManager
 * - PrimaryAsset 스캔/조회, 공용 동기 로딩 유틸 제공
 *
 * 핵심:
 * - DefaultEngine.ini의 AssetManagerClassName이 이 클래스로 설정되어 있어야 함.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UMosesAssetManager();

	/** 전역 접근 (GEngine->AssetManager 캐스팅) */
	static UMosesAssetManager& Get();

	/** 엔진 부팅 시 한 번 호출 (PrimaryAsset 스캔 이후 여기서 로그 찍기 좋음) */
	virtual void StartInitialLoading() override;

	/** 커맨드라인 -LogAssetLoads 옵션이 있으면 true */
	static bool ShouldLogAssetLoads();

	/**
	 * SoftObjectPath 동기 로딩 (TryLoad 금지하고 StreamableManager로 통일)
	 * - 로딩 시간 로그도 여기서 옵션으로 출력 가능
	 */
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);

	/** (선택) 로딩한 에셋을 GC 방지용으로 캐시에 유지 */
	void AddLoadedAsset(const UObject* Asset);

public:
	/** (옵션) Experience 타입 문자열을 한 곳에서만 관리 (타입명 통일 A 해결) */
	static FPrimaryAssetType ExperienceType()
	{
		return FPrimaryAssetType(TEXT("MosesExperienceDefinition"));
	}

protected:
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	FCriticalSection SyncObject;
};
