#include "MosesAttributeSet.h"

#include "Net/UnrealNetwork.h"

UMosesAttributeSet::UMosesAttributeSet()
{
	Health = 100.f;
	MaxHealth = 100.f;
}

void UMosesAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UMosesAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesAttributeSet, Health, OldValue);
}

void UMosesAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesAttributeSet, MaxHealth, OldValue);
}
