#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"

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

/**
 * Zombie Character (Server Authority + GAS)
 *
 * DAY10:
 * - 스팟에서 스폰
 * - 공격 몽타주 2패턴 랜덤
 * - 공격 윈도우 동안 Overlap -> Victim ASC에 GE(SetByCaller) 적용
 * - 좀비 HP=100 (GAS)
 * - KillFeed + Headshot이면 공용 Announcement 팝업
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesZombieCharacter();

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

private:
	// -------------------------
	// Init / Attributes
	// -------------------------
	void InitializeAttributes_Server();

	// -------------------------
	// Attack (Server)
	// -------------------------
	void ServerStartAttack();
	UAnimMontage* PickAttackMontage_Server() const;
	void SetAttackHitEnabled_Server(bool bEnabled);

	UFUNCTION()
	void OnAttackHitOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay);

	// -------------------------
	// Damage / Death (Server)
	// -------------------------
public:
	void HandleDamageAppliedFromGAS_Server(const struct FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth);

private:
	void HandleDeath_Server();

	void PushKillFeed_Server(bool bHeadshot, AMosesPlayerState* KillerPS);

	// [DAY10][MOD] Headshot -> 공용 Announcement 팝업
	void PushHeadshotAnnouncement_Server(AMosesPlayerState* KillerPS) const;

	// -------------------------
	// Context Helpers (Server)
	// -------------------------
	AMosesPlayerState* ResolveKillerPlayerState_FromEffectContext_Server(const struct FGameplayEffectContextHandle& Context) const;
	bool ResolveHeadshot_FromEffectContext_Server(const struct FGameplayEffectContextHandle& Context) const;

	UAbilitySystemComponent* FindASCFromActor_Server(AActor* TargetActor) const;
	bool ApplySetByCallerDamageGE_Server(UAbilitySystemComponent* TargetASC, float DamageAmount) const;

private:
	// -------------------------
	// GAS
	// -------------------------
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMosesAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMosesZombieAttributeSet> AttributeSet = nullptr;

private:
	// -------------------------
	// Data / Combat
	// -------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Data")
	TObjectPtr<UMosesZombieTypeData> ZombieTypeData = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Combat")
	FName HeadBoneName;

	UPROPERTY(VisibleAnywhere, Category = "Zombie|Combat")
	TObjectPtr<UBoxComponent> AttackHitBox = nullptr;

	bool bAttackHitEnabled = false;

	bool bLastDamageHeadshot = false;

	UPROPERTY()
	TObjectPtr<AMosesPlayerState> LastDamageKillerPS = nullptr;
};
