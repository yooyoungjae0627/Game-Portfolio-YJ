// ============================================================================
// UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h  (FULL - UPDATED)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponTypes.h" // EMosesAmmoType
#include "MosesCombatComponent.generated.h"

class UMosesWeaponData;
class AMosesGrenadeProjectile;
class UGameplayEffect;

// 기존 2파라미터 유지 (호환)
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChangedNative, int32 /*Mag*/, int32 /*ReserveCurrent*/);

// [MOD] ReserveMax 포함 확장 델리게이트
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnAmmoChangedExNative, int32 /*Mag*/, int32 /*ReserveCurrent*/, int32 /*ReserveMax*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);
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
	int32 GetCurrentReserveAmmo() const;     // ReserveCurrent
	int32 GetCurrentReserveMax() const;      // [MOD] ReserveMax

	bool IsDead() const { return bIsDead; }
	bool IsReloading() const { return bIsReloading; }

	// =========================================================================
	// Slot Query
	// =========================================================================
	int32 GetLastSwapFromSlot() const { return LastSwapFromSlot; }
	int32 GetLastSwapToSlot() const { return LastSwapToSlot; }
	int32 GetSwapSerial() const { return SwapSerial; }
	int32 GetMagAmmoForSlot(int32 SlotIndex) const;
	int32 GetReserveAmmoForSlot(int32 SlotIndex) const;     // ReserveCurrent
	int32 GetReserveMaxForSlot(int32 SlotIndex) const;      // [MOD]

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

	// [MOD] 기본 탄약 지급(예: 라이플 30/90 + ReserveMax=90)
	void ServerGrantDefaultRifleAmmo_30_90();

	// =========================================================================
	// Pickup/Grant API (Server only)
	// =========================================================================
	void ServerGrantWeaponToSlot(int32 SlotIndex, const FGameplayTag& WeaponId, bool bInitializeAmmoIfEmpty = true);

	// [MOD][AMMO_PICKUP] 탄약 파밍(타입 기반) - 서버 전용
	// - ReserveMax 증가 + ReserveCurrent 증가(Clamp)
	void ServerAddAmmoByType(EMosesAmmoType AmmoType, int32 ReserveMaxDelta, int32 ReserveFillDelta);

	// =========================================================================
	// Delegates (RepNotify -> Delegate)
	// =========================================================================
	FMosesOnEquippedChanged OnEquippedChanged;

	// 기존 HUD 호환(2 params)
	FMosesOnAmmoChangedNative OnAmmoChanged;

	// [MOD] 신규 HUD 권장(3 params: Mag/Cur/Max)
	FMosesOnAmmoChangedExNative OnAmmoChangedEx;

	FMosesOnDeadChangedNative OnDeadChanged;
	FMosesOnReloadingChangedNative OnReloadingChanged;
	FMosesOnSlotsStateChangedNative OnSlotsStateChanged;
	FMosesOnSwapStartedNative OnSwapStarted;

	// =========================================================================
	// Dead Hook (서버에서만 확정)
	// =========================================================================
	void ServerMarkDead();
	void ServerClearDeadAfterRespawn();

	void BroadcastSlotsStateChanged(int32 ChangedSlotOr0ForAll, const TCHAR* ContextTag);

	// [ADD][AMMO_TAG] PickupAmmo가 Tag 기반일 때 호출 (내부에서 enum으로 매핑해서 기존 로직 재사용)
	void ServerAddAmmoByTag(FGameplayTag AmmoTypeId, int32 ReserveMaxDelta, int32 ReserveFillDelta);


	// ============================================================================
	// [MOD][DEBUG] Trace Debug (Server -> All Clients)
	// - 서버에서 계산한 CamTrace / MuzzleTrace 결과를 멀티캐스트로 뿌려서
	//   "클라 창에서도" 반드시 보이게 만든다.
	// ============================================================================

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DrawFireTraceDebug(
		const FVector& CamStart,
		const FVector& CamEnd,
		bool bCamHit,
		const FVector& CamImpact,
		const FVector& MuzzleStart,
		const FVector& MuzzleEnd,
		bool bFinalHit,
		const FVector& FinalImpact
	);

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

	// [MOD] SlotXAmmo RepNotify가 Mag/ReserveCur/ReserveMax를 함께 갱신
	UFUNCTION() void OnRep_Slot1Ammo();
	UFUNCTION() void OnRep_Slot2Ammo();
	UFUNCTION() void OnRep_Slot3Ammo();
	UFUNCTION() void OnRep_Slot4Ammo();

	UFUNCTION() void OnRep_IsDead();
	UFUNCTION() void OnRep_IsReloading();
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

	void Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId);

	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur, int32& OutReserveMax) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur, int32 NewReserveMax);

	// [MOD][COMPAT] 기존 3인수 호출 호환 오버로드
	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur);

	void Server_SetSlotWeaponId_Internal(int32 SlotIndex, const FGameplayTag& WeaponId);

private:
	// =========================================================================
	// Fire helpers (Server only)  (AMMO 작업에서는 "선언만" 필요)
	// =========================================================================
	bool Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const;
	const UMosesWeaponData* Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const;

	void Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData);

	float Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* WeaponData) const;
	bool Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const;
	void Server_UpdateFireCooldownStamp();

	void Server_PerformFireAndApplyDamage(const UMosesWeaponData* WeaponData);

	float Server_CalcSpreadFactor01(const UMosesWeaponData* WeaponData, const APawn* OwnerPawn) const;
	FVector Server_ApplySpreadToDirection(const FVector& AimDir, const UMosesWeaponData* WeaponData, float SpreadFactor01, float& OutHalfAngleDeg) const;

	void Server_SpawnGrenadeProjectile(const UMosesWeaponData* WeaponData, const FVector& SpawnLoc, const FVector& FireDir, AController* InstigatorController, APawn* OwnerPawn);

	bool Server_ApplyDamageToTarget_GAS(
		AActor* TargetActor,
		float Damage,
		AController* InstigatorController,
		AActor* DamageCauser,
		const UMosesWeaponData* WeaponData,
		const FHitResult& Hit) const;

	void Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId);

private:
	// =========================================================================
	// Reload helpers (Server only) (AMMO 작업에서는 "선언만" 필요)
	// =========================================================================
	void Server_StartReload(const UMosesWeaponData* WeaponData);
	void Server_FinishReload();


private:
	bool Server_IsZombieTarget(const AActor* TargetActor) const;

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

	// -------------------- Slot 1 Ammo --------------------
	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo)
	int32 Slot1MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo)
	int32 Slot1ReserveAmmo = 0; // ReserveCurrent

	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo)
	int32 Slot1ReserveMax = 0;  // [MOD]

	// -------------------- Slot 2 Ammo --------------------
	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2ReserveMax = 0;  // [MOD]

	// -------------------- Slot 3 Ammo --------------------
	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3ReserveMax = 0;  // [MOD]

	// -------------------- Slot 4 Ammo --------------------
	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4ReserveMax = 0;  // [MOD]

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	bool bIsReloading = false;

	// SwapContext (Server approved) - Cosmetic trigger only
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
	// Fire policy (AMMO 작업에서는 "선언만" 필요)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float HitscanDistance = 20000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	TEnumAsByte<ECollisionChannel> FireTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	FName HeadshotBoneName = TEXT("Head");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float HeadshotDamageMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultFireIntervalSec = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS")
	TSoftClassPtr<class UGameplayEffect> DamageGE_SetByCaller;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire|Debug")
	bool bServerTraceDebugDraw = true;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire|Debug")
	float ServerTraceDebugDrawTime = 1.5f;

private:
	// =========================================================================
	// Reload policy
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Reload")
	bool bAutoReloadOnEmpty = true;

private:
	// =========================================================================
	// Projectile class
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Grenade")
	TSubclassOf<class AMosesGrenadeProjectile> GrenadeProjectileClass;
};
