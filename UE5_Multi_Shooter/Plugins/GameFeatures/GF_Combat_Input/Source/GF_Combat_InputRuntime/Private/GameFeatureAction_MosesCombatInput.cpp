#include "GameFeatureAction_MosesCombatInput.h"
#include "Logging/LogMacros.h"

void UGameFeatureAction_MosesCombatInput::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF][Combat_Input] Activated (Action Executed)"));
}

void UGameFeatureAction_MosesCombatInput::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF][Combat_Input] Deactivated (Action Executed)"));
}
