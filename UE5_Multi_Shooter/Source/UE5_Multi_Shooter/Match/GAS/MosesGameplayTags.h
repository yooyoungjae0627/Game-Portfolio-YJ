#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

/**
 * Native GameplayTags Singleton
 * - 프로젝트 전역 GameplayTag를 1회 등록
 */
struct FMosesGameplayTags
{
public:
	static const FMosesGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeTags();

private:
	static void AddTag(
		UGameplayTagsManager& Manager,
		FGameplayTag& OutTag,
		const ANSICHAR* TagName,
		const ANSICHAR* TagComment);

	static void AddAllTags(UGameplayTagsManager& Manager);

public:

	/* ---------- Init State ---------- */
	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;

	/* ---------- Input ---------- */
	FGameplayTag InputTag_Move;
	FGameplayTag InputTag_Look_Mouse;

	/* ---------- State ---------- */
	FGameplayTag State_Phase_Combat;
	FGameplayTag State_Dead;

	/* ---------- Weapon ---------- */
	FGameplayTag Weapon_Rifle_A;
	FGameplayTag Weapon_Rifle_B;
	FGameplayTag Weapon_Rifle_C;

	FGameplayTag Weapon_Shotgun_A;
	FGameplayTag Weapon_Sniper_A;
	FGameplayTag Weapon_GrenadeLauncher_A;

	FGameplayTag Weapon_Pistol;
	FGameplayTag Weapon_Grenade;

	/* ---------- SetByCaller ---------- */
	FGameplayTag Data_Damage;
	FGameplayTag Data_Heal;
	FGameplayTag Data_AmmoCost;

	/* ---------- Zombie ---------- */
	FGameplayTag Zombie_Attack_A;
	FGameplayTag Zombie_Attack_B;

	/* ---------- Ammo Types ---------- */
	FGameplayTag Ammo_Rifle;
	FGameplayTag Ammo_Sniper;
	FGameplayTag Ammo_Grenade;

	/* ---------- Hit ---------- */
	FGameplayTag Hit_Headshot;

	/* ---------- GameplayCue ---------- */
	FGameplayTag GameplayCue_Weapon_MuzzleFlash;
	FGameplayTag GameplayCue_Weapon_HitImpact;
	FGameplayTag GameplayCue_UI_Hitmarker;

private:
	static FMosesGameplayTags GameplayTags;
	static bool bInitialized;
};
