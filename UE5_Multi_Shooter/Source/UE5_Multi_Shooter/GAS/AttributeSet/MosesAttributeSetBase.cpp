#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSetBase.h"

UMosesAttributeSet::UMosesAttributeSet()
{
}

void UMosesAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesAttributeSet, Health, OldValue);
}

void UMosesAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesAttributeSet, MaxHealth, OldValue);
}

void UMosesAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesAttributeSet, Shield, OldValue);
}

void UMosesAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesAttributeSet, MaxShield, OldValue);
}

void UMosesAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
}
