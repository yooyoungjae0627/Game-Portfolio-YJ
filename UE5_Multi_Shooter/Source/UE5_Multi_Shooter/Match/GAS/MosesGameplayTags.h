#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

/**
 * Native GameplayTags Singleton
 * - 프로젝트 전역 GameplayTag를 "엔진 초기화 초반"에 1회 등록한다.
 * - 어디서든 FMosesGameplayTags::Get().Data_Damage 형태로 사용한다.
 */
struct FMosesGameplayTags
{
public:
	/** 싱글톤 접근자 */
	static const FMosesGameplayTags& Get() { return GameplayTags; }

	/** 엔진/게임 시작 초기에 1회 호출되어 네이티브 태그 등록 */
	static void InitializeNativeTags();

private:
	/** 단일 태그 등록 헬퍼 (Manager 기반) */
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

	FGameplayTag Weapon_Shotgun_A;
	FGameplayTag Weapon_Sniper_A;
	FGameplayTag Weapon_GrenadeLauncher_A;

	FGameplayTag Weapon_Pistol;
	FGameplayTag Weapon_Grenade;

	/* ---------- GAS SetByCaller Tags ---------- */
	FGameplayTag Data_Damage; // Data.Damage
	FGameplayTag Data_Heal;   // Data.Heal

	/* ---------- Zombie Tags ---------- */
	FGameplayTag Zombie_Attack_A;
	FGameplayTag Zombie_Attack_B;

	/* ---------- Ammo Type Tags ---------- */
	FGameplayTag Ammo_Rifle;   
	FGameplayTag Ammo_Sniper;  
	FGameplayTag Ammo_Grenade; 

private:
	static FMosesGameplayTags GameplayTags;
	static bool bInitialized;
};
