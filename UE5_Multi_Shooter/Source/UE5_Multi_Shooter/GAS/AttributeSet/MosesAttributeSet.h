#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MosesAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// NOTE(Day1): Health/MaxHealth만 먼저 고정.
	// Init/Rep 안정화 전에는 Attribute를 늘리면 디버깅이 불가능해진다.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMosesAttributeSet, MaxHealth)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
