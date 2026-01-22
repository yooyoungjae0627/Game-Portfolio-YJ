#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "MosesAbilitySet.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAbilitySystemComponent;

/**
 * FMosesAbilitySet_GrantedHandles
 *
 * [역할]
 * - AbilitySet이 ASC에 부여한 “핸들들”을 추적해 나중에 제거할 수 있게 한다.
 *
 * [주의]
 * -“부여”만으로 충분하지만,
 *   확장(Experience 교체/모드 전환 등)에서는 제거가 필요할 수 있다.
 */

USTRUCT(BlueprintType)
struct FMosesAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	void RemoveFromAbilitySystem(UAbilitySystemComponent& ASC);
	void Reset();

public:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

/**
 * FMosesAbilitySet_AbilityEntry
 *
 * [역할]
 * - 부여할 GameplayAbility의 정보(레벨/입력ID 포함).
 */

USTRUCT(BlueprintType)
struct FMosesAbilitySet_AbilityEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly)
	int32 InputID = 0;
};

/**
 * FMosesAbilitySet_EffectEntry
 *
 * [역할]
 * - 시작 시점에 적용할 GameplayEffect 정보(레벨 포함).
 */
USTRUCT(BlueprintType)
struct FMosesAbilitySet_EffectEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly)
	float Level = 1.f;
};

/**
 * UMosesAbilitySet
 *
 * [역할]
 * - Experience(AbilitySetPath)로부터 로드되어,
 *   서버에서 ASC에 부여되는 “GAS 페이로드 묶음”
 *
 * [원칙]
 * - 데이터만 보관한다.
 * - 실제 적용은 서버에서만 수행한다.
 */

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	void GiveToAbilitySystem(UAbilitySystemComponent& ASC, FMosesAbilitySet_GrantedHandles& OutGrantedHandles) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|AbilitySet")
	TArray<FMosesAbilitySet_AbilityEntry> Abilities;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|AbilitySet")
	TArray<FMosesAbilitySet_EffectEntry> StartupEffects;
};
