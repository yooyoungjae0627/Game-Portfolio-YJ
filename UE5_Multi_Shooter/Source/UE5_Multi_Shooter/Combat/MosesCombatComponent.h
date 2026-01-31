// ============================================================================
// MosesCombatComponent.h (FULL)
// ----------------------------------------------------------------------------
// Owner = PlayerState (SSOT)
// ----------------------------------------------------------------------------
// [역할]
// - 서버 권위 전투 SSOT 컴포넌트
// - 장착/발사/탄약/사망 상태를 "PlayerState 소유"로 유지한다.
// - 모든 HUD 갱신은 RepNotify -> Native Delegate 기반 (Tick/Binding 금지)
//
// [발사 정책]
// - [FIX] 발사 쿨다운은 "몽타주 길이"가 아니라 DefaultFireIntervalSec(=발사속도) 기반.
//   => 몽타주가 끝나지 않아도 서버 쿨다운만 통과하면 계속 발사 승인 가능.
//
// [코스메틱 정책]
// - 서버 승인된 WeaponId를 Multicast 파라미터로 전파하여
//   무기별 SFX/VFX를 클라에서 재생한다. (Dedicated Server는 코스메틱 재생 금지)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "MosesCombatComponent.generated.h"

class UMosesWeaponData;
class AMosesPlayerState;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnDeadChangedNative, bool /*bNewDead*/);

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

	// =========================================================================
	// Equip API (Client -> Server)
	// =========================================================================
	void RequestEquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	// 서버: 기본 슬롯 초기화(Day2 초기화용)
	void ServerInitDefaultSlots(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3);

	// 서버: 안전장치(슬롯/탄 초기화가 누락된 상황을 보정)
	void Server_EnsureInitialized_Day2();

	// =========================================================================
	// Fire API (Client -> Server)
	// =========================================================================
	void RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

	// =========================================================================
	// Delegates (RepNotify -> Delegate)
	// =========================================================================
	FMosesOnEquippedChanged OnEquippedChanged;
	FMosesOnAmmoChangedNative OnAmmoChanged;
	FMosesOnDeadChangedNative OnDeadChanged;

	// =========================================================================
	// Dead Hook (서버에서만 확정)
	// =========================================================================
	void ServerMarkDead();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// =========================================================================
	// RepNotifies
	// =========================================================================
	UFUNCTION() void OnRep_CurrentSlot();
	UFUNCTION() void OnRep_Slot1WeaponId();
	UFUNCTION() void OnRep_Slot2WeaponId();
	UFUNCTION() void OnRep_Slot3WeaponId();

	UFUNCTION() void OnRep_Slot1Ammo();
	UFUNCTION() void OnRep_Slot2Ammo();
	UFUNCTION() void OnRep_Slot3Ammo();

	UFUNCTION() void OnRep_IsDead();

private:
	// =========================================================================
	// Broadcast helpers (RepNotify → Delegate)
	// =========================================================================
	void BroadcastEquippedChanged(const TCHAR* ContextTag);
	void BroadcastAmmoChanged(const TCHAR* ContextTag);
	void BroadcastDeadChanged(const TCHAR* ContextTag);

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

	// [FIX] 몽타주 길이 기반 쿨다운 제거 -> DefaultFireIntervalSec 기반
	float Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* WeaponData) const;

	bool Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const;
	void Server_UpdateFireCooldownStamp();

	void Server_PerformHitscanAndApplyDamage(const UMosesWeaponData* WeaponData);
	void Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId);

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

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

private:
	// =========================================================================
	// Runtime guards / stamps
	// =========================================================================
	UPROPERTY(Transient)
	bool bInitialized_Day2 = false;

	// 슬롯별 발사 쿨다운 타임스탬프(서버 시간)
	UPROPERTY(Transient)
	double Slot1LastFireTimeSec = -9999.0;

	UPROPERTY(Transient)
	double Slot2LastFireTimeSec = -9999.0;

	UPROPERTY(Transient)
	double Slot3LastFireTimeSec = -9999.0;

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

	// [FIX] 서버 쿨다운은 이제 이 값이 기준(=연사 속도)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultFireIntervalSec = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultDamage = 25.0f;
};
