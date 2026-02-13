#pragma once

#include "CoreMinimal.h"

// ============================================================================
// [FIX]
// - UE 헤더 경로는 "GameFeatures/GameFeatureAction.h" 가 아니라
//   "GameFeatureAction.h" 이다.
// - GameFeatures 모듈(Build.cs)에 의존성이 들어가 있으면 이 include가 정상 동작한다.
// ============================================================================
#include "GameFeatureAction.h"

#include "GameFeatureAction_MosesCombatGAS.generated.h"

class UMosesAbilitySet;

/**
 * UGameFeatureAction_MosesCombatGAS
 *
 * [역할]
 * - GF_Combat_GAS 활성화 시점에 Combat AbilitySet을 서버에서 ASC에 부여한다.
 *
 * [주의]
 * - GF는 "정책/스위치 레이어"이며, 전투 로직(발사/탄약/판정)은 절대 포함하지 않는다.
 * - 실제 판정/SSOT는 UE5_Multi_Shooter 쪽(PlayerState/CombatComponent)에 있다.
 *
 * [서버 권위]
 * - AbilitySet 부여는 서버에서만 수행한다.
 */
UCLASS()
class GF_COMBAT_GASRUNTIME_API UGameFeatureAction_MosesCombatGAS : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS")
	TSoftObjectPtr<UMosesAbilitySet> CombatAbilitySet;

private:
	void ApplyAbilitySetToWorld(UWorld* World);
};
