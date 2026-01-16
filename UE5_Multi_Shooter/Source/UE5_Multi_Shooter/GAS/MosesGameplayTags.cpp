// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesGameplayTags.h"
#include "GameplayTagsManager.h"

/** 싱글톤 인스턴스 정의 */
FMosesGameplayTags FMosesGameplayTags::GameplayTags;
bool FMosesGameplayTags::bInitialized = false;

/**
 * 엔진 부팅 시 호출되어
 * 모든 네이티브 GameplayTag를 등록
 */
void FMosesGameplayTags::InitializeNativeTags()
{
	// Native GameplayTags는 "엔진 초기화 초반"에만 추가 가능.
	// GameInstance::Init() 시점은 이미 bDoneAddingNativeTags=true일 수 있어 ensure로 터짐.
	// 따라서 AssetManager::StartInitialLoading(or StartupModule)에서 1회만 초기화한다.
	if (bInitialized)
	{
		return;
	}

	bInitialized = true;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	GameplayTags.AddAllTags(Manager);
}

/**
 * 단일 네이티브 GameplayTag 등록
 * - 문자열 기반 태그를 엔진에 정식 등록
 */
void FMosesGameplayTags::AddTag(UGameplayTagsManager& Manager, FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = Manager.AddNativeGameplayTag(
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
	AddTag(Manager, InitState_Spawned, "InitState.Spawned", "Actor just spawned");
	AddTag(Manager, InitState_DataAvailable, "InitState.DataAvailable", "Required data is ready");
	AddTag(Manager, InitState_DataInitialized, "InitState.DataInitialized", "Internal initialization done");
	AddTag(Manager, InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");

	// 입력 의미 태그
	AddTag(Manager, InputTag_Move, "InputTag.Move", "Move input");
	AddTag(Manager, InputTag_Look_Mouse, "InputTag.Look.Mouse", "Mouse look input");

	// --- Combat / Dead 정책 태그
	AddTag(Manager, State_Phase_Combat, "State.Phase.Combat", "In combat phase (server authoritative)");
	AddTag(Manager, State_Dead, "State.Dead", "Dead state (server authoritative)");
}



