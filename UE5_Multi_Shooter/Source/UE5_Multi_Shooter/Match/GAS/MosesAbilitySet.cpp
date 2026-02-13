#include "MosesAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

void FMosesAbilitySet_GrantedHandles::Reset()
{
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

void FMosesAbilitySet_GrantedHandles::RemoveFromAbilitySystem(UAbilitySystemComponent& ASC)
{
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC.ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC.RemoveActiveGameplayEffect(Handle);
		}
	}

	Reset();
}

void UMosesAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent& ASC, FMosesAbilitySet_GrantedHandles& OutGrantedHandles) const
{
	// 서버 권위만 허용
	if (!ASC.GetOwner() || !ASC.GetOwner()->HasAuthority())
	{
		return;
	}

	for (const FMosesAbilitySet_AbilityEntry& Entry : Abilities)
	{
		if (!Entry.Ability)
		{
			continue;
		}

		FGameplayAbilitySpec Spec(Entry.Ability, Entry.AbilityLevel);
		Spec.InputID = Entry.InputID;

		const FGameplayAbilitySpecHandle Handle = ASC.GiveAbility(Spec);
		OutGrantedHandles.AbilitySpecHandles.Add(Handle);
	}

	for (const FMosesAbilitySet_EffectEntry& Entry : StartupEffects)
	{
		if (!Entry.Effect)
		{
			continue;
		}

		const FGameplayEffectContextHandle Ctx = ASC.MakeEffectContext();
		const FGameplayEffectSpecHandle SpecHandle = ASC.MakeOutgoingSpec(Entry.Effect, Entry.Level, Ctx);
		if (SpecHandle.IsValid())
		{
			const FActiveGameplayEffectHandle ActiveHandle = ASC.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			OutGrantedHandles.GameplayEffectHandles.Add(ActiveHandle);
		}
	}
}
