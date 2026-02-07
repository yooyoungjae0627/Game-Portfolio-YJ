// ============================================================================
// UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.cpp  (FULL - FIXED)
// ============================================================================

#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h" // for APlayerCharacter

#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace MosesAttr_Private
{
	static AController* ResolveInstigatorController(const FGameplayEffectContextHandle& Ctx)
	{
		if (AActor* InstigatorActor = Ctx.GetInstigator())
		{
			if (AController* C = Cast<AController>(InstigatorActor))
			{
				return C;
			}

			if (APawn* P = Cast<APawn>(InstigatorActor))
			{
				return P->GetController();
			}
		}

		if (AActor* CauserActor = Ctx.GetEffectCauser())
		{
			if (AController* C = Cast<AController>(CauserActor))
			{
				return C;
			}

			if (APawn* P = Cast<APawn>(CauserActor))
			{
				return P->GetController();
			}
		}

		return nullptr;
	}

	static APawn* ResolveVictimPawn_Strong(const FGameplayEffectModCallbackData& Data, AMosesPlayerState* VictimPS)
	{
		// 1) AbilityActorInfo의 Avatar가 제일 정확/안정
		if (Data.Target.AbilityActorInfo.IsValid())
		{
			if (AActor* Avatar = Data.Target.AbilityActorInfo->AvatarActor.Get())
			{
				if (APawn* P = Cast<APawn>(Avatar))
				{
					return P;
				}
			}

			// 2) PlayerController 경유
			if (AController* PC = Data.Target.AbilityActorInfo->PlayerController.Get())
			{
				if (APawn* P = PC->GetPawn())
				{
					return P;
				}
			}
		}

		// 3) PS->GetPawn (보조)
		return VictimPS ? VictimPS->GetPawn() : nullptr;
	}
}

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

	bool bTouchedVitalsThisExecute = false;

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageAmount = GetIncomingDamage();
		if (DamageAmount > 0.f)
		{
			ApplySplitDamage_Server(ASC, DamageAmount);
			bTouchedVitalsThisExecute = true;
		}
		SetIncomingDamage(0.f);
	}

	if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float HealAmount = GetIncomingHeal();
		if (HealAmount > 0.f)
		{
			ApplyHeal_Server(ASC, HealAmount);
			bTouchedVitalsThisExecute = true;
		}
		SetIncomingHeal(0.f);
	}

	ClampVitals();

	if (bTouchedVitalsThisExecute)
	{
		NotifyDeathIfNeeded_Server(ASC, Data);
	}
}

void UMosesAttributeSet::ApplySplitDamage_Server(UAbilitySystemComponent* ASC, float DamageAmount)
{
	float ShieldDamage = DamageAmount * 0.8f;
	float HealthDamage = DamageAmount - ShieldDamage;

	const float CurShield = GetShield();
	const float CurHealth = GetHealth();

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

void UMosesAttributeSet::NotifyDeathIfNeeded_Server(
	UAbilitySystemComponent* ASC,
	const FGameplayEffectModCallbackData& Data)
{
	if (!ASC)
	{
		return;
	}

	AActor* OwnerActor = ASC->GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	AMosesPlayerState* VictimPS = Cast<AMosesPlayerState>(OwnerActor);
	if (!VictimPS)
	{
		if (Data.Target.AbilityActorInfo.IsValid())
		{
			if (AController* TC = Data.Target.AbilityActorInfo->PlayerController.Get())
			{
				VictimPS = TC->GetPlayerState<AMosesPlayerState>();
			}
		}
	}

	if (!VictimPS)
	{
		UE_LOG(LogMosesHP, Warning,
			TEXT("[DEATH][SV] NotifyDeath FAIL (VictimPS NULL) Owner=%s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	if (VictimPS->IsDead())
	{
		UE_LOG(LogMosesHP, Verbose,
			TEXT("[DEATH][SV] SKIP (AlreadyDead) VictimPS=%s"),
			*GetNameSafe(VictimPS));
		return;
	}

	const float CurHP = GetHealth();
	if (CurHP > 0.f)
	{
		return;
	}

	UE_LOG(LogMosesHP, Warning,
		TEXT("[DEATH][SV] DeathConfirmed HP=%.1f VictimPS=%s"),
		CurHP,
		*GetNameSafe(VictimPS));

	// 1) SSOT Death 확정(상태/리스폰 스케줄)
	VictimPS->ServerNotifyDeathFromGAS();

	// 2) ✅ Victim Pawn 해상도 강화 -> VictimChar에서만 DeathMontage 멀티캐스트
	APawn* VictimPawn = MosesAttr_Private::ResolveVictimPawn_Strong(Data, VictimPS);
	if (APlayerCharacter* VictimChar = Cast<APlayerCharacter>(VictimPawn))
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[ANIM][DEATH][SV] Multicast Death VictimPS=%s Pawn=%s Avatar=%s"),
			*GetNameSafe(VictimPS),
			*GetNameSafe(VictimChar),
			Data.Target.AbilityActorInfo.IsValid()
			? *GetNameSafe(Data.Target.AbilityActorInfo->AvatarActor.Get())
			: TEXT("None"));

		// ✅ Pid 필터 제거 (제일 안전)
		VictimChar->Multicast_PlayDeathMontage();
	}
	else
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[ANIM][DEATH][SV] SKIP (VictimPawn invalid) VictimPS=%s Pawn=%s"),
			*GetNameSafe(VictimPS),
			*GetNameSafe(VictimPawn));
	}

	// 3) Killer PvP Kill 처리 (자살/환경사 제외)
	const FGameplayEffectContextHandle Ctx = Data.EffectSpec.GetEffectContext();
	AController* InstigatorController = MosesAttr_Private::ResolveInstigatorController(Ctx);

	AMosesPlayerState* KillerPS =
		InstigatorController
		? InstigatorController->GetPlayerState<AMosesPlayerState>()
		: nullptr;

	if (KillerPS && KillerPS != VictimPS)
	{
		KillerPS->ServerAddPvPKill(1);

		UE_LOG(LogMosesPlayer, Warning,
			TEXT("[PVP][SV] KillAwarded KillerPS=%s VictimPS=%s"),
			*GetNameSafe(KillerPS),
			*GetNameSafe(VictimPS));
	}
	else
	{
		UE_LOG(LogMosesPlayer, Warning,
			TEXT("[PVP][SV] KillAward SKIP KillerPS=%s VictimPS=%s"),
			*GetNameSafe(KillerPS),
			*GetNameSafe(VictimPS));
	}
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
