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
 * DAY11:
 * - Health/MaxHealth
 * - Shield/MaxShield (== Armor 역할)
 * - IncomingDamage (Meta)
 * - IncomingHeal   (Meta)
 *
 * 서버 권위:
 * - IncomingDamage를 Shield(80%) / Health(20%)로 분배해서 실제 값을 확정한다.
 * - Heal은 Health에 Add 한다.
 * - Health <= 0 되면 서버에서 PlayerState에 "Death"를 통지한다(중복 방지).
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesAttributeSet();

	//~UAttributeSet
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override; // [MOD]

public:
	// -------------------------------------------------------------------------
	// Attributes
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
	UPROPERTY(BlueprintReadOnly, Category="Attr|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category="Attr|Meta")
	FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, IncomingHeal)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

private:
	// [MOD] 서버 권위 분배 로직
	void ApplySplitDamage_Server(UAbilitySystemComponent* ASC, float DamageAmount);
	void ApplyHeal_Server(UAbilitySystemComponent* ASC, float HealAmount);
	void ClampVitals();

	// [MOD] Death 라우팅 (서버 전용)
	void NotifyDeathIfNeeded_Server(UAbilitySystemComponent* ASC);
};
