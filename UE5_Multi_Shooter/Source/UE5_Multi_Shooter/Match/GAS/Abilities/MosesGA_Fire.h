#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MosesGA_Fire.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitInputRelease;

/**
 * UMosesGA_Fire
 * - Fire 입력으로 실행되는 "자동 연사" Ability
 * - 서버 권위 유지: 발사/판정/탄약 소모는 CombatComponent(서버 루트)로 위임
 * - Press: Activate(서버) -> 첫 발 + 주기 반복
 * - Release: 반복 중지 -> EndAbility
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

private:
	// [FIRE] 연사 주기(초). 무기별 ROF는 CombatComp/WeaponData에서 가져오고 싶으면 여기 대신 거기서 계산하도록 변경 가능.
	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	float FireIntervalSeconds = 0.1f;

	// [FIRE] Hold 중인지
	bool bWantsToFire = false;

	// [FIRE] 반복 발사 1회
	void FireOnce(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	// [FIRE] Delay 종료 후 다음 발
	UFUNCTION()
	void OnFireDelayFinished();

	// [FIRE] 입력 해제 시 루프 종료
	UFUNCTION()
	void OnInputReleased(float TimeHeldSeconds);
};
