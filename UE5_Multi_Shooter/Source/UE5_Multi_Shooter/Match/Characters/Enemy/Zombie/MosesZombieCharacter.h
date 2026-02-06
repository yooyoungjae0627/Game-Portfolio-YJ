// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h
// (FULL - UPDATED)
// - [NEW] bIsDying_Server 가드
// - Death RPC 제거(공용 Multicast_PlayAttackMontage 재사용)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Types/MosesZombieAttackTypes.h"
#include "MosesZombieCharacter.generated.h"

class UMosesAbilitySystemComponent;
class UMosesZombieAttributeSet;
class UMosesZombieTypeData;
class UBoxComponent;
class UAnimMontage;
class AMosesPlayerState;

struct FGameplayEffectModCallbackData;
struct FGameplayEffectContextHandle;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesZombieCharacter();

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

public:
	// -------------------------
	// AI Helper (Server)
	// -------------------------
	UMosesZombieTypeData* GetZombieTypeData() const { return ZombieTypeData; }

	float GetAttackRange_Server() const;
	float GetMaxChaseDistance_Server() const;

	/** AI가 호출: 쿨다운/상태 가드 후 공격 시작 */
	bool ServerTryStartAttack_FromAI(AActor* TargetActor);

public:
	/** 서버에서 공격 시작(외부 호출 전제) */
	void ServerStartAttack();

	/** AnimNotifyState에서 공격 윈도우를 켜고 끄기 */
	void ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin);

	/** GAS 데미지 적용 후(AttrSet에서 콜백) */
	void HandleDamageAppliedFromGAS_Server(const FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void AttachAttackHitBoxesToHandSockets();
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

	UFUNCTION()
	void OnAttackMontageEnded_Server(UAnimMontage* Montage, bool bInterrupted);

	AMosesPlayerState* ResolveKillerPlayerState_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;
	bool ResolveHeadshot_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;

	void HandleDeath_Server();
	float ComputeDeathDestroyDelaySeconds_Server(UAnimMontage* DeathMontage) const; 

	void PushKillFeed_Server(bool bHeadshot, AMosesPlayerState* KillerPS);
	void PushHeadshotAnnouncement_Server(AMosesPlayerState* KillerPS) const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay);

	// ---- Window Hit Dedup ----
	void ResetHitActorsThisWindow();
	bool HasHitActorThisWindow(AActor* Actor) const;
	void MarkHitActorThisWindow(AActor* Actor);

private:
	// ---- GAS ----
	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie")
	TObjectPtr<UMosesAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesZombieAttributeSet> AttributeSet = nullptr;

	// ---- Data ----
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie")
	TObjectPtr<UMosesZombieTypeData> ZombieTypeData = nullptr;

	// ---- HitBoxes ----
	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie|HitBox")
	TObjectPtr<UBoxComponent> AttackHitBox_L = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie|HitBox")
	TObjectPtr<UBoxComponent> AttackHitBox_R = nullptr;

	bool bAttackHitEnabled_L = false;
	bool bAttackHitEnabled_R = false;

	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisWindow;

	// ---- Headshot ----
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie")
	FName HeadBoneName;

	UPROPERTY()
	TObjectPtr<AMosesPlayerState> LastDamageKillerPS = nullptr;

	bool bLastDamageHeadshot = false;

	// -------------------------
	// AI Attack Guard
	// -------------------------
	double NextAttackServerTime = 0.0;
	bool bIsAttacking_Server = false;

	// -------------------------
	// [NEW] Death Guard (Server)
	// -------------------------
	bool bIsDying_Server = false; // [NEW]
};
