#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "MosesAbilitySystemComponent.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void DumpOwnedTagsToLog() const;
};
