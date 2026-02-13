#include "GameFeatureAction_MosesCombatGAS.h"

#include "GFCombatGASLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

#include "UE5_Multi_Shooter/Match/GAS/MosesAbilitySet.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/GAS/Components/MosesAbilitySystemComponent.h"

void UGameFeatureAction_MosesCombatGAS::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	UE_LOG(LogGFCombatGAS, Warning, TEXT("[GF][Combat_GAS] Activating"));

	if (!GEngine)
	{
		return;
	}

	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UWorld* World = WC.World();
		if (!World || !World->IsGameWorld())
		{
			continue;
		}

		// 서버에서만 부여
		if (World->GetNetMode() == NM_Client)
		{
			continue;
		}

		ApplyAbilitySetToWorld(World);
	}
}

void UGameFeatureAction_MosesCombatGAS::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogGFCombatGAS, Warning, TEXT("[GF][Combat_GAS] Deactivating (No revoke in DAY6 scope)"));
	Super::OnGameFeatureDeactivating(Context);
}

void UGameFeatureAction_MosesCombatGAS::ApplyAbilitySetToWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	UMosesAbilitySet* LoadedSet = CombatAbilitySet.LoadSynchronous();
	if (!LoadedSet)
	{
		UE_LOG(LogGFCombatGAS, Warning, TEXT("[GF][Combat_GAS] CombatAbilitySet Load FAILED"));
		return;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		UE_LOG(LogGFCombatGAS, Warning, TEXT("[GF][Combat_GAS] GameState is null"));
		return;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		AMosesPlayerState* MosesPS = Cast<AMosesPlayerState>(PS);
		if (!MosesPS)
		{
			continue;
		}

		if (!MosesPS->HasAuthority())
		{
			continue;
		}

		UMosesAbilitySystemComponent* ASC = Cast<UMosesAbilitySystemComponent>(MosesPS->GetAbilitySystemComponent());
		if (!ASC)
		{
			UE_LOG(LogGFCombatGAS, Warning, TEXT("[GF][Combat_GAS] No ASC on PS=%s"), *GetNameSafe(MosesPS));
			continue;
		}

		MosesPS->ServerApplyCombatAbilitySetOnce(LoadedSet);

		UE_LOG(LogGFCombatGAS, Warning, TEXT("[GF][Combat_GAS] AbilitySet Applied PS=%s Set=%s"),
			*GetNameSafe(MosesPS), *GetNameSafe(LoadedSet));
	}
}
