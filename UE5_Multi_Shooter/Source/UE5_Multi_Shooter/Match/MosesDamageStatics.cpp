#include "UE5_Multi_Shooter/Match/MosesDamageStatics.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"

UAbilitySystemComponent* FMosesDamageStatics::FindASC(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (Actor->GetClass()->ImplementsInterface(UAbilitySystemInterface::StaticClass()))
	{
		IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
		return ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	}

	// 폴백: 컴포넌트 탐색
	return Actor->FindComponentByClass<UAbilitySystemComponent>();
}

bool FMosesDamageStatics::ApplyDamageEffect_Server(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	const float DamageAmount)
{
	if (!SourceASC || !TargetASC || !DamageEffectClass)
	{
		return false;
	}

	const FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, /*Level*/ 1.0f, Context);

	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Damage, DamageAmount);

	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}
