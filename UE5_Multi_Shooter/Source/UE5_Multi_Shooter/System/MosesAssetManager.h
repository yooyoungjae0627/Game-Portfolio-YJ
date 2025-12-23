// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "MosesAssetManager.generated.h"

/**
 * UMosesAssetManager
 * - 프로젝트 전역에서 사용하는 커스텀 AssetManager
 * - 엔진 부팅 시 생성되며, 에셋 스캔 / 태그 초기화 / 공용 로딩 유틸 제공
 */

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	UMosesAssetManager();

	/** 전역 AssetManager 접근용 싱글톤 Getter */
	static UMosesAssetManager& Get();

	/**
	 * 엔진 부팅 단계에서 한 번 호출되는 초기화 함수
	 * - PrimaryAsset 스캔
	 * - 프로젝트 공용 초기화 로직 수행
	 */
	virtual void StartInitialLoading() final;

	/** -LogAssetLoads 옵션이 있을 경우 로딩 시간 로그 출력 여부 */
	static bool ShouldLogAssetLoads();

	/** SoftObjectPath 기반 동기 로딩 함수 */
	//  이 함수는 SoftObjectPath처럼 ‘경로만 알고 있는 에셋’을
	//	지금 당장 메모리에 올려서 실제 UObject로 만들어주는 동기 로딩 함수입니다.

	//	AssetManager의 StreamableManager를 통해 로딩하기 때문에
	//	엔진의 공식 로딩 흐름을 따르고, 필요하면 로딩 시간도 로그로 확인할 수 있습니다.

	//	다만 동기 로딩이라 호출하는 동안 프레임이 멈출 수 있어서,
	//	주로 게임 시작이나 로딩 단계처럼 플레이 중이 아닌 구간에서만 사용합니다.
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);

	/**
	 * GetAsset
	 * - SoftObjectPtr로 참조된 에셋을 실제 Object로 가져오는 함수
	 * - 아직 로딩되지 않았다면 동기 로딩 수행
	 * - 옵션에 따라 메모리에 계속 유지(GC 방지)
	 */
	template <typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/**
	 * GetSubclass
	 * - SoftClassPtr로 참조된 BP Class를 실제 UClass로 가져오는 함수
	 * - 아직 로딩되지 않았다면 동기 로딩 수행
	 * - 옵션에 따라 메모리에 계속 유지(GC 방지)
	 */
	template <typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/** 로딩된 에셋을 메모리에 유지하기 위한 캐시 등록 */
	void AddLoadedAsset(const UObject* Asset);

protected:
	/** GC 방지를 위한 로딩된 에셋 목록 */
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	/** 멀티스레드 접근 보호용 Lock */
	FCriticalSection SyncObject;
	
};


//GetAsset()
//├─ 이미 로딩됨 ? → 바로 반환
//├─ 아니면
//│    └─ SynchronousLoadAsset()
//└─ bKeepInMemory ?
//└─ 캐시에 등록

template <typename AssetType>
AssetType* UMosesAssetManager::GetAsset(
	const TSoftObjectPtr<AssetType>& AssetPointer,
	bool bKeepsInMemory
)
{
	AssetType* LoadedAsset = nullptr;

	// SoftObjectPtr → 실제 에셋 경로
	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();

	// 유효한 경로일 때만 처리
	if (AssetPath.IsValid())
	{
		// 이미 로딩되어 있으면 바로 사용
		LoadedAsset = AssetPointer.Get();

		// 아직 로딩되지 않았다면 동기 로딩
		if (LoadedAsset == nullptr)
		{
			LoadedAsset = Cast<AssetType>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(
				LoadedAsset,
				TEXT("Failed to load asset [%s]"),
				*AssetPointer.ToString()
			);
		}

		// 메모리에 계속 유지해야 하는 경우 캐시에 등록
		if (LoadedAsset && bKeepsInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}

		//“SoftObjectPtr로 참조만 해 두었던 에셋을
		//	처음 실제로 사용하려는 순간”에 실행되며,
		//	필요하면 그 에셋을 메모리에 계속 유지시키기 위한 처리다.
	}

	return LoadedAsset;
}

template <typename AssetType>
TSubclassOf<AssetType> UMosesAssetManager::GetSubclass(
	const TSoftClassPtr<AssetType>& AssetPointer,
	bool bKeepInMemory
)
{
	TSubclassOf<AssetType> LoadedSubclass;

	// SoftClassPtr → 실제 클래스 경로
	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();

	// 유효한 경로일 때만 처리
	if (AssetPath.IsValid())
	{
		// 이미 로딩되어 있으면 바로 사용
		LoadedSubclass = AssetPointer.Get();

		// 아직 로딩되지 않았다면 동기 로딩
		if (LoadedSubclass == nullptr)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(
				LoadedSubclass,
				TEXT("Failed to load asset class [%s]"),
				*AssetPointer.ToString()
			);
		}

		// 메모리에 계속 유지해야 하는 경우 캐시에 등록
		if (LoadedSubclass && bKeepInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}

	return LoadedSubclass;
}