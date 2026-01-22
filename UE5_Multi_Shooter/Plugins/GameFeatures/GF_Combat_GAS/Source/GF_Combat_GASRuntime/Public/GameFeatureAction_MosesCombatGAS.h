#pragma once

#include "GameFeatureAction.h"
#include "GameFeatureAction_MosesCombatGAS.generated.h"

/**
 * UGameFeatureAction_MosesCombatGAS
 *
 * - GF는 “적용”을 하지 않는다.
 * - 오직 증거 로그만 남긴다.
 */
UCLASS()
class GF_COMBAT_GASRUNTIME_API UGameFeatureAction_MosesCombatGAS : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
};
