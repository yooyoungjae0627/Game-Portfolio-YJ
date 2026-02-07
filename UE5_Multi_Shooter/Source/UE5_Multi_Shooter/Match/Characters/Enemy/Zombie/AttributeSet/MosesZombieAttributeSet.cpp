#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AttributeSet/MosesZombieAttributeSet.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h" 

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Actor.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"

UMosesZombieAttributeSet::UMosesZombieAttributeSet()
{
	// [MOD] 안전 기본값 (InitAttr가 늦거나 누락돼도 0/0 clamp로 꼬임 방지)
	MaxHealth.SetBaseValue(100.f);
	MaxHealth.SetCurrentValue(100.f);

	Health.SetBaseValue(100.f);
	Health.SetCurrentValue(100.f);

	Damage.SetBaseValue(0.f);
	Damage.SetCurrentValue(0.f);
}

void UMosesZombieAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AActor* OwningActor = GetOwningActor();
	if (!OwningActor)
	{
		return;
	}

	const bool bAuth = OwningActor->HasAuthority();

	// ------------------------------------------------------------------------
	// Damage(meta) -> Health 감소
	// ------------------------------------------------------------------------
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);

		if (LocalDamage <= 0.f)
		{
			return;
		}

		const float OldHealth = GetHealth();

		const float SafeMaxHealth = FMath::Max(1.f, GetMaxHealth());
		if (SafeMaxHealth != GetMaxHealth())
		{
			SetMaxHealth(SafeMaxHealth);
		}

		const float NewHealth = FMath::Clamp(OldHealth - LocalDamage, 0.f, SafeMaxHealth);
		SetHealth(NewHealth);

		if (bAuth)
		{
			LogDamageContext_Server(Data, LocalDamage, OldHealth, NewHealth);

			// ✅ [MOD] 여기서 좀비 캐릭터로 “데미지 적용됨” 콜백 전달
			NotifyZombieDamageApplied_Server(Data, LocalDamage, NewHealth);
		}

		return;
	}

	// ------------------------------------------------------------------------
	// Health/MaxHealth 보정
	// ------------------------------------------------------------------------
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float SafeMaxHealth = FMath::Max(1.f, GetMaxHealth());
		SetHealth(FMath::Clamp(GetHealth(), 0.f, SafeMaxHealth));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(1.f, GetMaxHealth()));
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		return;
	}
}

void UMosesZombieAttributeSet::NotifyZombieDamageApplied_Server(const FGameplayEffectModCallbackData& Data, float LocalDamage, float NewHealth) const
{
	AActor* OwningActor = GetOwningActor();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	if (AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(OwningActor))
	{
		Zombie->HandleDamageAppliedFromGAS_Server(Data, LocalDamage, NewHealth);
	}
}

void UMosesZombieAttributeSet::LogDamageContext_Server(const FGameplayEffectModCallbackData& Data, float LocalDamage, float OldHealth, float NewHealth) const
{
	const AActor* TargetActor = GetOwningActor();
	const AActor* InstigatorActor = Data.EffectSpec.GetContext().GetOriginalInstigator();
	const AActor* CauserActor = Data.EffectSpec.GetContext().GetEffectCauser();
	const FString GEName = Data.EffectSpec.Def ? Data.EffectSpec.Def->GetName() : TEXT("None");

	UE_LOG(LogMosesZombie, Warning,
		TEXT("[ZOMBIE][HP][SV] Hit Damage=%.1f HP %.1f -> %.1f / %.1f Target=%s Instigator=%s Causer=%s GE=%s"),
		LocalDamage,
		OldHealth,
		NewHealth,
		FMath::Max(1.f, GetMaxHealth()),
		*GetNameSafe(TargetActor),
		*GetNameSafe(InstigatorActor),
		*GetNameSafe(CauserActor),
		*GEName);
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
