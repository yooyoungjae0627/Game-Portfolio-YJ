#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectExtension.h" 
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Types/MosesZombieAttackTypes.h"
#include "MosesZombieCharacter.generated.h"

class UMosesAbilitySystemComponent;
class UMosesZombieAttributeSet;
class UMosesZombieTypeData;
class UMosesZombieAnimInstance;
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
	UMosesZombieTypeData* GetZombieTypeData() const { return ZombieTypeData; }

	float GetAttackRange_Server() const;
	float GetMaxChaseDistance_Server() const;

	bool ServerTryStartAttack_FromAI(AActor* TargetActor);

public:
	void ServerStartAttack();
	void ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin);
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

	UFUNCTION()
	void OnDeathMontageEnded_Server(UAnimMontage* Montage, bool bInterrupted);

	// [MOD] 킬러/헤드샷 해석 (EffectContext 기반)
	AMosesPlayerState* ResolveKillerPlayerState_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;
	bool ResolveHeadshot_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;

	void HandleDeath_Server();

	// RPC
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetDeadState(bool bInDead);

	// ---- Window Hit Dedup ----
	void ResetHitActorsThisWindow();
	bool HasHitActorThisWindow(AActor* Actor) const;
	void MarkHitActorThisWindow(AActor* Actor);

	UMosesZombieAnimInstance* GetZombieAnimInstance() const;
	void SetZombieDeadFlag_Local(bool bInDead, const TCHAR* From) const;

private:
	// GAS
	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie")
	TObjectPtr<UMosesAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesZombieAttributeSet> AttributeSet = nullptr;

	// Data
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie")
	TObjectPtr<UMosesZombieTypeData> ZombieTypeData = nullptr;

	// HitBoxes
	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie|HitBox")
	TObjectPtr<UBoxComponent> AttackHitBox_L = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie|HitBox")
	TObjectPtr<UBoxComponent> AttackHitBox_R = nullptr;

	bool bAttackHitEnabled_L = false;
	bool bAttackHitEnabled_R = false;

	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisWindow;

	// Headshot
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie")
	FName HeadBoneName;

	// ✅ [MOD] 마지막 데미지의 가해자 정보(서버에서만)
	UPROPERTY()
	TObjectPtr<AMosesPlayerState> LastDamageKillerPS = nullptr;

	bool bLastDamageHeadshot = false;

	// AI Attack Guard
	double NextAttackServerTime = 0.0;
	bool bIsAttacking_Server = false;

	// Death Guard
	bool bIsDying_Server = false;

	// Destroy delay after death montage end
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Death")
	float DestroyDelayAfterDeathMontageSeconds = 5.0f; // ✅ 요청: 몽타주 끝나고 5초 뒤 삭제
};
