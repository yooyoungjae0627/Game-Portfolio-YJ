#pragma once

#include "GameplayTagContainer.h"

class UGameplayTagsManager;

/**
 * FYJGameplayTags
 * - 프로젝트에서 사용하는 모든 네이티브 GameplayTag 집합
 * - 엔진 부팅 시 한 번만 등록됨
 */
struct FMosesGameplayTags
{
	/** 전역 접근 함수 */
	static const FMosesGameplayTags& Get() { return GameplayTags; }

	/** 네이티브 GameplayTag 등록 진입점 */
	static void InitializeNativeTags();

	/** 단일 태그 등록 헬퍼 */
	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

	/** 이 프로젝트에서 사용하는 모든 태그 정의 */
	void AddAllTags(UGameplayTagsManager& Manager);

	/* ---------- Init State Tags ---------- */
	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;

	/* ---------- Input Tags ---------- */
	FGameplayTag InputTag_Move;
	FGameplayTag InputTag_Look_Mouse;

private:
	/** 전역 싱글톤 인스턴스 */
	static FMosesGameplayTags GameplayTags;
};
