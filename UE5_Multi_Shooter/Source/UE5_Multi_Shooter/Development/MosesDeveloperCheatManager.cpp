#include "UE5_Multi_Shooter/Development/MosesDeveloperCheatManager.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Development/Components/MosesAssetBootstrapComponent.h"

UMosesAssetBootstrapComponent* UMosesDeveloperCheatManager::FindAssetBootstrapComponentOnPawn() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s DevCheat: PlayerController null"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this));
		return nullptr;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s DevCheat: Pawn null"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this));
		return nullptr;
	}

	UMosesAssetBootstrapComponent* Comp = Pawn->FindComponentByClass<UMosesAssetBootstrapComponent>();
	if (!Comp)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s DevCheat: UMosesAssetBootstrapComponent not found (Pawn=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(Pawn));
		return nullptr;
	}

	return Comp;
}

void UMosesDeveloperCheatManager::Moses_AssetValidate()
{
	if (UMosesAssetBootstrapComponent* Comp = FindAssetBootstrapComponentOnPawn())
	{
		Comp->ValidateAssets();
		Comp->BootstrapWeapon_ServerOnly();
	}
}

void UMosesDeveloperCheatManager::Moses_Hit()
{
	if (UMosesAssetBootstrapComponent* Comp = FindAssetBootstrapComponentOnPawn())
	{
		Comp->PlayHitReact();
	}
}

void UMosesDeveloperCheatManager::Moses_Death()
{
	if (UMosesAssetBootstrapComponent* Comp = FindAssetBootstrapComponentOnPawn())
	{
		Comp->PlayDeath();
	}
}

void UMosesDeveloperCheatManager::Moses_Attack()
{
	if (UMosesAssetBootstrapComponent* Comp = FindAssetBootstrapComponentOnPawn())
	{
		Comp->PlayAttack();
	}
}
