#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameplayTagsManager.h"

FMosesGameplayTags FMosesGameplayTags::GameplayTags;
bool FMosesGameplayTags::bInitialized = false;

void FMosesGameplayTags::InitializeNativeTags()
{
	if (bInitialized)
	{
		return;
	}

	bInitialized = true;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	AddAllTags(Manager);

	UE_LOG(LogMosesGAS, Warning, TEXT("[TAGS][INIT] Native GameplayTags Initialized"));
}

void FMosesGameplayTags::AddTag(UGameplayTagsManager& Manager, FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = Manager.AddNativeGameplayTag(
		FName(TagName),
		FString(TEXT("(Native) ")) + FString(TagComment)
	);

#if !UE_BUILD_SHIPPING
	if (!OutTag.IsValid())
	{
		UE_LOG(LogMosesGAS, Error, TEXT("[TAGS][INIT] AddNativeGameplayTag FAILED Tag=%s Comment=%s"),
			UTF8_TO_TCHAR(TagName), UTF8_TO_TCHAR(TagComment));
	}
#endif
}

void FMosesGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	// InitState
	AddTag(Manager, GameplayTags.InitState_Spawned, "InitState.Spawned", "Actor just spawned");
	AddTag(Manager, GameplayTags.InitState_DataAvailable, "InitState.DataAvailable", "Required data is ready");
	AddTag(Manager, GameplayTags.InitState_DataInitialized, "InitState.DataInitialized", "Internal initialization done");
	AddTag(Manager, GameplayTags.InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");

	// Input
	AddTag(Manager, GameplayTags.InputTag_Move, "InputTag.Move", "Move input");
	AddTag(Manager, GameplayTags.InputTag_Look_Mouse, "InputTag.Look.Mouse", "Mouse look input");

	// State
	AddTag(Manager, GameplayTags.State_Phase_Combat, "State.Phase.Combat", "In combat phase (server authoritative)");
	AddTag(Manager, GameplayTags.State_Dead, "State.Dead", "Dead state (server authoritative)");

	// Weapons (DataAsset key)
	AddTag(Manager, GameplayTags.Weapon_Rifle_A, "Weapon.Rifle.A", "Rifle A (DataAsset key)");
	AddTag(Manager, GameplayTags.Weapon_Rifle_B, "Weapon.Rifle.B", "Rifle B (DataAsset key)");
	AddTag(Manager, GameplayTags.Weapon_Rifle_C, "Weapon.Rifle.C", "Rifle C (DataAsset key)");

	AddTag(Manager, GameplayTags.Weapon_Shotgun_A, "Weapon.Shotgun.A", "Shotgun A (DataAsset key)");
	AddTag(Manager, GameplayTags.Weapon_Sniper_A, "Weapon.Sniper.A", "Sniper A (DataAsset key)");
	AddTag(Manager, GameplayTags.Weapon_GrenadeLauncher_A, "Weapon.GrenadeLauncher.A", "GrenadeLauncher A (DataAsset key)");

	AddTag(Manager, GameplayTags.Weapon_Pistol, "Weapon.Pistol", "Pistol (DataAsset key)");
	AddTag(Manager, GameplayTags.Weapon_Grenade, "Weapon.Grenade", "Grenade (DataAsset key)");

	// GAS SetByCaller
	AddTag(Manager, GameplayTags.Data_Damage, "Data.Damage", "SetByCaller damage");
	AddTag(Manager, GameplayTags.Data_Heal, "Data.Heal", "SetByCaller heal amount");

	// Zombie
	AddTag(Manager, GameplayTags.Zombie_Attack_A, "Zombie.Attack.A", "Zombie attack type A");
	AddTag(Manager, GameplayTags.Zombie_Attack_B, "Zombie.Attack.B", "Zombie attack type B");

	// Ammo Types 
	AddTag(Manager, GameplayTags.Ammo_Rifle, "Ammo.Rifle", "Rifle ammo type");
	AddTag(Manager, GameplayTags.Ammo_Sniper, "Ammo.Sniper", "Sniper ammo type");
	AddTag(Manager, GameplayTags.Ammo_Grenade, "Ammo.Grenade", "Grenade ammo type");

	// Hit
	AddTag(Manager, GameplayTags.Hit_Headshot, "Hit.Headshot", "Headshot marker tag");
}
