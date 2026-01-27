// ============================================================================
// MosesCombatComponent.h (FULL)
// - 서버 권위 SSOT 전투 컴포넌트
// - 발사 쿨다운은 WeaponData의 FireMontage 길이(초)로 계산하여
//   "애니가 끝나야 다음 발사"를 서버가 보장한다.
// - 서버 승인된 WeaponId를 Multicast 파라미터로 전파하여
//   총마다 다른 SFX/VFX를 클라에서 재생할 수 있게 한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MosesCombatComponent.generated.h"

class UMosesWeaponData;
class AMosesPlayerState;
class APawn;
class AController;

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

public:
	// =========================================================================
	// SSOT Query
	// =========================================================================
	int32 GetCurrentSlot() const { return CurrentSlot; }
	FGameplayTag GetWeaponIdForSlot(int32 SlotIndex) const;
	FGameplayTag GetEquippedWeaponId() const;

	int32 GetCurrentMagAmmo() const;
	int32 GetCurrentReserveAmmo() const;

	bool IsDead() const { return bIsDead; }

public:
	// =========================================================================
	// Equip API (Client -> Server)
	// =========================================================================
	void RequestEquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	// 서버에서 슬롯 기본 무기 세팅(Experience/PawnData 등에서 1회 호출)
	void ServerInitDefaultSlots(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3);

	// 기존 초기화 흐름 보강(늦게 들어오는 복제/Travel 타이밍 대비)
	void Server_EnsureInitialized_Day2();

public:
	// =========================================================================
	// Fire API (Client -> Server)
	// =========================================================================
	void RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

public:
	// =========================================================================
	// Delegates (RepNotify -> Delegate)
	// =========================================================================
	FMosesOnEquippedChanged OnEquippedChanged;
	FMosesOnAmmoChangedNative OnAmmoChanged;
	FMosesOnDeadChangedNative OnDeadChanged;

public:
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

	/**
	 * Server_CanFire
	 *
	 * [역할]
	 * - "기본 가드"만 수행한다.
	 * - WeaponData가 있어야 가능한 검증(쿨다운/몽타주 길이 등)은 ServerFire에서 WeaponData resolve 후 수행한다.
	 */
	bool Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const;

	/** 장착 WeaponId를 resolve하고 WeaponData를 가져온다. (서버 전용) */
	const UMosesWeaponData* Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const;

	/** 승인된 Fire에서만 탄약을 차감한다. (서버 전용) */
	void Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData);

	/** WeaponData의 FireMontage 길이(초)를 읽어 재발사 간격으로 사용한다. (서버 전용) */
	float Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* WeaponData) const;

	/** WeaponData 기반 쿨다운 체크 (서버 전용) */
	bool Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const;

	/** 승인된 순간 서버 타임스탬프를 갱신한다. */
	void Server_UpdateFireCooldownStamp();

	/** 히트스캔 + 데미지 적용 (서버 전용) */
	void Server_PerformHitscanAndApplyDamage(const UMosesWeaponData* WeaponData);

	/** 승인된 WeaponId를 Multicast 파라미터로 전달하여 코스메틱을 전파한다. */
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

	// WeaponData의 몽타주 길이를 못 읽는 경우 fallback
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultFireIntervalSec = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultDamage = 25.0f;
};
