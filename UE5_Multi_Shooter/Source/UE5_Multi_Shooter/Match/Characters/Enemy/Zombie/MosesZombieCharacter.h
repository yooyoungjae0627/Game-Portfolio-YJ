// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h (CLEANED)
// ----------------------------------------------------------------------------
// 역할
// - 좀비 Pawn 자체가 ASC를 소유 (Zombie Pawn = ASC Owner/Avatar)
// - 서버 권위 공격(히트박스 윈도우 + SetByCaller Damage GE 적용)
// - Multicast로 애니/상태 코스메틱만 전파
// ----------------------------------------------------------------------------
// 정책
// - Server Authority 100%
// - Tick 의존 X (AI/AnimNotify/Overlap 기반)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Types/MosesZombieAttackTypes.h"
#include "MosesZombieCharacter.generated.h"

class AActor;
class UAnimMontage;
class UBoxComponent;

class UAbilitySystemComponent;
class UMosesAbilitySystemComponent;
class UMosesZombieAttributeSet;

class UMosesZombieTypeData;
class UMosesZombieAnimInstance;

class AMosesPlayerState;
class AMosesPlayerController;

struct FGameplayEffectModCallbackData;
struct FGameplayEffectContextHandle;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesZombieCharacter();

	// =========================================================================
	// IAbilitySystemInterface
	// =========================================================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// =========================================================================
	// Read-only
	// =========================================================================
	UMosesZombieTypeData* GetZombieTypeData() const { return ZombieTypeData; }

	// =========================================================================
	// Server query / AI entry
	// =========================================================================
	float GetAttackRange_Server() const;
	float GetMaxChaseDistance_Server() const;

	// AIController/BTTask -> 여기로 요청
	bool ServerTryStartAttack_FromAI(AActor* TargetActor);

	// =========================================================================
	// Server attack pipeline
	// =========================================================================
	void ServerStartAttack();
	void ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin);

	// GAS Damage callback entry
	void HandleDamageAppliedFromGAS_Server(const FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// =========================================================================
	// Setup (Server)
	// =========================================================================
	void InitializeAttributes_Server();
	void AttachAttackHitBoxesToHandSockets();

	// =========================================================================
	// Attack internals (Server)
	// =========================================================================
	UAnimMontage* PickAttackMontage_Server() const;
	void SetAttackHitEnabled_Server(EMosesZombieAttackHand Hand, bool bEnabled);

	// Target ASC resolve + GE apply
	UAbilitySystemComponent* FindASCFromActor_Server(AActor* TargetActor) const;
	bool ApplySetByCallerDamageGE_Server(UAbilitySystemComponent* TargetASC, float DamageAmount) const;

	// HitBox overlap (Server)
	UFUNCTION()
	void OnAttackHitOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// Montage end (Server)
	UFUNCTION()
	void OnAttackMontageEnded_Server(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnDeathMontageEnded_Server(UAnimMontage* Montage, bool bInterrupted);

	// EffectContext 기반으로 킬러/헤드샷 해석 (Server)
	AMosesPlayerState* ResolveKillerPlayerState_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;
	bool ResolveHeadshot_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const;

	void HandleDeath_Server();

private:
	// =========================================================================
	// Multicast cosmetics
	// =========================================================================
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetDeadState(bool bInDead);

private:
	// =========================================================================
	// Window hit dedup (Server)
	// =========================================================================
	void ResetHitActorsThisWindow();
	bool HasHitActorThisWindow(AActor* Actor) const;
	void MarkHitActorThisWindow(AActor* Actor);

private:
	// =========================================================================
	// Anim helper
	// =========================================================================
	UMosesZombieAnimInstance* GetZombieAnimInstance() const;
	void SetZombieDeadFlag_Local(bool bInDead, const TCHAR* From) const;

private:
	// =========================================================================
	// GAS
	// =========================================================================
	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie")
	TObjectPtr<UMosesAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesZombieAttributeSet> AttributeSet = nullptr;

private:
	// =========================================================================
	// Data
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie")
	TObjectPtr<UMosesZombieTypeData> ZombieTypeData = nullptr;

private:
	// =========================================================================
	// HitBoxes
	// =========================================================================
	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie|HitBox")
	TObjectPtr<UBoxComponent> AttackHitBox_L = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Zombie|HitBox")
	TObjectPtr<UBoxComponent> AttackHitBox_R = nullptr;

	bool bAttackHitEnabled_L = false;
	bool bAttackHitEnabled_R = false;

	// 서버 윈도우 중복타격 방지
	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisWindow;

private:
	// =========================================================================
	// Headshot / last damage meta (Server-only)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie")
	FName HeadBoneName;

	UPROPERTY()
	TObjectPtr<AMosesPlayerState> LastDamageKillerPS = nullptr;

	bool bLastDamageHeadshot = false;

private:
	// =========================================================================
	// Attack / death guards (Server-only)
	// =========================================================================
	double NextAttackServerTime = 0.0;
	bool bIsAttacking_Server = false;

	bool bIsDying_Server = false;

private:
	// =========================================================================
	// Destroy delay
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Death")
	float DestroyDelayAfterDeathMontageSeconds = 5.0f;
};
