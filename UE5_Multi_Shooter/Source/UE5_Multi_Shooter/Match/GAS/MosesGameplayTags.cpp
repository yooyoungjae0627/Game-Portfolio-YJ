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

void FMosesGameplayTags::AddTag(
	UGameplayTagsManager& Manager,
	FGameplayTag& OutTag,
	const ANSICHAR* TagName,
	const ANSICHAR* TagComment)
{
	OutTag = Manager.AddNativeGameplayTag(
		FName(TagName),
		FString(TEXT("(Native) ")) + UTF8_TO_TCHAR(TagComment));

#if !UE_BUILD_SHIPPING
	if (!OutTag.IsValid())
	{
		UE_LOG(LogMosesGAS, Error,
			TEXT("[TAGS][INIT] AddNativeGameplayTag FAILED Tag=%s"),
			UTF8_TO_TCHAR(TagName));
	}
#endif
}

void FMosesGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	auto& Tags = GameplayTags;

	// InitState
	AddTag(Manager, Tags.InitState_Spawned, "InitState.Spawned", "Actor just spawned");
	AddTag(Manager, Tags.InitState_DataAvailable, "InitState.DataAvailable", "Required data is ready");
	AddTag(Manager, Tags.InitState_DataInitialized, "InitState.DataInitialized", "Internal initialization done");
	AddTag(Manager, Tags.InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");

	// Input
	AddTag(Manager, Tags.InputTag_Move, "InputTag.Move", "Move input");
	AddTag(Manager, Tags.InputTag_Look_Mouse, "InputTag.Look.Mouse", "Mouse look input");

	// State
	AddTag(Manager, Tags.State_Phase_Combat, "State.Phase.Combat", "In combat phase");
	AddTag(Manager, Tags.State_Dead, "State.Dead", "Dead state");

	// Weapon
	AddTag(Manager, Tags.Weapon_Rifle_A, "Weapon.Rifle.A", "Rifle A");
	AddTag(Manager, Tags.Weapon_Rifle_B, "Weapon.Rifle.B", "Rifle B");
	AddTag(Manager, Tags.Weapon_Rifle_C, "Weapon.Rifle.C", "Rifle C");

	AddTag(Manager, Tags.Weapon_Shotgun_A, "Weapon.Shotgun.A", "Shotgun A");
	AddTag(Manager, Tags.Weapon_Sniper_A, "Weapon.Sniper.A", "Sniper A");
	AddTag(Manager, Tags.Weapon_GrenadeLauncher_A, "Weapon.GrenadeLauncher.A", "GrenadeLauncher A");

	AddTag(Manager, Tags.Weapon_Pistol, "Weapon.Pistol", "Pistol");
	AddTag(Manager, Tags.Weapon_Grenade, "Weapon.Grenade", "Grenade");

	// SetByCaller
	AddTag(Manager, Tags.Data_Damage, "Data.Damage", "SetByCaller damage");
	AddTag(Manager, Tags.Data_Heal, "Data.Heal", "SetByCaller heal");
	AddTag(Manager, Tags.Data_AmmoCost, "Data.AmmoCost", "SetByCaller ammo cost");

	// Zombie
	AddTag(Manager, Tags.Zombie_Attack_A, "Zombie.Attack.A", "Zombie attack A");
	AddTag(Manager, Tags.Zombie_Attack_B, "Zombie.Attack.B", "Zombie attack B");

	// Ammo Types
	AddTag(Manager, Tags.Ammo_Rifle, "Ammo.Rifle", "Rifle ammo");
	AddTag(Manager, Tags.Ammo_Sniper, "Ammo.Sniper", "Sniper ammo");
	AddTag(Manager, Tags.Ammo_Grenade, "Ammo.Grenade", "Grenade ammo");

	// Hit
	AddTag(Manager, Tags.Hit_Headshot, "Hit.Headshot", "Headshot marker");

	// GameplayCue
	AddTag(Manager, Tags.GameplayCue_Weapon_MuzzleFlash, "GameplayCue.Weapon.MuzzleFlash", "Muzzle flash");
	AddTag(Manager, Tags.GameplayCue_Weapon_HitImpact, "GameplayCue.Weapon.HitImpact", "Hit impact");
	AddTag(Manager, Tags.GameplayCue_UI_Hitmarker, "GameplayCue.UI.Hitmarker", "Hitmarker UI");
}
