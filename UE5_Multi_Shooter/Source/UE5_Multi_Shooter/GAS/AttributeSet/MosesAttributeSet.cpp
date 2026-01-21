#include "MosesAttributeSet.h"

UMosesAttributeSet::UMosesAttributeSet()
{
	// [MOD] 기본값 (서버에서 Init 시 다시 덮어써도 OK)
	Health = 100.f;
	MaxHealth = 100.f;

	Shield = 100.f;     
	MaxShield = 100.f; 
}

void UMosesAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Shield, COND_None, REPNOTIFY_Always);    
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);  
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
