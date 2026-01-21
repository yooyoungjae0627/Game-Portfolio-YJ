#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "MosesAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UMosesAttributeSet
 *
 * [역할]
 * - HP/Shield 같은 "속성(Attribute)"을 GAS로 관리한다. (사용자 지시: 무조건 GAS)
 *
 * [정책]
 * - RepNotify 기반으로 값 변경을 GAS 이벤트로 흘려보낸다.
 * - Tick/Binding 없음. HUD는 PlayerState Delegate를 구독한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// -------------------------
	// Health
	// -------------------------
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, MaxHealth)

	// -------------------------
	// Shield  [ADD]
	// -------------------------
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Attributes|Shield")
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, Shield)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "Attributes|Shield")
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, MaxShield)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION() // [ADD]
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION() // [ADD]
	void OnRep_MaxShield(const FGameplayAttributeData& OldValue);
};
