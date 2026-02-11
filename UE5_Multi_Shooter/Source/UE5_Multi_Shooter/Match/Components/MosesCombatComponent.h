// ============================================================================
// UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h  (FULL / COMPILE FIXED)
// - Owner = AMosesPlayerState (SSOT)
// - Server authoritative: weapon/ammo/equip/fire/reload/damage apply
// - Client: 요청만(RPC), 결과는 RepNotify -> Delegate 로 UI 갱신
//
// [FIX SUMMARY]
// 1) Delegate를 "Native Multicast(DECLARE_MULTICAST_DELEGATE*)"로 통일
//    -> HUD/Widget/Character 쪽 AddUObject() 그대로 사용 가능
//    -> (Dynamic multicast는 AddUObject가 없어서 너 로그처럼 터짐)
// 2) cpp에 구현되어 있는데 h에 선언이 없던 함수들 선언 추가
//    -> GetCurrentSlot / IsDead / IsReloading / GetMagAmmoForSlot / GetReserveAmmoForSlot / GetReserveMaxForSlot
// 3) EMosesAmmoType 중복 정의 방지
//    -> MosesWeaponTypes.h 에 정의된 enum을 그대로 사용(여기서 재정의 X)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponTypes.h" // ✅ EMosesAmmoType here
#include "MosesCombatComponent.generated.h"

class AMosesPlayerState;
class APlayerCharacter;
class AMosesGrenadeProjectile;
class AMosesZombieCharacter;
class UMosesWeaponData;
class UGameplayEffect;
class UAbilitySystemComponent;

// ============================================================================
// Fire Guard fail reasons (서버 Fire 가드 실패 원인)
// ============================================================================
UENUM(BlueprintType)
enum class EMosesFireGuardFailReason : uint8
{
	None,
	IsDead,
	InvalidPhase,
	NoOwner,
	NoPawn,
	NoController,
	NoWeaponId,
	NoAmmo,
};

// ============================================================================
// Delegates (C++ Only)
// - 너 프로젝트 코드가 AddUObject()로 구독하고 있으므로 Native Multicast로 선언
// - (DECLARE_DYNAMIC_MULTICAST_DELEGATE는 AddUObject가 없음)
// ============================================================================
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*CurrentSlot*/, FGameplayTag /*WeaponId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChanged, int32 /*Mag*/, int32 /*ReserveCur*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnAmmoChangedEx, int32 /*Mag*/, int32 /*ReserveCur*/, int32 /*ReserveMax*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnDeadChanged, bool /*bDead*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnReloadingChanged, bool /*bReloading*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMosesOnSwapStarted, int32 /*FromSlot*/, int32 /*ToSlot*/, int32 /*Serial*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnSlotsStateChanged, int32 /*ChangedSlotOr0ForAll*/);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent();

	// =========================================================================
	// Engine
	// =========================================================================
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// =========================================================================
	// ✅ Simple Getters (다른 파일들이 이미 호출 중이라 반드시 제공)
	// - [출처] MosesMatchHUD.cpp / PlayerCharacter.cpp / MosesFlagSpot.cpp 등
	// =========================================================================
	FORCEINLINE int32 GetCurrentSlot() const { return CurrentSlot; }
	FORCEINLINE bool  IsDead() const { return bIsDead; }
	FORCEINLINE bool  IsReloading() const { return bIsReloading; }

	// =========================================================================
	// SSOT Query (읽기 전용)
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Combat|Query")
	FGameplayTag GetWeaponIdForSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Query")
	FGameplayTag GetEquippedWeaponId() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Query")
	int32 GetCurrentMagAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Query")
	int32 GetCurrentReserveAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Query")
	int32 GetCurrentReserveMax() const;

	// =========================================================================
	// ✅ Slot Query (HUD 슬롯 패널 갱신용)
	// - [출처] MosesCombatComponent.cpp 하단에 이미 정의돼있는데,
	//          h에 선언이 없어서 "멤버가 아니다" 컴파일 에러가 났던 부분
	// =========================================================================
	int32 GetMagAmmoForSlot(int32 SlotIndex) const;
	int32 GetReserveAmmoForSlot(int32 SlotIndex) const;
	int32 GetReserveMaxForSlot(int32 SlotIndex) const;

	// =========================================================================
	// Init / Loadout (Server)
	// =========================================================================
	void ServerInitDefaultSlots_4(
		const FGameplayTag& InSlot1,
		const FGameplayTag& InSlot2,
		const FGameplayTag& InSlot3,
		const FGameplayTag& InSlot4);

	void ServerGrantDefaultRifleAmmo_30_90();

	void ServerGrantWeaponToSlot(int32 SlotIndex, const FGameplayTag& WeaponId, bool bInitializeAmmoIfEmpty = true);

	// =========================================================================
	// Equip (Client Request -> Server Authority)
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Combat|Equip")
	void RequestEquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	// =========================================================================
	// Fire (Client Request -> Server Authority)
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Combat|Fire")
	void RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

	// =========================================================================
	// Reload (Client Request -> Server Authority)
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Combat|Reload")
	void RequestReload();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	// =========================================================================
	// Ammo Pickup (Server)
	// =========================================================================
	void ServerAddAmmoByType(EMosesAmmoType AmmoType, int32 ReserveMaxDelta, int32 ReserveFillDelta);
	void ServerAddAmmoByTag(FGameplayTag AmmoTypeId, int32 ReserveMaxDelta, int32 ReserveFillDelta);

	// =========================================================================
	// Dead hooks (Server)
	// =========================================================================
	void ServerMarkDead();
	void ServerClearDeadAfterRespawn();

	// =========================================================================
	// Delegates (HUD/Widget/Character는 이것만 AddUObject로 구독)
	// =========================================================================
	FMosesOnEquippedChanged    OnEquippedChanged;
	FMosesOnAmmoChanged        OnAmmoChanged;
	FMosesOnAmmoChangedEx      OnAmmoChangedEx;
	FMosesOnDeadChanged        OnDeadChanged;
	FMosesOnReloadingChanged   OnReloadingChanged;
	FMosesOnSwapStarted        OnSwapStarted;
	FMosesOnSlotsStateChanged  OnSlotsStateChanged;

	// =========================================================================
	// Tunables / Config
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage")
	float DefaultDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Fire")
	float DefaultFireIntervalSec = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	float HitscanDistance = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	TEnumAsByte<ECollisionChannel> FireTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	bool bServerTraceDebugDraw = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace", meta = (EditCondition = "bServerTraceDebugDraw"))
	float ServerTraceDebugDrawTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage")
	FName HeadshotBoneName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Reload")
	bool bAutoReloadOnEmpty = true;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile")
	float ProjectileSpeedFallback = 3000.f;

	// =========================================================================
	// Damage GE Soft Reference (optional; grenade uses this in your cpp)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Combat|GAS")
	TSoftClassPtr<UGameplayEffect> DamageGE_SetByCaller;

protected:
	// =========================================================================
	// Replicated SSOT fields (Owner = PlayerState)
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSlot)
	int32 CurrentSlot = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Slot1WeaponId) FGameplayTag Slot1WeaponId;
	UPROPERTY(ReplicatedUsing = OnRep_Slot2WeaponId) FGameplayTag Slot2WeaponId;
	UPROPERTY(ReplicatedUsing = OnRep_Slot3WeaponId) FGameplayTag Slot3WeaponId;
	UPROPERTY(ReplicatedUsing = OnRep_Slot4WeaponId) FGameplayTag Slot4WeaponId;

	// Ammo (Mag / ReserveCur / ReserveMax)
	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo) int32 Slot1MagAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo) int32 Slot1ReserveAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot1Ammo) int32 Slot1ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo) int32 Slot2MagAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo) int32 Slot2ReserveAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot2Ammo) int32 Slot2ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo) int32 Slot3MagAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo) int32 Slot3ReserveAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot3Ammo) int32 Slot3ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo) int32 Slot4MagAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo) int32 Slot4ReserveAmmo = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Slot4Ammo) int32 Slot4ReserveMax = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	bool bIsReloading = false;

	// Swap cosmetics trigger
	UPROPERTY(ReplicatedUsing = OnRep_SwapSerial)
	int32 SwapSerial = 0;

	UPROPERTY(Replicated)
	int32 LastSwapFromSlot = 1;

	UPROPERTY(Replicated)
	int32 LastSwapToSlot = 1;

	// 내부 플래그(초기화 여부)
	bool bInitialized_DefaultSlots = false;

	// Fire cooldown stamps (slot 1~4)
	double SlotLastFireTimeSec[4] = { 0,0,0,0 };

	FTimerHandle ReloadTimerHandle;

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

	UFUNCTION() void OnRep_IsReloading();
	UFUNCTION() void OnRep_IsDead();
	UFUNCTION() void OnRep_SwapSerial();

	// =========================================================================
	// Broadcast helpers (RepNotify -> Delegate)
	// =========================================================================
	void BroadcastEquippedChanged(const TCHAR* ContextTag);
	void BroadcastAmmoChanged(const TCHAR* ContextTag);
	void BroadcastDeadChanged(const TCHAR* ContextTag);
	void BroadcastReloadingChanged(const TCHAR* ContextTag);
	void BroadcastSwapStarted(const TCHAR* ContextTag);
	void BroadcastSlotsStateChanged(int32 ChangedSlotOr0ForAll, const TCHAR* ContextTag);

	// =========================================================================
	// Server internals (from your cpp)
	// =========================================================================
	bool Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const;

	const UMosesWeaponData* Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const;

	bool Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const;
	void Server_UpdateFireCooldownStamp();
	float Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* WeaponData) const;

	void Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData);

	float Server_CalcSpreadFactor01(const UMosesWeaponData* WeaponData, const APawn* OwnerPawn) const;
	FVector Server_ApplySpreadToDirection(const FVector& AimDir, const UMosesWeaponData* WeaponData, float SpreadFactor01, float& OutHalfAngleDeg) const;

	void Server_PerformFireAndApplyDamage(const UMosesWeaponData* WeaponData);

	void Server_StartReload(const UMosesWeaponData* WeaponData);
	void Server_FinishReload();

	// Slot helpers
	bool IsValidSlotIndex(int32 SlotIndex) const;
	FGameplayTag GetSlotWeaponIdInternal(int32 SlotIndex) const;
	void Server_SetSlotWeaponId_Internal(int32 SlotIndex, const FGameplayTag& WeaponId);

	// Ammo init helpers
	void Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId);

	// Ammo getters/setters (ReserveMax 포함)
	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur, int32& OutReserveMax) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur, int32 NewReserveMax);

	// overload (기존 호환)
	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur);

	// Projectile
	void Server_SpawnGrenadeProjectile(
		const UMosesWeaponData* WeaponData,
		const FVector& SpawnLoc,
		const FVector& FireDir,
		AController* InstigatorController,
		APawn* OwnerPawn);

	// Damage apply
	bool Server_IsZombieTarget(const AActor* TargetActor) const;

	bool Server_ApplyDamageToTarget_GAS(
		AActor* TargetActor,
		float Damage,
		AController* InstigatorController,
		AActor* DamageCauser,
		const UMosesWeaponData* WeaponData,
		const FHitResult& Hit) const;

	// Cosmetics
	void Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId);

	// =========================================================================
	// Debug draw multicast
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
		const FVector& FinalImpact);
};
