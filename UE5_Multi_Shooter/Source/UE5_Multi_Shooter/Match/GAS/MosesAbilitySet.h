#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"

#include "MosesAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

/**
 * [MOD] GrantedHandles는 "AbilitySet을 ASC에 부여한 결과"를 기록하는 핸들 묶음이다.
 * - 나중에 일괄 회수(RemoveFromAbilitySystem)하기 위해 필요
 * - 반드시 UMosesAbilitySet 선언/함수 시그니처보다 "앞"에 정의되어야 한다.
 */
USTRUCT()
struct FMosesAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

public:
	void Reset();
	void RemoveFromAbilitySystem(UAbilitySystemComponent& ASC);
};

USTRUCT(BlueprintType)
struct FMosesAbilitySet_AbilityEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly)
	int32 InputID = 0;
};

USTRUCT(BlueprintType)
struct FMosesAbilitySet_EffectEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> Effect = nullptr;

	UPROPERTY(EditDefaultsOnly)
	float Level = 1.0f;
};

/**
 * UMosesAbilitySet
 * - GAS 능력/이펙트 “부여 세트”
 * - 서버 권위로 ASC에 Abilities/StartupEffects를 지급
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesAbilitySet : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "AbilitySet")
	TArray<FMosesAbilitySet_AbilityEntry> Abilities;

	UPROPERTY(EditDefaultsOnly, Category = "AbilitySet")
	TArray<FMosesAbilitySet_EffectEntry> StartupEffects;

public:
	void GiveToAbilitySystem(UAbilitySystemComponent& ASC, FMosesAbilitySet_GrantedHandles& OutGrantedHandles) const;
};
