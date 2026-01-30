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

	// =========================================================================
	// Input Endpoints (HeroComponent -> Pawn)
	// =========================================================================
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

	/**
	 * [MOD] Sprint 게이팅 규칙 (Tick 없이 입력 이벤트로만 처리)
	 *
	 * - Shift만 누르면 Sprint가 켜지면 안 된다.
	 * - Shift + WASD(= Move 입력이 0이 아님)일 때만 Sprint ON.
	 * - Shift가 눌려있어도 Move 입력이 0이면 Sprint OFF.
	 *
	 * 처리 타이밍
	 * - Input_Move에서 매번 Move 입력을 캐시하고, 조건을 평가해 서버에 요청
	 * - Input_SprintPressed/Released에서도 조건 평가(즉시 반영)
	 */
	void UpdateSprintRequest_Local(const TCHAR* FromTag);

	/** 로컬 체감용: 속도만 즉시 반영 (서버 승인 전이라도 "느낌"은 좋아짐) */
	void ApplySprintSpeed_LocalPredict(bool bNewSprinting);

	/** 서버 승인/OnRep로 내려온 값 기준으로 최종 확정(SSOT) */
	void ApplySprintSpeed_FromAuth(const TCHAR* From);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	/**
	 * [MOD] Sprint 몽타주 코스메틱
	 * - 서버 승인 후에만 Multicast로 재생/정지한다(모든 클라에서 동일 연출)
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySprintMontage(bool bStart);

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

	/** [MOD] 요구사항: Sprint MaxSpeed = 800 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 800.0f;

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

	/** [MOD] SprintStart 같은 짧은 몽타주 추천 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> SprintMontage = nullptr;

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
	// [MOD] Sprint 입력 캐시 (Tick 없이 게이팅)
	// =========================================================================
	UPROPERTY(Transient)
	bool bSprintKeyDownLocal = false;

	UPROPERTY(Transient)
	FVector2D LastMoveInputLocal = FVector2D::ZeroVector;

private:
	// =========================================================================
	// Hold-to-fire runtime state
	// =========================================================================
	UPROPERTY(Transient)
	bool bFireHeldLocal = false;

	FTimerHandle AutoFireTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float AutoFireTickRate = 0.06f;
};
