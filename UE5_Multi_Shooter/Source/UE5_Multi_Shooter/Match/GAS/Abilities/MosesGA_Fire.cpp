#include "UE5_Multi_Shooter/Match/GAS/Abilities/MosesGA_Fire.h"

#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

UMosesGA_Fire::UMosesGA_Fire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// ✅ 핵심 변경:
	// ServerOnly면 WaitInputRelease가 서버에서 안 올 수 있어 무한연사 위험.
	// LocalPredicted로 두고, "로컬 컨트롤"에서만 RequestFire를 호출해서 1회만 서버에 요청한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// (선택) 입력 기반이면 보통 이렇게도 둠
	// ActivationPolicy = EGameplayAbilityActivationPolicy::OnInputTriggered;
}

void UMosesGA_Fire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// ✅ LocalPredicted에서는 Commit이 로컬에서도 호출되는데,
	// 실패 시 즉시 종료(서버에서도 실패하면 reject됨)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][GA_Fire] CommitAbility FAILED"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bWantsToFire = true;

	// ✅ Release 감지: 이제 로컬에서 확실히 잡힘 (LocalPredicted)
	if (UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
	{
		WaitRelease->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
		WaitRelease->ReadyForActivation();
	}

	// 첫 발
	FireOnce(Handle, ActorInfo, ActivationInfo);

	// 다음 발 예약
	if (FireIntervalSeconds > 0.0f)
	{
		if (UAbilityTask_WaitDelay* WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, FireIntervalSeconds))
		{
			WaitDelay->OnFinish.AddDynamic(this, &ThisClass::OnFireDelayFinished);
			WaitDelay->ReadyForActivation();
		}
	}
}

void UMosesGA_Fire::FireOnce(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	AMosesPlayerState* PS = ActorInfo ? Cast<AMosesPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	if (!PS)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][GA_Fire] Reject (NoPS)"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UMosesCombatComponent* CombatComp = PS->FindComponentByClass<UMosesCombatComponent>();
	if (!CombatComp)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][GA_Fire] Reject (NoCombatComponent) PS=%s"), *GetNameSafe(PS));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ============================================================
	// ✅ 핵심: "로컬 컨트롤"에서만 RequestFire를 호출한다.
	// - Dedicated 서버/서버측 Ability 인스턴스에서도 실행되지만
	//   LocallyControlled가 아니면 여기서 막아서 "중복 발사" 방지.
	// - 원격 클라: 클라(로컬)에서 1번 호출 -> Server RPC/서버 권위 루트로 처리
	// - 리슨서버 로컬플레이어: LocallyControlled=true 이므로 호출됨(서버 권위로 바로 처리)
	// ============================================================
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		CombatComp->RequestFire();
		UE_LOG(LogMosesGAS, Verbose,
			TEXT("[GAS][GA_Fire] FireOnce -> RequestFire (LocallyControlled) PS=%s"),
			*GetNameSafe(PS));
	}
	else
	{
		UE_LOG(LogMosesGAS, VeryVerbose,
			TEXT("[GAS][GA_Fire] FireOnce SKIP (NotLocallyControlled) PS=%s"),
			*GetNameSafe(PS));
	}
}

void UMosesGA_Fire::OnFireDelayFinished()
{
	if (!bWantsToFire)
	{
		UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS][GA_Fire] Stop (InputReleased)"));

		// ✅ 끝낼 때 replicate true (서버/클라 정리)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FireOnce(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);

	// 다음 발 계속 예약
	if (FireIntervalSeconds > 0.0f)
	{
		if (UAbilityTask_WaitDelay* WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, FireIntervalSeconds))
		{
			WaitDelay->OnFinish.AddDynamic(this, &ThisClass::OnFireDelayFinished);
			WaitDelay->ReadyForActivation();
		}
	}
}

void UMosesGA_Fire::OnInputReleased(float /*TimeHeldSeconds*/)
{
	bWantsToFire = false;

	UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS][GA_Fire] OnInputReleased -> bWantsToFire=0"));
}

void UMosesGA_Fire::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	bWantsToFire = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
