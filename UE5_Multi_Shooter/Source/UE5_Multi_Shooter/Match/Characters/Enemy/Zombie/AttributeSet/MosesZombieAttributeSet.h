#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h" 
#include "MosesZombieAttributeSet.generated.h"

class AActor;
class AMosesZombieCharacter;
struct FGameplayEffectModCallbackData;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesZombieAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesZombieAttributeSet();

	// UAttributeSet
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attr", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attr", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, MaxHealth)

	// Damage(meta): GE_Damage_SetByCaller가 이 Attribute에 누적되도록 설계
	UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attr")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UMosesZombieAttributeSet, Damage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

private:
	// [MOD] 좀비 캐릭터에 “데미지 적용됨” 콜백 전달 (킬카운트/죽음 처리 연결용)
	void NotifyZombieDamageApplied_Server(const FGameplayEffectModCallbackData& Data, float LocalDamage, float NewHealth) const;

	// [MOD] 디버그
	void LogDamageContext_Server(const FGameplayEffectModCallbackData& Data, float LocalDamage, float OldHealth, float NewHealth) const;
};
