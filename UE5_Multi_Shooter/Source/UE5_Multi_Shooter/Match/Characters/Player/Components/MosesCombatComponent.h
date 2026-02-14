// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Player/Components/MosesCombatComponent.h
// (FULL - UPDATED / CLEAN / COMMENTED)
// ----------------------------------------------------------------------------
// [FIX 핵심]
// - Client FireHeld Heartbeat: PlayerState Owner 패턴에 맞게 Pawn resolve 수정
// - Heartbeat 타이머 멤버/이름 불일치 제거(컴파일 에러 제거)
// - ClientTick에서 Server_UpdateFireHeldHeartbeat() 주기적으로 전송
// - Server AutoFireTick에서 Heartbeat timeout 시 강제 Stop (연사 고착 방지)
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
class AController;
class APawn;

// ============================================================================
// Delegates (Native Only - UI는 여기만 구독)
// ============================================================================

DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChangedNative, int32 /*Mag*/, int32 /*ReserveCurrent*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnAmmoChangedExNative, int32 /*Mag*/, int32 /*ReserveCurrent*/, int32 /*ReserveMax*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnDeadChangedNative, bool /*bNewDead*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnReloadingChangedNative, bool /*bReloading*/);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnSwapStartedNative, int32 /*FromSlot*/, int32 /*ToSlot*/, int32 /*Serial*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnSlotsStateChangedNative, int32 /*ChangedSlotOr0ForAll*/);

// ============================================================================
// Fire Guard Fail Reason
// ============================================================================

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

// ============================================================================
// UMosesCombatComponent (Owner=AMosesPlayerState 가정)
// ============================================================================

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
	int32 GetCurrentReserveMax() const;

	bool IsDead() const { return bIsDead; }
	bool IsReloading() const { return bIsReloading; }

	// =========================================================================
	// Slot Query (UI/코스메틱용)
	// =========================================================================
	int32 GetLastSwapFromSlot() const { return LastSwapFromSlot; }
	int32 GetLastSwapToSlot() const { return LastSwapToSlot; }
	int32 GetSwapSerial() const { return SwapSerial; }

	int32 GetMagAmmoForSlot(int32 SlotIndex) const;
	int32 GetReserveAmmoForSlot(int32 SlotIndex) const;
	int32 GetReserveMaxForSlot(int32 SlotIndex) const;

	// =========================================================================
	// Equip
	// =========================================================================
	void RequestEquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	// =========================================================================
	// Fire (단발)
	// =========================================================================
	void RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

	// =========================================================================
	// Fire (연사)
	// =========================================================================
	void RequestStartFire();
	void RequestStopFire();

	UFUNCTION(Server, Reliable)
	void ServerStartFire();

	UFUNCTION(Server, Reliable)
	void ServerStopFire();

	// =========================================================================
	// Reload
	// =========================================================================
	void RequestReload();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	// =========================================================================
	// AutoFire Heartbeat (Client -> Server Fail-safe)
	// - 클라이언트가 "버튼을 계속 누르고 있다"는 것을 서버에 주기적으로 알림
	// - 서버는 heartbeat timeout이면 연사 강제 종료
	// =========================================================================
	UFUNCTION(Server, Unreliable)
	void Server_UpdateFireHeldHeartbeat();

	void StartClientFireHeldHeartbeat();
	void StopClientFireHeldHeartbeat();

	// 타이머에서 호출(클라 전용): (1) 서버로 heartbeat 전송 (2) LMB 업이면 Stop 강제
	void ClientFireHeldHeartbeatTick();

	// =========================================================================
	// Default Init / Loadout (Server only)
	// =========================================================================
	void ServerInitDefaultSlots_4(
		const FGameplayTag& InSlot1,
		const FGameplayTag& InSlot2,
		const FGameplayTag& InSlot3,
		const FGameplayTag& InSlot4);

	void ServerGrantDefaultRifleAmmo_30_90();

	// =========================================================================
	// Pickup / Grant (Server only)
	// =========================================================================
	void ServerGrantWeaponToSlot(int32 SlotIndex, const FGameplayTag& WeaponId, bool bInitializeAmmoIfEmpty = true);
	void ServerAddAmmoByType(EMosesAmmoType AmmoType, int32 ReserveMaxDelta, int32 ReserveFillDelta);
	void ServerAddAmmoByTag(FGameplayTag AmmoTypeId, int32 ReserveMaxDelta, int32 ReserveFillDelta);

	// =========================================================================
	// Dead / Respawn (Server only)
	// =========================================================================
	void ServerMarkDead();
	void ServerClearDeadAfterRespawn();

	// =========================================================================
	// Delegates
	// =========================================================================
	FMosesOnEquippedChanged         OnEquippedChanged;
	FMosesOnAmmoChangedNative       OnAmmoChanged;
	FMosesOnAmmoChangedExNative     OnAmmoChangedEx;
	FMosesOnDeadChangedNative       OnDeadChanged;
	FMosesOnReloadingChangedNative  OnReloadingChanged;
	FMosesOnSlotsStateChangedNative OnSlotsStateChanged;
	FMosesOnSwapStartedNative       OnSwapStarted;

	void BroadcastSlotsStateChanged(int32 ChangedSlotOr0ForAll, const TCHAR* ContextTag);

	// =========================================================================
	// Debug
	// =========================================================================
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	UFUNCTION() void OnRep_SwapSerial();

	// =========================================================================
	// Delegate Emit Helpers
	// =========================================================================
	void BroadcastEquippedChanged(const TCHAR* ContextTag);
	void BroadcastAmmoChanged(const TCHAR* ContextTag);
	void BroadcastDeadChanged(const TCHAR* ContextTag);
	void BroadcastReloadingChanged(const TCHAR* ContextTag);
	void BroadcastSwapStarted(const TCHAR* ContextTag);

	// =========================================================================
	// Slot Helpers
	// =========================================================================
	bool IsValidSlotIndex(int32 SlotIndex) const;
	FGameplayTag GetSlotWeaponIdInternal(int32 SlotIndex) const;

	void Server_SetSlotWeaponId_Internal(int32 SlotIndex, const FGameplayTag& WeaponId);

	void Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId);

	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur, int32& OutReserveMax) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur, int32 NewReserveMax);

	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur);

	// =========================================================================
	// Fire Core (Server only)
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

	void Server_SpawnGrenadeProjectile(
		const UMosesWeaponData* WeaponData,
		const FVector& SpawnLoc,
		const FVector& FireDir,
		AController* InstigatorController,
		APawn* OwnerPawn);

	bool Server_ApplyDamageToTarget_GAS(
		AActor* TargetActor,
		float Damage,
		AController* InstigatorController,
		AActor* DamageCauser,
		const UMosesWeaponData* WeaponData,
		const FHitResult& Hit) const;

	void Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId);

	// =========================================================================
	// Reload Core (Server only)
	// =========================================================================
	void Server_StartReload(const UMosesWeaponData* WeaponData);
	void Server_FinishReload();

	// =========================================================================
	// Target Policy
	// =========================================================================
	bool Server_IsZombieTarget(const AActor* TargetActor) const;

	// =========================================================================
	// AutoFire (Server only)
	// =========================================================================
	void StartAutoFire_Server(float IntervalSec);
	void StopAutoFire_Server();
	void AutoFireTick_Server();

private:
	// =========================================================================
	// Replicated SSOT
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

	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo)
	int32 Slot1ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo)
	int32 Slot2ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo)
	int32 Slot3ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4MagAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4ReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo)
	int32 Slot4ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	bool bIsReloading = false;

	UPROPERTY(ReplicatedUsing = OnRep_SwapSerial)
	int32 SwapSerial = 0;

	UPROPERTY(Replicated)
	int32 LastSwapFromSlot = 1;

	UPROPERTY(Replicated)
	int32 LastSwapToSlot = 1;

	// =========================================================================
	// Transient (Server runtime)
	// =========================================================================
	UPROPERTY(Transient)
	bool bInitialized_DefaultSlots = false;

	UPROPERTY(Transient)
	double SlotLastFireTimeSec[4] = { -9999.0, -9999.0, -9999.0, -9999.0 };

	FTimerHandle ReloadTimerHandle;

	// =========================================================================
	// Tunables
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
	float DefaultFireIntervalSec = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS")
	TSoftClassPtr<UGameplayEffect> DamageGE_SetByCaller;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire|Debug")
	bool bServerTraceDebugDraw = true;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire|Debug")
	float ServerTraceDebugDrawTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Reload")
	bool bAutoReloadOnEmpty = true;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Grenade")
	TSubclassOf<AMosesGrenadeProjectile> GrenadeProjectileClass;

	// =========================================================================
	// AutoFire (Server runtime)
	// =========================================================================
	FTimerHandle AutoFireTimerHandle;

	// “Stop 이후에도 남아있는 Timer tick” 무력화 토큰
	uint32 FireRequestToken = 0;
	uint32 ActiveFireToken = 0;

	float CachedAutoFireIntervalSec = 0.12f;

	UPROPERTY(Transient)
	bool bWantsToFire = false;

	// 서버에서 마지막으로 "버튼 유지중" 신호를 받은 시간
	UPROPERTY(Transient)
	double LastFireHeldHeartbeatSec = -9999.0;

	// 서버: heartbeat 끊기면 연사 강제 종료까지 허용 시간
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float FireHeldHeartbeatTimeoutSec = 0.25f; // 0.2~0.35 추천

	// 클라: 버튼 누르는 동안 heartbeat를 보내는 타이머
	FTimerHandle ClientFireHeldHeartbeatHandle;

	// 클라: heartbeat 동작 여부(타이머 active로도 판단 가능하지만, 디버깅용으로 둠)
	UPROPERTY(Transient)
	bool bClientFireHeldHeartbeatRunning = false;

	// 클라: heartbeat 주기(너무 빠를 필요 없음)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float ClientFireHeldHeartbeatIntervalSec = 0.05f; // 20Hz
};
