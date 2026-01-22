#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeatureAction_MosesCombatInput.generated.h"

/**
 * UGameFeatureAction_MosesCombatInput
 *
 * - GF는 “적용”을 하지 않는다.
 * - 오직 증거 로그만 남긴다.
 */
UCLASS()
class GF_COMBAT_INPUTRUNTIME_API UGameFeatureAction_MosesCombatInput : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
};
