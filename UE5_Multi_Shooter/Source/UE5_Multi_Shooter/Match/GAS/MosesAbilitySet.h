#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"

#include "MosesAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

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
 *
 * [MOD] UPrimaryDataAsset로 변경:
 * - 에디터에서 Data Asset 생성 가능
 * - GameFeatureAction의 SoftObjectPtr로 참조하기 좋음
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesAbilitySet : public UDataAsset  
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
