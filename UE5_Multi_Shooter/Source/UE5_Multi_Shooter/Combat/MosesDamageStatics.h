#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * Damage statics (server authority)
 *
 * - 모든 무기 데미지를 "GAS GameplayEffect + SetByCaller(Data.Damage)"로 통일 적용한다.
 * - 타겟에 ASC가 없으면 폴백(ApplyDamage)을 쓰되, 프로젝트 캐논은 GAS 경로다.
 */
struct UE5_MULTI_SHOOTER_API FMosesDamageStatics
{
public:
	/** 서버: Target ASC에 DamageEffectClass를 Spec으로 만들어 적용한다. */
	static bool ApplyDamageEffect_Server(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		const float DamageAmount);

	/** 서버: ASC를 안전하게 얻는다(Actor에서). */
	static UAbilitySystemComponent* FindASC(AActor* Actor);
};
