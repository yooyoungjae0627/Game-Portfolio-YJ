#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "MosesZombieCharacter.generated.h"

class UBoxComponent;
class UAnimMontage;
class UAbilitySystemComponent;
class UMosesAbilitySystemComponent;
class UMosesZombieAttributeSet;
class UMosesZombieTypeData;
class AMosesMatchGameState;
class AMosesPlayerState;
class UGameplayEffect;

/**
 * Zombie Character (Server Authority + GAS)
 *
 * 목표(DAY10):
 * - 스팟에서 스폰
 * - 공격 몽타주 2패턴 랜덤 재생
 * - 공격 타이밍 Overlap -> 서버가 Victim ASC에 GE_Damage_SetByCaller 적용(10)
 * - Zombie HP=100(GAS Health), 죽으면 Destroy
 * - KillFeed 표시, Headshot이면 "Headshot" + 전용 사운드
 *
 * 핵심:
 * - "피해/HP"는 반드시 GAS(UGameplayEffect)로만 변경한다.
 * - 판정(Overlap/HitResult)은 서버에서만 한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesZombieCharacter();

public:
	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

public:
	/** GAS Health 초기화 (서버) */
	void InitializeAttributes_Server();

	/** AttributeSet에서 Damage 적용 후 콜백(서버) */
	void HandleDamageAppliedFromGAS_Server(const struct FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth);

protected:
	//~AActor
	virtual void BeginPlay() override;

	/** 공격 시작(서버) */
	void ServerStartAttack();

	/** 공격 몽타주 선택(서버) */
	UAnimMontage* PickAttackMontage_Server() const;

	/** 공격 히트박스 토글(서버) */
	void SetAttackHitEnabled_Server(bool bEnabled);

	/** 손/무기 Overlap 히트(서버) -> Victim ASC에 GE Apply */
	UFUNCTION()
	void OnAttackHitOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 공격 몽타주 재생(코스메틱) */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay);

	/** 죽음 처리(서버) */
	void HandleDeath_Server();

	/** KillFeed push(서버) */
	void PushKillFeed_Server(bool bHeadshot, AMosesPlayerState* KillerPS);

	/** EffectContext에서 Killer/HitResult 추출(서버) */
	AMosesPlayerState* ResolveKillerPlayerState_FromEffectContext_Server(const struct FGameplayEffectContextHandle& Context) const;
	bool ResolveHeadshot_FromEffectContext_Server(const struct FGameplayEffectContextHandle& Context) const;

	/** Victim Actor -> ASC 찾기(서버) */
	UAbilitySystemComponent* FindASCFromActor_Server(AActor* TargetActor) const;

	/** Victim ASC에 Damage GE(SetByCaller) 적용(서버) */
	bool ApplySetByCallerDamageGE_Server(UAbilitySystemComponent* TargetASC, float DamageAmount) const;

protected:
	// --------------------
	// GAS Components
	// --------------------

	/** AbilitySystemComponent (Zombie owns ASC) */
	UPROPERTY(VisibleAnywhere, Category="GAS")
	TObjectPtr<UMosesAbilitySystemComponent> AbilitySystemComponent;

	/** Zombie AttributeSet */
	UPROPERTY(VisibleAnywhere, Category="GAS")
	TObjectPtr<UMosesZombieAttributeSet> AttributeSet;

protected:
	// --------------------
	// Data / Combat
	// --------------------

	UPROPERTY(EditDefaultsOnly, Category="Zombie|Data")
	TObjectPtr<UMosesZombieTypeData> ZombieTypeData;

	/** Head bone name for headshot detection (Default "head") */
	UPROPERTY(EditDefaultsOnly, Category="Zombie|Combat")
	FName HeadBoneName;

	/** Attack hit collision */
	UPROPERTY(VisibleAnywhere, Category="Zombie|Combat")
	TObjectPtr<UBoxComponent> AttackHitBox;

	/** Attack window enabled (server) */
	bool bAttackHitEnabled;

	/** Cached last-killer data (server) */
	bool bLastDamageHeadshot;
	UPROPERTY()
	TObjectPtr<AMosesPlayerState> LastDamageKillerPS;

public:
	// --------------------
	// Variables order rule: public -> protected -> private
	// --------------------
};
