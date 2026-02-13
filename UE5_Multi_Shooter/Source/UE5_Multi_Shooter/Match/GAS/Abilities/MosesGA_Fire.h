#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MosesGA_Fire.generated.h"

/**
 * UMosesGA_Fire
 * - 입력(Fire)으로 실행되는 단발 Ability
 * - 실제 발사/판정/탄약 소모는 CombatComponent(서버 권위)로 위임
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesGA_Fire : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMosesGA_Fire();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
};
