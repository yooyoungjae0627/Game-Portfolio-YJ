#include "GameFeatureAction_MosesCombatGAS.h"

#include "GFCombatGASLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

#include "UE5_Multi_Shooter/Match/GAS/MosesAbilitySet.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

void UGameFeatureAction_MosesCombatGAS::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UWorld* World = WC.World();
		if (!World || !World->IsGameWorld())
		{
			continue;
		}

		ApplyAbilitySetToWorld(World);
	}
}

void UGameFeatureAction_MosesCombatGAS::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogGFCombatGAS, Warning,
		TEXT("[GF][Combat_GAS] Deactivating (No revoke in DAY6 scope)"));

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
		return;
	}

	World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateLambda(
			[LoadedSet](AActor* Actor)
			{
				if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(Actor))
				{
					if (PS->HasAuthority())
					{
						PS->SetPendingCombatAbilitySet(LoadedSet);
					}
				}
			}
		)
	);

	// 이미 존재하는 PS도 처리
	if (AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AMosesPlayerState* MosesPS = Cast<AMosesPlayerState>(PS))
			{
				if (MosesPS->HasAuthority())
				{
					MosesPS->SetPendingCombatAbilitySet(LoadedSet);
				}
			}
		}
	}
}
