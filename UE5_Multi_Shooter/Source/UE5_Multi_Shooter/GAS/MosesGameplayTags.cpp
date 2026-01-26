#include "MosesGameplayTags.h"
#include "GameplayTagsManager.h"

// (선택) 로그 넣고 싶으면 여기에 MosesLogChannels include
// #include "UE5_Multi_Shooter/MosesLogChannels.h"

/** 싱글톤 인스턴스 정의 */
FMosesGameplayTags FMosesGameplayTags::GameplayTags;
bool FMosesGameplayTags::bInitialized = false;

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
	AddAllTags(Manager);

	// (선택) 증거 로그
	// UE_LOG(LogMosesAsset, Log, TEXT("[TAGS] Native GameplayTags initialized"));
}

void FMosesGameplayTags::AddTag(UGameplayTagsManager& Manager, FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = Manager.AddNativeGameplayTag(
		FName(TagName),
		FString(TEXT("(Native) ")) + FString(TagComment)
	);
}

void FMosesGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	// 초기화 상태 태그
	AddTag(Manager, GameplayTags.InitState_Spawned, "InitState.Spawned", "Actor just spawned");
	AddTag(Manager, GameplayTags.InitState_DataAvailable, "InitState.DataAvailable", "Required data is ready");
	AddTag(Manager, GameplayTags.InitState_DataInitialized, "InitState.DataInitialized", "Internal initialization done");
	AddTag(Manager, GameplayTags.InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");

	// 입력 의미 태그
	AddTag(Manager, GameplayTags.InputTag_Move, "InputTag.Move", "Move input");
	AddTag(Manager, GameplayTags.InputTag_Look_Mouse, "InputTag.Look.Mouse", "Mouse look input");

	// Combat / Dead 정책 태그
	AddTag(Manager, GameplayTags.State_Phase_Combat, "State.Phase.Combat", "In combat phase (server authoritative)");
	AddTag(Manager, GameplayTags.State_Dead, "State.Dead", "Dead state (server authoritative)");

	// ------------------------------
	// Weapon Tags
	// ------------------------------
	AddTag(Manager, GameplayTags.Weapon_Rifle_A, "Weapon.Rifle.A", "Rifle A (DataAsset key)");    
	AddTag(Manager, GameplayTags.Weapon_Rifle_B, "Weapon.Rifle.B", "Rifle B (DataAsset key)");    
	AddTag(Manager, GameplayTags.Weapon_Rifle_C, "Weapon.Rifle.C", "Rifle C (DataAsset key)");    
	AddTag(Manager, GameplayTags.Weapon_Pistol, "Weapon.Pistol", "Pistol (DataAsset key)");      
	AddTag(Manager, GameplayTags.Weapon_Grenade, "Weapon.Grenade", "Grenade (DataAsset key)");    

	// ------------------------------
	// Zombie Tags (optional)
	// ------------------------------
	AddTag(Manager, GameplayTags.Zombie_Attack_A, "Zombie.Attack.A", "Zombie attack type A");      
	AddTag(Manager, GameplayTags.Zombie_Attack_B, "Zombie.Attack.B", "Zombie attack type B");      
}
