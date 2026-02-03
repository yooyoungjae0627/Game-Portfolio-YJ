#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h" // ATTRIBUTE_ACCESSORS(매크로 헤더)

#include "MosesAttributeSetBase.generated.h"

/**
 * UMosesAttributeSet (Player Core Attributes)
 * - 기존 코드베이스가 UMosesAttributeSet::GetHealthAttribute() 같은 정적 Getter를 기대하므로
 *   Health/MaxHealth/Shield/MaxShield를 실제로 제공한다.
 * - HUD는 PlayerState(SSOT)에서 RepNotify/Delegate로 처리하되,
 *   Attribute 자체 Rep은 프로젝트 정책에 맞게 유지 가능.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMosesAttributeSet();

public:
	//~UAttributeSet interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldValue);
};
