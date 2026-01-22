#include "GameFeatureAction_MosesCombatUI.h"

#include "Logging/LogMacros.h"

void UGameFeatureAction_MosesCombatUI::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF][Combat_UI] Activated"));
}

void UGameFeatureAction_MosesCombatUI::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF][Combat_UI] Deactivated"));
}
