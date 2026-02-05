#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h" // FGameplayEffectModCallbackData

UMosesAttributeSet::UMosesAttributeSet()
{
}

void UMosesAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMosesAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
}

void UMosesAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	UAbilitySystemComponent* ASC = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid())
	{
		ASC = Data.Target.AbilityActorInfo->AbilitySystemComponent.Get();
	}

	if (!ASC)
	{
		return;
	}

	AActor* OwnerActor = ASC->GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	// [MOD] Damage 분배
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageAmount = GetIncomingDamage();
		if (DamageAmount > 0.f)
		{
			ApplySplitDamage_Server(ASC, DamageAmount);
		}
		SetIncomingDamage(0.f);
	}

	// [MOD] Heal 적용
	if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float HealAmount = GetIncomingHeal();
		if (HealAmount > 0.f)
		{
			ApplyHeal_Server(ASC, HealAmount);
		}
		SetIncomingHeal(0.f);
	}

	ClampVitals();

	// [MOD] Death 감지 후 PS로 라우팅
	NotifyDeathIfNeeded_Server(ASC);
}

void UMosesAttributeSet::ApplySplitDamage_Server(UAbilitySystemComponent* ASC, float DamageAmount)
{
	// 규칙: Shield(Armor) 80% / Health 20%
	float ShieldDamage = DamageAmount * 0.8f;
	float HealthDamage = DamageAmount - ShieldDamage;

	const float CurShield = GetShield();
	const float CurHealth = GetHealth();

	// Shield 부족 시 overflow를 Health로 이동
	if (ShieldDamage > CurShield)
	{
		const float Overflow = ShieldDamage - CurShield;
		ShieldDamage = CurShield;
		HealthDamage += Overflow;
	}

	const float NewShield = FMath::Max(0.f, CurShield - ShieldDamage);
	const float NewHealth = FMath::Max(0.f, CurHealth - HealthDamage);

	SetShield(NewShield);
	SetHealth(NewHealth);

	UE_LOG(LogMosesHP, Warning,
		TEXT("[ARMOR][SV] SplitDamage Damage=%.1f Shield-%.1f HP-%.1f -> Shield=%.1f HP=%.1f Owner=%s"),
		DamageAmount, ShieldDamage, HealthDamage, NewShield, NewHealth, *GetNameSafe(ASC->GetOwner()));
}

void UMosesAttributeSet::ApplyHeal_Server(UAbilitySystemComponent* ASC, float HealAmount)
{
	const float Cur = GetHealth();
	const float Max = GetMaxHealth();

	const float NewHealth = FMath::Min(Max, Cur + HealAmount);
	SetHealth(NewHealth);

	UE_LOG(LogMosesPickup, Warning,
		TEXT("[HP][SV] Heal Amount=%.0f -> HP=%.0f/%.0f Owner=%s"),
		HealAmount, NewHealth, Max, *GetNameSafe(ASC->GetOwner()));
}

void UMosesAttributeSet::ClampVitals()
{
	SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
}

void UMosesAttributeSet::NotifyDeathIfNeeded_Server(UAbilitySystemComponent* ASC)
{
	if (!ASC || !ASC->GetOwner() || !ASC->GetOwner()->HasAuthority())
	{
		return;
	}

	if (GetHealth() > 0.f)
	{
		return;
	}

	AMosesPlayerState* PS = Cast<AMosesPlayerState>(ASC->GetOwner());
	if (!PS)
	{
		return;
	}

	// PS 쪽에서 중복 방지/리스폰 스케줄까지 담당
	PS->ServerNotifyDeathFromGAS();
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
