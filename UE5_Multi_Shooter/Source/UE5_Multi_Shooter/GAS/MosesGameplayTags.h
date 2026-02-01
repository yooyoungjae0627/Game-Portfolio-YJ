#pragma once

#include "GameplayTagContainer.h"

class UGameplayTagsManager;

struct FMosesGameplayTags
{
public:
	static const FMosesGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeTags();

private:
	static void AddTag(UGameplayTagsManager& Manager, FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);
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

	FGameplayTag Weapon_Shotgun_A;          // ✅ ADD
	FGameplayTag Weapon_Sniper_A;           // ✅ ADD
	FGameplayTag Weapon_GrenadeLauncher_A;  // ✅ ADD

	FGameplayTag Weapon_Pistol;
	FGameplayTag Weapon_Grenade;

	/* ---------- GAS SetByCaller Tags ---------- */
	FGameplayTag Data_Damage;               // ✅ ADD (Data.Damage)

	/* ---------- Zombie Tags ---------- */
	FGameplayTag Zombie_Attack_A;
	FGameplayTag Zombie_Attack_B;

private:
	static FMosesGameplayTags GameplayTags;
	static bool bInitialized;
};
