#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "PlayerCharacter.generated.h"

class AController;
class APickupBase;

class UMosesHeroComponent;
class UMosesCameraComponent;
class UMosesCombatComponent;

class USkeletalMeshComponent;
class UAnimMontage;

UCLASS()
class UE5_MULTI_SHOOTER_API APlayerCharacter : public AMosesCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	bool IsSprinting() const;

	void Input_Move(const FVector2D& MoveValue);
	void Input_Look(const FVector2D& LookValue);

	void Input_SprintPressed();
	void Input_SprintReleased();

	void Input_JumpPressed();
	void Input_InteractPressed();

	void Input_EquipSlot1();
	void Input_EquipSlot2();
	void Input_EquipSlot3();

	// =========================================================================
	// Hold-to-fire
	// =========================================================================
	void Input_FirePressed();
	void Input_FireReleased();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PostInitializeComponents() override;

	virtual void OnRep_Controller() override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

public:
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireMontage(FGameplayTag WeaponId);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReactMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage();

private:
	// =========================================================================
	// Sprint
	// =========================================================================
	void ApplySprintSpeed_LocalPredict(bool bNewSprinting);
	void ApplySprintSpeed_FromAuth(const TCHAR* From);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

private:
	// =========================================================================
	// Pickup
	// =========================================================================
	APickupBase* FindPickupTarget_Local() const;

	UFUNCTION(Server, Reliable)
	void Server_TryPickup(APickupBase* Target);

private:
	// =========================================================================
	// Combat bind
	// =========================================================================
	void BindCombatComponent();
	void UnbindCombatComponent();

	void HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId);
	void HandleDeadChanged(bool bNewDead);

	void RefreshWeaponCosmetic(FGameplayTag WeaponId);

	UMosesCombatComponent* GetCombatComponent_Checked() const;

private:
	// =========================================================================
	// Montage cosmetics
	// =========================================================================
	void TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const;
	void PlayFireAV_Local(FGameplayTag WeaponId) const;
	void ApplyDeadCosmetics_Local() const;

private:
	// =========================================================================
	// Hold-to-fire (Local only)
	// =========================================================================
	void StartAutoFire_Local();
	void StopAutoFire_Local();
	void HandleAutoFireTick_Local();

private:
	// =========================================================================
	// Components
	// =========================================================================
	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> MosesCameraComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp = nullptr;

private:
	// =========================================================================
	// Tunables
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Pickup")
	float PickupTraceDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FName CharacterWeaponSocketName = TEXT("WeaponSocket");

private:
	// =========================================================================
	// Montages
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

private:
	// =========================================================================
	// Replicated state
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(Transient)
	bool bLocalPredictedSprinting = false;

	UPROPERTY(Transient)
	TObjectPtr<UMosesCombatComponent> CachedCombatComponent = nullptr;

private:
	// =========================================================================
	// Hold-to-fire runtime state
	// =========================================================================
	UPROPERTY(Transient)
	bool bFireHeldLocal = false;

	// [MOD] TimerHandle 타입은 헤더에서 선언 가능(Engine 타입)
	// - cpp에서 TimerManager.h include 필요
	FTimerHandle AutoFireTimerHandle;

	// 타이머 틱은 서버가 아니라 "클라 요청 빈도"임. 서버는 FireInterval로 최종 승인.
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float AutoFireTickRate = 0.06f;
};
