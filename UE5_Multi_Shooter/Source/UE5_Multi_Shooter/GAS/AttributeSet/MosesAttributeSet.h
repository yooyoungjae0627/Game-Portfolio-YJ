// ============================================================================
// UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h  (CLEANED)
// - Player Core Attributes + Meta(IncomingDamage/IncomingHeal)
// - Server authoritative: split damage (Shield/Health) + death routing to PlayerState SSOT
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSetBase.h"
#include "MosesAttributeSet.generated.h"

class AMosesPlayerState;

/**
 * UMosesAttributeSet (Player Core Attributes)
 *
 * 핵심 포인트
 * - Health/Shield는 Replicate (HUD/클라 표시용)
 * - IncomingDamage/IncomingHeal은 Meta (Replicate 안 함)
 * - 실제 HP/Shield 확정은 서버에서만(PostGameplayEffectExecute)
 * - 죽음 확정도 서버에서만, PlayerState(SSOT)로 라우팅
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesAttributeSet();

	//~UAttributeSet
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// -------------------------------------------------------------------------
	// Replicated Attributes (Client는 "표시/읽기" 목적)
	// -------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Attr|Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attr|Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attr|Shield", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, Shield)

	UPROPERTY(BlueprintReadOnly, Category = "Attr|Shield", ReplicatedUsing = OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, MaxShield)

	// -------------------------------------------------------------------------
	// Meta Attributes (Not replicated)
	// - GE_Damage_SetByCaller -> IncomingDamage (Data.Damage)
	// - GE_Heal_SetByCaller   -> IncomingHeal   (Data.Heal)
	// -------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Attr|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Attr|Meta")
	FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, IncomingHeal)

protected:
	// RepNotify (ASC가 클라 HUD 갱신 트리거 가능)
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

private:
	// -------------------------------------------------------------------------
	// Server authoritative apply helpers
	// -------------------------------------------------------------------------
	void ApplySplitDamage_Server(UAbilitySystemComponent* ASC, float DamageAmount);
	void ApplyHeal_Server(UAbilitySystemComponent* ASC, float HealAmount);
	void ClampVitals();

	// 죽음 확정(서버 전용) + SSOT(PlayerState) 라우팅
	void NotifyDeathIfNeeded_Server(UAbilitySystemComponent* ASC, const FGameplayEffectModCallbackData& Data);
};
