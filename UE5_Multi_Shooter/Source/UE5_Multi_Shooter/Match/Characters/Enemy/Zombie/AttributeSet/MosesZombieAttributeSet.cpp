#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AttributeSet/MosesZombieAttributeSet.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "GameplayEffectExtension.h"

UMosesZombieAttributeSet::UMosesZombieAttributeSet()
{
}

void UMosesZombieAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);

		if (LocalDamage <= 0.f)
		{
			return;
		}

		const float NewHealth = FMath::Clamp(GetHealth() - LocalDamage, 0.f, GetMaxHealth());
		SetHealth(NewHealth);

		if (AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(GetOwningActor()))
		{
			Zombie->HandleDamageAppliedFromGAS_Server(Data, LocalDamage, NewHealth);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(1.f, GetMaxHealth()));
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}

void UMosesZombieAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesZombieAttributeSet, Health, OldValue);
}

void UMosesZombieAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMosesZombieAttributeSet, MaxHealth, OldValue);
}

void UMosesZombieAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMosesZombieAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesZombieAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	// Damage(meta)는 Rep 불필요
}
