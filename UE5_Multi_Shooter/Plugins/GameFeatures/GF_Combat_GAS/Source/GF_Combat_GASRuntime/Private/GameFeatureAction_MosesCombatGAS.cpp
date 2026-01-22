#include "GameFeatureAction_MosesCombatGAS.h"
#include "Logging/LogMacros.h"

void UGameFeatureAction_MosesCombatGAS::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF][Combat_GAS] Activated (Action Executed)"));
}

void UGameFeatureAction_MosesCombatGAS::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF][Combat_GAS] Deactivated (Action Executed)"));
}
