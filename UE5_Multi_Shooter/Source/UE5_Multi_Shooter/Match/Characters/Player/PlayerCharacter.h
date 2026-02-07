// ============================================================================
// UE5_Multi_Shooter/Character/PlayerCharacter.h  (FULL - UPDATED)  [STEP2]
// ============================================================================
//
// [STEP2 목표]
// - "등 소켓 빈칸 없이 채우기" 규칙을 Pawn 코스메틱에서 완성한다.
//   - HandSocket  : CurrentSlot (서버 승인 결과)
//   - Back_1~Back_3: "무기 있는 슬롯"만 모아서 빈칸 없이 채움
//     예) Slot3만 있음 -> Back_1 = Slot3
//         Slot2,3 있음 -> Back_1 = Slot2, Back_2 = Slot3
//
// - Swap Montage AnimNotify 타이밍에서도 동일 규칙이 유지되어야 한다.
//   (Detach: FromSlot을 최종 상태 기준 Back_n으로, Attach: ToSlot을 Hand로)
//
// 정책 유지:
// - Tick 금지, UMG Binding 금지
// - SSOT = PlayerState(CombatComponent)
// - Pawn = Cosmetic Only (Attach/Montage/VFX/SFX)
//
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/MosesCharacter.h"

#include "PlayerCharacter.generated.h"

class AController;

class UMosesHeroComponent;
class UMosesCameraComponent;
class UMosesCombatComponent;
class UMosesInteractionComponent;

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
	void Input_LookYaw(float Value);
	void Input_LookPitch(float Value);

	void Input_SprintPressed();
	void Input_SprintReleased();

	void Input_JumpPressed();

	void Input_InteractPressed();
	void Input_InteractReleased();

	void Input_EquipSlot1();
	void Input_EquipSlot2();
	void Input_EquipSlot3();
	void Input_EquipSlot4();

	void Input_Reload();

	// =========================================================================
	// Hold-to-fire
	// =========================================================================
	void Input_FirePressed();
	void Input_FireReleased();

	// =========================================================================
	// AnimNotify entrypoints (Swap montage)
	// =========================================================================
	void HandleSwapDetachNotify();
	void HandleSwapAttachNotify();

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

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage_WithPid(const FString& VictimPid); 

private:
	// =========================================================================
	// Sprint
	// =========================================================================
	void UpdateSprintRequest_Local(const TCHAR* FromTag);
	void ApplySprintSpeed_LocalPredict(bool bNewSprinting);
	void ApplySprintSpeed_FromAuth(const TCHAR* From);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySprintMontage(bool bStart);
private:
	// =========================================================================
	// Combat bind (RepNotify -> Delegate -> Cosmetic)
	// =========================================================================
	void BindCombatComponent();
	void UnbindCombatComponent();

	void HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId);
	void HandleDeadChanged(bool bNewDead);
	void HandleReloadingChanged(bool bReloading);

	// 서버 승인 Swap 이벤트(From/To/Serial)
	void HandleSwapStarted(int32 FromSlot, int32 ToSlot, int32 Serial);

	UMosesCombatComponent* GetCombatComponent_Checked() const;

	void ForceSyncAnimDeadStateFromSSOT(const TCHAR* FromTag);
	bool GetDeadState_FromSSOT() const;

private:
	// =========================================================================
	// Weapon visuals (Hand + Back1/2/3)
	// =========================================================================
	void RefreshAllWeaponMeshes_FromSSOT();
	void RefreshWeaponMesh_ForSlot(int32 SlotIndex, FGameplayTag WeaponId, USkeletalMeshComponent* TargetMeshComp);

	/**
	 * ApplyAttachmentPlan_Immediate
	 * - SSOT(CurrentSlot + 슬롯별 WeaponId)를 기준으로 손/등 소켓 Attach를 즉시 강제한다.
	 *
	 * [STEP2 규칙]
	 * - EquippedSlot -> Hand
	 * - Back_1~3 -> "무기 있는 슬롯"만 모아서 빈칸 없이 채움
	 */
	void ApplyAttachmentPlan_Immediate(int32 EquippedSlot);

	FName GetHandSocketName() const;

	/**
	 * GetBackSocketNameForSlot
	 * - EquippedSlot(Hand)을 제외한 슬롯 중 "무기 있는 슬롯"만 모아
	 *   Back_1~3에 빈칸 없이 배치하는 소켓을 반환한다.
	 *
	 * 예)
	 * - Equipped=1, Have={3}           -> Slot3 -> Back_1
	 * - Equipped=1, Have={2,3}         -> Slot2 -> Back_1, Slot3 -> Back_2
	 * - Equipped=2, Have={1,3,4}       -> Slot1 -> Back_1, Slot3 -> Back_2, Slot4 -> Back_3
	 */
	FName GetBackSocketNameForSlot(int32 EquippedSlot, int32 SlotIndex) const; // ✅ [MOD][STEP2]

	USkeletalMeshComponent* GetMeshCompForSlot(int32 SlotIndex) const;
	void CacheSlotMeshMapping();

	/** [STEP2] Equipped 제외 "무기 있는 슬롯" 리스트를 만든다. (오름차순 정렬) */
	void BuildBackSlotList_UsingSSOT(int32 EquippedSlot, TArray<int32>& OutBackSlots) const; // ✅ [MOD][STEP2]

private:
	// =========================================================================
	// Swap montage runtime (Cosmetic only)
	// =========================================================================
	void BeginSwapCosmetic_Local(int32 FromSlot, int32 ToSlot, int32 Serial);
	void EndSwapCosmetic_Local(const TCHAR* Reason);

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

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesInteractionComponent> InteractionComponent = nullptr;

	// ---------------------------------------------------------------------
	// Weapon Meshes (Cosmetic only)
	// ---------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Hand = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Back1 = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Back2 = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Back3 = nullptr;

private:
	// =========================================================================
	// Tunables
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 800.0f;

	// ---------------------------------------------------------------------
	// Socket names (Skeleton sockets must exist)
	// ---------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Hand = TEXT("WeaponSocket_Hand");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Back_1 = TEXT("WeaponSocket_Back_1");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Back_2 = TEXT("WeaponSocket_Back_2");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Back_3 = TEXT("WeaponSocket_Back_3");

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

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> SprintMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim|Weapon")
	TObjectPtr<UAnimMontage> SwapMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim|Weapon")
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;

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
	// Sprint 입력 캐시 (Tick 없이 게이팅)
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

private:
	// =========================================================================
	// Swap runtime state (Cosmetic only)
	// =========================================================================
	UPROPERTY(Transient)
	bool bSwapInProgress = false;

	UPROPERTY(Transient)
	int32 PendingSwapFromSlot = 1;

	UPROPERTY(Transient)
	int32 PendingSwapToSlot = 1;

	UPROPERTY(Transient)
	int32 PendingSwapSerial = 0;

	// SlotIndex(1~4) -> MeshComponent mapping cache
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SlotMeshCache[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

	UPROPERTY(Transient)
	bool bSlotMeshCacheBuilt = false;

	// Notify guard
	UPROPERTY(Transient)
	bool bSwapDetachDone = false;

	UPROPERTY(Transient)
	bool bSwapAttachDone = false;
};
