// ============================================================================
// UE5_Multi_Shooter/Match/GAS/Interfaces/MosesAmmoConsumer.h
// ============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MosesAmmoConsumer.generated.h"

struct FGameplayEffectContextHandle;

UINTERFACE(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesAmmoConsumer : public UInterface
{
	GENERATED_BODY()
};

class UE5_MULTI_SHOOTER_API IMosesAmmoConsumer
{
	GENERATED_BODY()

public:
	// 서버에서만 호출되는 것을 전제로 한다.
	virtual void ConsumeAmmoByCost_Server(float AmmoCost, const FGameplayEffectContextHandle& Context) = 0;
};
