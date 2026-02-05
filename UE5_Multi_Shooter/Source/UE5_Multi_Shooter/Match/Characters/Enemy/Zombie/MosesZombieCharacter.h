#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"

#include "GameplayEffectTypes.h" // [MOD] FGameplayEffectContextHandle 정의 여기 있음(이거 없어서 연쇄 폭발)

#include "UE5_Multi_Shooter/Match/Characters/Animation/AnimNotifies/AnimNotifyState_MosesZombieAttackWindow.h" // [MOD] 경로 수정

#include "MosesZombieCharacter.generated.h"

class UBoxComponent;

class UMosesAbilitySystemComponent;
class UMosesZombieAttributeSet;
class UMosesZombieTypeData;

class AMosesPlayerState;
class AMosesMatchGameState;

struct FGameplayEffectModCallbackData;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	// [MOD] AttributeSet이 보호된 서버 처리 함수를 호출해야 하므로 friend로만 허용
	friend class UMosesZombieAttributeSet;

public:
	AMosesZombieCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Moses|Zombie")
	void ServerStartAttack();

	void ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin);

protected:
	void InitializeAttributes_Server();
	UAnimMontage* PickAttackMontage_Server() const;

	void SetAttackHitEnabled_Server(EMosesZombieAttackHand Hand, bool bEnabled);

	UAbilitySystemComponent* FindASCFromActor_Server(AActor* TargetActor) const;
	bool ApplySetByCallerDamageGE_Server(UAbilitySystemComponent* TargetASC, float DamageAmount) const;

	UFUNCTION()
	void OnAttackHitOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// 여기 protected 유지 가능 (friend로 AttributeSet만 호출 가능)
	void HandleDamageAppliedFromGAS_Server(const FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth);

	AMosesPlayerState* ResolveKillerPlayerState_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;
	bool ResolveHeadshot_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;

	void HandleDeath_Server();
	void PushKillFeed_Server(bool bHeadshot, AMosesPlayerState* KillerPS);
	void PushHeadshotAnnouncement_Server(AMosesPlayerState* KillerPS) const;

	/** 손 소켓에 공격 히트박스를 부착한다. (소켓 없으면 부착하지 않음, BP 상대 트랜스폼 유지) */
	void AttachAttackHitBoxesToHandSockets();

protected:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay);

private:
	void ResetHitActorsThisWindow();
	bool HasHitActorThisWindow(AActor* Actor) const;
	void MarkHitActorThisWindow(AActor* Actor);

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Zombie")
	TObjectPtr<UMosesZombieTypeData> ZombieTypeData;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|GAS")
	TObjectPtr<UMosesAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|GAS")
	TObjectPtr<UMosesZombieAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Zombie|Melee")
	TObjectPtr<UBoxComponent> AttackHitBox_L;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Zombie|Melee")
	TObjectPtr<UBoxComponent> AttackHitBox_R;

	UPROPERTY(VisibleInstanceOnly, Category = "Moses|Zombie|Melee")
	bool bAttackHitEnabled_L = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Moses|Zombie|Melee")
	bool bAttackHitEnabled_R = false;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Hit")
	FName HeadBoneName;

	UPROPERTY(Transient)
	TObjectPtr<AMosesPlayerState> LastDamageKillerPS;

	UPROPERTY(Transient)
	bool bLastDamageHeadshot = false;

private:
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsThisWindow;
};
