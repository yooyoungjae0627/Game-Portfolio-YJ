// ============================================================================
// MosesCombatComponent.h (FULL)  [MOD: DAY6 SwapContext + BackWeapons]
// ----------------------------------------------------------------------------
// Owner = PlayerState (SSOT)
// ----------------------------------------------------------------------------
// - 슬롯 1~4 고정 확장
// - Reload 서버 권위 + 서버 타이머
// - 기본 지급: Slot1 Rifle.A + 30/90
// - Damage: GAS(SetByCaller Data.Damage) 우선 적용 + ASC 없으면 ApplyDamage fallback
// - HUD 갱신: RepNotify -> Native Delegate (Tick/Binding 금지)
// - [DAY6] SwapContext(From/To/Serial) 복제 -> Pawn 코스메틱(몽타주/Notify Attach 교체) 트리거
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "MosesCombatComponent.generated.h"

class UMosesWeaponData;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnDeadChangedNative, bool /*bNewDead*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnReloadingChangedNative, bool /*bReloading*/);

/** [DAY6] 서버 승인 Swap 컨텍스트: Pawn 코스메틱 전용 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnSwapStartedNative, int32 /*FromSlot*/, int32 /*ToSlot*/, int32 /*Serial*/);

UENUM(BlueprintType)
enum class EMosesFireGuardFailReason : uint8
{
	None,

	NoOwner,
	NoPawn,
	NoController,

	NoWeaponId,
	NoWeaponData,

	Cooldown,
	NoAmmo,

	InvalidPhase,
	IsDead,
};

UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent();

	// =========================================================================
	// SSOT Query
	// =========================================================================
	int32 GetCurrentSlot() const { return CurrentSlot; }
	FGameplayTag GetWeaponIdForSlot(int32 SlotIndex) const;
	FGameplayTag GetEquippedWeaponId() const;

	int32 GetCurrentMagAmmo() const;
	int32 GetCurrentReserveAmmo() const;

	bool IsDead() const { return bIsDead; }
	bool IsReloading() const { return bIsReloading; }

	// =========================================================================
	// [DAY6] SwapContext Query (Cosmetic trigger only)
	// =========================================================================
	int32 GetLastSwapFromSlot() const { return LastSwapFromSlot; }
	int32 GetLastSwapToSlot() const { return LastSwapToSlot; }
	int32 GetSwapSerial() const { return SwapSerial; }

	// =========================================================================
	// Equip API (Client -> Server)
	// =========================================================================
	void RequestEquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	// =========================================================================
	// Fire API (Client -> Server)
	// =========================================================================
	void RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

	// =========================================================================
	// Reload API (Client -> Server)
	// =========================================================================
	void RequestReload();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	// =========================================================================
	// Default Init / Loadout (Server only)
	// =========================================================================
	void ServerInitDefaultSlots_4(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3, const FGameplayTag& InSlot4);
	void ServerGrantDefaultRifleAmmo_30_90();

	// =========================================================================
	// Delegates (RepNotify -> Delegate)
	// =========================================================================
	FMosesOnEquippedChanged OnEquippedChanged;
	FMosesOnAmmoChangedNative OnAmmoChanged;
	FMosesOnDeadChangedNative OnDeadChanged;
	FMosesOnReloadingChangedNative OnReloadingChanged;

	// [DAY6] 서버 승인 Swap 이벤트(코스메틱 트리거)
	FMosesOnSwapStartedNative OnSwapStarted;

	// =========================================================================
	// Dead Hook (서버에서만 확정)
	// =========================================================================
	void ServerMarkDead();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// =========================================================================
	// RepNotifies
	// =========================================================================
	UFUNCTION() void OnRep_CurrentSlot();

	UFUNCTION() void OnRep_Slot1WeaponId();
	UFUNCTION() void OnRep_Slot2WeaponId();
	UFUNCTION() void OnRep_Slot3WeaponId();
	UFUNCTION() void OnRep_Slot4WeaponId();

	UFUNCTION() void OnRep_Slot1Ammo();
	UFUNCTION() void OnRep_Slot2Ammo();
	UFUNCTION() void OnRep_Slot3Ammo();
	UFUNCTION() void OnRep_Slot4Ammo();

	UFUNCTION() void OnRep_IsDead();
	UFUNCTION() void OnRep_IsReloading();

	// [DAY6] 서버 승인 Swap 컨텍스트 RepNotify
	UFUNCTION() void OnRep_SwapSerial();

private:
	// =========================================================================
	// Broadcast helpers (RepNotify → Delegate)
	// =========================================================================
	void BroadcastEquippedChanged(const TCHAR* ContextTag);
	void BroadcastAmmoChanged(const TCHAR* ContextTag);
	void BroadcastDeadChanged(const TCHAR* ContextTag);
	void BroadcastReloadingChanged(const TCHAR* ContextTag);

	// [DAY6]
	void BroadcastSwapStarted(const TCHAR* ContextTag);

private:
	// =========================================================================
	// Slot / Ammo helpers
	// =========================================================================
	bool IsValidSlotIndex(int32 SlotIndex) const;
	FGameplayTag GetSlotWeaponIdInternal(int32 SlotIndex) const;

	void Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId);
	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserve) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserve);

private:
	// =========================================================================
	// Fire helpers (Server only)
	// =========================================================================
	bool Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const;
	const UMosesWeaponData* Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const;

	void Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData);

	float Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* WeaponData) const;
	bool Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const;
	void Server_UpdateFireCooldownStamp();

	void Server_PerformHitscanAndApplyDamage(const UMosesWeaponData* WeaponData);
	bool Server_ApplyDamageToTarget_GAS(AActor* TargetActor, float Damage, AController* InstigatorController, AActor* DamageCauser, const UMosesWeaponData* WeaponData) const;

	void Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId);

private:
	// =========================================================================
	// Reload helpers (Server only)
	// =========================================================================
	void Server_StartReload(const UMosesWeaponData* WeaponData);
	void Server_FinishReload();

private:
	// =========================================================================
	// Replicated SSOT state
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSlot)
	int32 CurrentSlot = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Slot1WeaponId)
	FGameplayTag Slot1WeaponId;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2WeaponId)
	FGameplayTag Slot2WeaponId;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3WeaponId)
	FGameplayTag Slot3WeaponId;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4WeaponId)
	FGameplayTag Slot4WeaponId;

	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo)
	int32 Slot1MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo)
	int32 Slot1ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	bool bIsReloading = false;

	// ---------------------------------------------------------------------
	// [DAY6] SwapContext (Server approved) - Cosmetic trigger only
	// ---------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_SwapSerial)
	int32 SwapSerial = 0;

	UPROPERTY(Replicated)
	int32 LastSwapFromSlot = 1;

	UPROPERTY(Replicated)
	int32 LastSwapToSlot = 1;

private:
	// =========================================================================
	// Runtime guards / stamps
	// =========================================================================
	UPROPERTY(Transient)
	bool bInitialized_DefaultSlots = false;

	UPROPERTY(Transient)
	double SlotLastFireTimeSec[4] = { -9999.0, -9999.0, -9999.0, -9999.0 };

	FTimerHandle ReloadTimerHandle;

private:
	// =========================================================================
	// Fire policy
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float HitscanDistance = 20000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	TEnumAsByte<ECollisionChannel> FireTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	FName HeadshotBoneName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float HeadshotDamageMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultFireIntervalSec = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS")
	TSoftClassPtr<class UGameplayEffect> DamageGE_SetByCaller;
};
