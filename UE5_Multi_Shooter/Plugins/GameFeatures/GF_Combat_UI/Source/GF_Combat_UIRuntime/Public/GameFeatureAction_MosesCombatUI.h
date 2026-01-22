#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeatureAction_MosesCombatUI.generated.h"

/**
 * - GF는 적용(UI 생성 등)을 하지 않는다.
 * - 오직 증거 로그만 남긴다.
 */
UCLASS()
class GF_COMBAT_UIRUNTIME_API UGameFeatureAction_MosesCombatUI : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
};
