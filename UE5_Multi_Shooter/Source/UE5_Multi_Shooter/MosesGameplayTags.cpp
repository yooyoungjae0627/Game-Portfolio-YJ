// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesGameplayTags.h"
#include "GameplayTagsManager.h"

/** 싱글톤 인스턴스 정의 */
FMosesGameplayTags FMosesGameplayTags::GameplayTags;

/**
 * 엔진 부팅 시 호출되어
 * 모든 네이티브 GameplayTag를 등록
 */
void FMosesGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	GameplayTags.AddAllTags(Manager);
}

/**
 * 단일 네이티브 GameplayTag 등록
 * - 문자열 기반 태그를 엔진에 정식 등록
 */
void FMosesGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TagName),
		FString(TEXT("(Native) ")) + FString(TagComment)
	);
}

/**
 * 프로젝트에서 사용할 모든 GameplayTag 정의
 */
void FMosesGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	// 초기화 상태 태그
	AddTag(InitState_Spawned, "InitState.Spawned", "Actor just spawned");
	AddTag(InitState_DataAvailable, "InitState.DataAvailable", "Required data is ready");
	AddTag(InitState_DataInitialized, "InitState.DataInitialized", "Internal initialization done");
	AddTag(InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");

	// 입력 의미 태그
	AddTag(InputTag_Move, "InputTag.Move", "Move input");
	AddTag(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Mouse look input");
}



