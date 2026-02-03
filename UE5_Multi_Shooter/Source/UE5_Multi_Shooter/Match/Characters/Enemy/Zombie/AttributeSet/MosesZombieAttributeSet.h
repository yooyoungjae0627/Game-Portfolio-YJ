#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h" // [MOD] Rep 관련 안정성

#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h" // ATTRIBUTE_ACCESSORS

#include "MosesZombieAttributeSet.generated.h"

/**
 * GAS AttributeSet for Zombie
 * - Health / MaxHealth
 * - Damage(메타) : GE_Damage_SetByCaller가 "Data.Damage"로 넣는 값을 받는다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesZombieAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesZombieAttributeSet();

public:
	//~UAttributeSet interface
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** Current Health */
	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attr", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, Health)

		/** Max Health */
		UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attr", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, MaxHealth)

		/**
		 * Incoming Damage (meta attribute)
		 * - GE에서 SetByCaller(Data.Damage) -> Damage에 적용
		 * - PostGameplayEffectExecute에서 Health 감소 처리 후 0으로 리셋
		 */
		UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attr")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, Damage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
