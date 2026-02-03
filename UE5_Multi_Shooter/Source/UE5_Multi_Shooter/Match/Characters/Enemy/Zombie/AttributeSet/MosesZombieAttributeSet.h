#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h" // ATTRIBUTE_ACCESSORS

#include "MosesZombieAttributeSet.generated.h"

/**
 * GAS AttributeSet for Zombie
 * - Health / MaxHealth
 * - Damage(meta): GE_Damage_SetByCaller가 "Data.Damage"로 넣는 값을 받는다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesZombieAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesZombieAttributeSet();

public:
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(BlueprintReadOnly, Category="Zombie|Attr", ReplicatedUsing=OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category="Zombie|Attr", ReplicatedUsing=OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, MaxHealth)

	/** Meta Attribute */
	UPROPERTY(BlueprintReadOnly, Category="Zombie|Attr")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, Damage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
