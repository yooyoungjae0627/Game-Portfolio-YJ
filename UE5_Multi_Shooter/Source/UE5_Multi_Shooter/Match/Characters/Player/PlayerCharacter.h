// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h  (FULL - CLEAN)
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

	// Input endpoints (HeroComponent -> Pawn)
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

	// Hold-to-fire
	void Input_FirePressed();
	void Input_FireReleased();

	// AnimNotify entrypoints (Swap montage)
	void HandleSwapDetachNotify();
	void HandleSwapAttachNotify();

protected:
	// Engine
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
	// Cosmetics (server -> multicast)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireMontage(FGameplayTag WeaponId);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReactMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage_WithPid(const FString& VictimPid);

private:
	// Sprint
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
	// Combat bind (RepNotify -> Delegate -> Cosmetic)
	void BindCombatComponent();
	void UnbindCombatComponent();

	void HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId);
	void HandleDeadChanged(bool bNewDead);
	void HandleReloadingChanged(bool bReloading);
	void HandleSwapStarted(int32 FromSlot, int32 ToSlot, int32 Serial);

	UMosesCombatComponent* GetCombatComponent_Checked() const;

	void ForceSyncAnimDeadStateFromSSOT(const TCHAR* FromTag);
	bool GetDeadState_FromSSOT() const;

private:
	// Weapon visuals (Hand + Back1/2/3)
	void RefreshAllWeaponMeshes_FromSSOT();
	void RefreshWeaponMesh_ForSlot(int32 SlotIndex, FGameplayTag WeaponId, USkeletalMeshComponent* TargetMeshComp);

	void ApplyAttachmentPlan_Immediate(int32 EquippedSlot);

	FName GetHandSocketName() const;

	FName GetBackSocketNameForSlot(int32 EquippedSlot, int32 SlotIndex) const;

	USkeletalMeshComponent* GetMeshCompForSlot(int32 SlotIndex) const;
	void CacheSlotMeshMapping();

	void BuildBackSlotList_UsingSSOT(int32 EquippedSlot, TArray<int32>& OutBackSlots) const;

private:
	// Swap montage runtime (Cosmetic only)
	void BeginSwapCosmetic_Local(int32 FromSlot, int32 ToSlot, int32 Serial);
	void EndSwapCosmetic_Local(const TCHAR* Reason);

private:
	// Montage cosmetics
	void TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const;

	void PlayFireAV_Local(FGameplayTag WeaponId) const;
	void ApplyDeadCosmetics_Local() const;

private:
	// Hold-to-fire (Local only)
	void StartAutoFire_Local();
	void StopAutoFire_Local();
	void HandleAutoFireTick_Local();

private:
	// Components
	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> MosesCameraComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesInteractionComponent> InteractionComponent = nullptr;

	// Weapon meshes (Cosmetic only)
	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Hand = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Back1 = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Back2 = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh_Back3 = nullptr;

private:
	// Tunables
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 800.0f;

	// Socket names
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Hand = TEXT("WeaponSocket_Hand");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Back_1 = TEXT("WeaponSocket_Back_1");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Back_2 = TEXT("WeaponSocket_Back_2");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|Socket")
	FName WeaponSocket_Back_3 = TEXT("WeaponSocket_Back_3");

private:
	// Montages
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
	// Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(Transient)
	bool bLocalPredictedSprinting = false;

	UPROPERTY(Transient)
	TObjectPtr<UMosesCombatComponent> CachedCombatComponent = nullptr;

private:
	// Sprint input cache
	UPROPERTY(Transient)
	bool bSprintKeyDownLocal = false;

	UPROPERTY(Transient)
	FVector2D LastMoveInputLocal = FVector2D::ZeroVector;

private:
	// Hold-to-fire runtime
	UPROPERTY(Transient)
	bool bFireHeldLocal = false;

	FTimerHandle AutoFireTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float AutoFireTickRate = 0.06f;

private:
	// Swap runtime (Cosmetic only)
	UPROPERTY(Transient)
	bool bSwapInProgress = false;

	UPROPERTY(Transient)
	int32 PendingSwapFromSlot = 1;

	UPROPERTY(Transient)
	int32 PendingSwapToSlot = 1;

	UPROPERTY(Transient)
	int32 PendingSwapSerial = 0;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SlotMeshCache[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

	UPROPERTY(Transient)
	bool bSlotMeshCacheBuilt = false;

	UPROPERTY(Transient)
	bool bSwapDetachDone = false;

	UPROPERTY(Transient)
	bool bSwapAttachDone = false;
};
