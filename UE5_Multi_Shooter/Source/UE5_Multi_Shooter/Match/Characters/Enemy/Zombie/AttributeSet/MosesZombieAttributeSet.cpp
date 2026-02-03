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

	// Damage meta -> Health 감소
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

		// 서버에서만 죽음 처리
		if (AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(GetOwningActor()))
		{
			Zombie->HandleDamageAppliedFromGAS_Server(Data, LocalDamage, NewHealth);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Clamp 안전
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

	// Damage는 Meta Attribute라 Rep 할 필요 없음(서버에서만 사용 후 0으로 리셋)
}
