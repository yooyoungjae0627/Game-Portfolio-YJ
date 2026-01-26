#pragma once

#include "GameplayTagContainer.h"

class UGameplayTagsManager;

/**
 * Native GameplayTags Singleton
 * - 프로젝트 전역에서 사용할 네이티브 GameplayTag를 "엔진 초기화 초반"에 1회 등록한다.
 *
 * Lyra 스타일:
 * - "태그 문자열"은 코드에 상수로 고정(SSOT)
 * - 어디서든 FMosesGameplayTags::Get().Weapon_Rifle_A 형태로 사용
 */
struct FMosesGameplayTags
{
public:
	/** 싱글톤 접근자 */
	static const FMosesGameplayTags& Get() { return GameplayTags; }

	/** 엔진 부팅 초기에 1회 호출되어 네이티브 태그 등록 */
	static void InitializeNativeTags();

private:
	/** 단일 태그 등록 헬퍼 (Manager 기반으로만 등록한다) */
	static void AddTag(UGameplayTagsManager& Manager, FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

	/** 프로젝트에서 사용할 모든 GameplayTag 정의 */
	static void AddAllTags(UGameplayTagsManager& Manager);

public:
	/* ---------- Init State Tags ---------- */
	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;

	/* ---------- Input Tags ---------- */
	FGameplayTag InputTag_Move;
	FGameplayTag InputTag_Look_Mouse;

	/* ---------- Combat State Tags ---------- */
	FGameplayTag State_Phase_Combat;
	FGameplayTag State_Dead;

	/* ---------- Weapon Tags ---------- */
	FGameplayTag Weapon_Rifle_A;     
	FGameplayTag Weapon_Rifle_B;     
	FGameplayTag Weapon_Rifle_C;     
	FGameplayTag Weapon_Pistol;      
	FGameplayTag Weapon_Grenade;     

	/* ---------- Zombie Tags ---------- */
	FGameplayTag Zombie_Attack_A;    
	FGameplayTag Zombie_Attack_B;    

private:
	/** 전역 싱글톤 인스턴스 */
	static FMosesGameplayTags GameplayTags;

	/** 중복 호출 방지 가드 */
	static bool bInitialized;
};
