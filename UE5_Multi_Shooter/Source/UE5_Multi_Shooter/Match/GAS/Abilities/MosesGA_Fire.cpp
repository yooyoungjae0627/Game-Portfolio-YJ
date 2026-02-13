#include "UE5_Multi_Shooter/Match/GAS/Abilities/MosesGA_Fire.h"

#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesGA_Fire::UMosesGA_Fire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 서버 권위 유지: 실제 발사도 서버에서만
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// (선택) Ability 태그는 나중에 UI/로그/정책에 유용
	// AbilityTags.AddTag(FMosesGameplayTags::Get().Ability_Weapon_Fire);

	// (선택) 리로드 태그 붙이면 발사 차단
	// ActivationBlockedTags.AddTag(FMosesGameplayTags::Get().State_Reloading);
}

void UMosesGA_Fire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AMosesPlayerState* PS = ActorInfo ? Cast<AMosesPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	if (!PS)
	{
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

	// ✅ 실제 발사 판정/탄약/데미지는 CombatComponent의 서버 권위 루트가 처리
	CombatComp->RequestFire();

	UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS][GA_Fire] Fired -> CombatComponent::RequestFire PS=%s"), *GetNameSafe(PS));

	// 단발 처리: 누를 때마다 실행하고 즉시 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UMosesGA_Fire::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
