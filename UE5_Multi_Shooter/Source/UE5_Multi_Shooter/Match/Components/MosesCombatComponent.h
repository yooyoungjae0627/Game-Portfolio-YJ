// ============================================================================
// UE5_Multi_Shooter/Combat/MosesCombatComponent.h  (FULL - UPDATED)
// ============================================================================
//
// UMosesCombatComponent
// - SSOT(PlayerState) 소유 컴포넌트.
// - 슬롯(1~4) 고정 무기/탄약/리로드/사격/스왑을 서버 권위로 판정한다.
// - Pawn/HUD는 RepNotify -> Native Delegate 이벤트만 받아 "표시"만 한다.
//
// [이번 STEP1 작업 목표]
// - (핵심) 파밍으로 슬롯에 무기가 들어오면, 해당 슬롯의 탄약이 WeaponData 기준으로 초기화되어
//   스왑(1~4) 시 "그 무기의 탄약만" HUD에 표시되고, "그 무기의 탄약만" 소비되게 한다.
// - 이를 위해 서버 전용 API(ServerGrantWeaponToSlot)를 제공하고,
//   Server_EnsureAmmoInitializedForSlot가 실제로 WeaponData 기반 초기 탄약을 세팅하도록 수정한다.
// - Tick 금지, HUD Binding 금지. RepNotify -> Delegate만 사용.
//
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "MosesCombatComponent.generated.h"

class UMosesWeaponData;
class AMosesGrenadeProjectile;
class UGameplayEffect;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnDeadChangedNative, bool /*bNewDead*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnReloadingChangedNative, bool /*bReloading*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnSwapStartedNative, int32 /*FromSlot*/, int32 /*ToSlot*/, int32 /*Serial*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnSlotsStateChangedNative, int32 /*ChangedSlotOr0ForAll*/);

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

/**
 * UMosesCombatComponent
 *
 * 정책:
 * - Server Authority 100%
 * - SSOT = PlayerState 소유 컴포넌트
 * - Pawn = Cosmetic Only (손/등 소켓 Attach, 몽타주, VFX/SFX)
 * - HUD = RepNotify -> Delegate only
 */
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
	// SwapContext Query (Cosmetic trigger only)
	// =========================================================================
	int32 GetLastSwapFromSlot() const { return LastSwapFromSlot; }
	int32 GetLastSwapToSlot() const { return LastSwapToSlot; }
	int32 GetSwapSerial() const { return SwapSerial; }
	int32 GetMagAmmoForSlot(int32 SlotIndex) const;
	int32 GetReserveAmmoForSlot(int32 SlotIndex) const;

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
	// [MOD][STEP1] Pickup/Grant API (Server only)
	// - 파밍 성공 시, 해당 슬롯 WeaponId 세팅 + WeaponData 기반 탄약 초기화까지 SSOT에서 확정한다.
	// - Pawn/HUD는 RepNotify -> Delegate로 자동 갱신된다.
	// =========================================================================
	void ServerGrantWeaponToSlot(int32 SlotIndex, const FGameplayTag& WeaponId, bool bInitializeAmmoIfEmpty = true); // ✅ [MOD]

	// =========================================================================
	// Delegates (RepNotify -> Delegate)
	// =========================================================================
	FMosesOnEquippedChanged OnEquippedChanged;
	FMosesOnAmmoChangedNative OnAmmoChanged;
	FMosesOnDeadChangedNative OnDeadChanged;
	FMosesOnReloadingChangedNative OnReloadingChanged;
	FMosesOnSlotsStateChangedNative OnSlotsStateChanged;

	// 서버 승인 Swap 이벤트(코스메틱 트리거)
	FMosesOnSwapStartedNative OnSwapStarted;

	// =========================================================================
	// Dead Hook (서버에서만 확정)
	// =========================================================================
	void ServerMarkDead();

	// ✅ [ADD] 슬롯 패널 갱신 브로드캐스트 (0=전체, 1~4=특정)
	void BroadcastSlotsStateChanged(int32 ChangedSlotOr0ForAll, const TCHAR* ContextTag);


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

	// 서버 승인 Swap 컨텍스트 RepNotify
	UFUNCTION() void OnRep_SwapSerial();

private:
	// =========================================================================
	// Broadcast helpers (RepNotify → Delegate)
	// =========================================================================
	void BroadcastEquippedChanged(const TCHAR* ContextTag);
	void BroadcastAmmoChanged(const TCHAR* ContextTag);
	void BroadcastDeadChanged(const TCHAR* ContextTag);
	void BroadcastReloadingChanged(const TCHAR* ContextTag);

	void BroadcastSwapStarted(const TCHAR* ContextTag);

private:
	// =========================================================================
	// Slot / Ammo helpers
	// =========================================================================
	bool IsValidSlotIndex(int32 SlotIndex) const;
	FGameplayTag GetSlotWeaponIdInternal(int32 SlotIndex) const;

	// [MOD] 실제 WeaponData 기준으로 초기 탄약을 세팅한다.
	void Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId); // ✅ [MOD]

	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserve) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserve);

	// [MOD] Slot WeaponId를 서버에서 세팅하고, 즉시 RepNotify/Delegate를 갱신한다.
	void Server_SetSlotWeaponId_Internal(int32 SlotIndex, const FGameplayTag& WeaponId); // ✅ [MOD]

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

	// 스프레드 적용 후 히트스캔 수행
	void Server_PerformFireAndApplyDamage(const UMosesWeaponData* WeaponData);

	// 스프레드 계산/적용
	float Server_CalcSpreadFactor01(const UMosesWeaponData* WeaponData, const APawn* OwnerPawn) const;
	FVector Server_ApplySpreadToDirection(const FVector& AimDir, const UMosesWeaponData* WeaponData, float SpreadFactor01, float& OutHalfAngleDeg) const;

	// Projectile 스폰(유탄)
	void Server_SpawnGrenadeProjectile(const UMosesWeaponData* WeaponData, const FVector& SpawnLoc, const FVector& FireDir, AController* InstigatorController, APawn* OwnerPawn);

	// GAS(SetByCaller Data.Damage) 우선 적용 + ASC 없으면 ApplyDamage fallback
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
	// SwapContext (Server approved) - Cosmetic trigger only
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

private:
	// =========================================================================
	// Projectile class
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Grenade")
	TSubclassOf<AMosesGrenadeProjectile> GrenadeProjectileClass;
};
