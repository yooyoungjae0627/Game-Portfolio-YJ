// ============================================================================
// UE5_Multi_Shooter/System/MosesAssetManager.h  (FULL - REORDERED)
// ============================================================================

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
 * - 공용 동기 로딩 유틸 제공(로딩 방식/로그 통일)
 *
 * [필수 설정]
 * - DefaultEngine.ini
 *   [/Script/Engine.Engine]
 *   AssetManagerClassName=/Script/UE5_Multi_Shooter.MosesAssetManager
 *
 * - Experience 스캔 등록
 *   [/Script/Engine.AssetManagerSettings]
 *   +PrimaryAssetTypesToScan=(PrimaryAssetType="Experience", AssetBaseClass=/Script/UE5_Multi_Shooter.MosesExperienceDefinition, ...)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UMosesAssetManager();

	// --------------------------------------------------------------------
	// Global access
	// --------------------------------------------------------------------
	static UMosesAssetManager& Get();

	// --------------------------------------------------------------------
	// Engine boot
	// --------------------------------------------------------------------
	virtual void StartInitialLoading() override;

	// --------------------------------------------------------------------
	// Logging option
	// --------------------------------------------------------------------
	static bool ShouldLogAssetLoads();

	// --------------------------------------------------------------------
	// Load utilities
	// --------------------------------------------------------------------
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);

	// --------------------------------------------------------------------
	// Optional cache (GC prevention)
	// --------------------------------------------------------------------
	void AddLoadedAsset(const UObject* Asset);

	// --------------------------------------------------------------------
	// PrimaryAssetType helpers
	// --------------------------------------------------------------------
	static FPrimaryAssetType ExperienceType()
	{
		return FPrimaryAssetType(TEXT("Experience"));
	}

private:
	// --------------------------------------------------------------------
	// Internal init
	// --------------------------------------------------------------------
	void InitializeMosesNativeTags();

private:
	// --------------------------------------------------------------------
	// State (UPROPERTY first)
	// --------------------------------------------------------------------
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	// --------------------------------------------------------------------
	// State (non-UPROPERTY)
	// --------------------------------------------------------------------
	FCriticalSection SyncObject;
};
