#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MosesCombatComponent.generated.h"

class UMosesWeaponData;
class AMosesPlayerState;
class APawn;
class AController;

/**
 * 델리게이트(네 기존 구조 유지)
 * - PlayerCharacter가 바인딩하는 EquippedChanged
 * - HUD가 바인딩하는 AmmoChanged
 * - (DAY3) DeadChanged
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMosesOnDeadChangedNative, bool /*bNewDead*/);

/**
 * [DAY3] Fire Guard 실패 사유 (증거 로그 고정용)
 */
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
 * UMosesCombatComponent (SSOT)
 * ============================================================================
 * [역할]
 * - PlayerState 소유 전투 단일 진실(SSOT)
 * - 서버 권위 Equip/Ammo/Fire 승인 + 서버 Hitscan + 서버 Damage
 * - UI는 RepNotify -> Delegate만 구독 (Tick/Binding 금지)
 *
 * [중요]
 * - 이 컴포넌트는 "인터페이스가 깨지면 프로젝트 전체가 깨진다".
 *   (PlayerState/PlayerCharacter가 여러 API에 의존)
 * - 따라서 Day3 기능을 추가하더라도 기존 Day1~Day2 API는 유지한다.
 * ============================================================================
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent();

	// =========================================================================
	// SSOT Query (PlayerCharacter/HUD에서 사용)
	// =========================================================================
	int32 GetCurrentSlot() const { return CurrentSlot; }
	FGameplayTag GetWeaponIdForSlot(int32 SlotIndex) const;
	FGameplayTag GetEquippedWeaponId() const;

	int32 GetCurrentMagAmmo() const;
	int32 GetCurrentReserveAmmo() const;

	// [ADD] Death SSOT 접근
	bool IsDead() const { return bIsDead; }

	// =========================================================================
	// Equip API (Client -> Server)
	// =========================================================================
	void RequestEquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	/**
	 * (서버 전용) 슬롯 기본 무기 세팅
	 * - 보통 Experience/PawnData/로드 결과로 서버에서 한 번 호출
	 */
	void ServerInitDefaultSlots(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3);

	/**
	 * DAY2 기존 호출 유지 (MosesPlayerState.cpp에서 호출됨)
	 * - LateJoin/재접속/초기화 타이밍 흔들림 방지용
	 */
	void Server_EnsureInitialized_Day2();

	// =========================================================================
	// Fire API (Client -> Server) [DAY3]
	// =========================================================================
	void RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

	// =========================================================================
	// Delegates
	// =========================================================================
	FMosesOnEquippedChanged OnEquippedChanged;
	FMosesOnAmmoChangedNative OnAmmoChanged;

	// [DAY3] 사망 상태 변경 이벤트 (Anim/HUD 연결용)
	FMosesOnDeadChangedNative OnDeadChanged;

	// =========================================================================
	// [ADD] 외부(HP 시스템)에서 서버가 사망을 확정할 때 호출할 수 있는 API
	// - 프로젝트마다 HP/데미지 SSOT 위치가 다르므로, Day3에서는 "Hook"만 제공한다.
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

	UFUNCTION() void OnRep_IsDead(); // [DAY3]

	// =========================================================================
	// Broadcast helpers (RepNotify → Delegate)
	// =========================================================================
	void BroadcastEquippedChanged(const TCHAR* ContextTag);
	void BroadcastAmmoChanged(const TCHAR* ContextTag);
	void BroadcastDeadChanged(const TCHAR* ContextTag); // [DAY3]

	// =========================================================================
	// Slot helpers
	// =========================================================================
	bool IsValidSlotIndex(int32 SlotIndex) const;
	FGameplayTag GetSlotWeaponIdInternal(int32 SlotIndex) const;

	// =========================================================================
	// Ammo helpers
	// =========================================================================
	void Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId);
	void GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserve) const;
	void SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserve);

	// =========================================================================
	// Fire helpers (Server only) [DAY3]
	// =========================================================================
	bool Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const;

	/**
	 * 무기 데이터 resolve는 "존재 여부 검증" 용도로만 사용.
	 * - WeaponData 구조가 프로젝트마다 달라 컴파일 깨질 수 있으니,
	 *   Day3에서는 WeaponData의 필드에 직접 의존하지 않는다.
	 */
	const UMosesWeaponData* Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const;

	void Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData);
	bool Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const;
	void Server_UpdateFireCooldownStamp();

	void Server_PerformHitscanAndApplyDamage(const UMosesWeaponData* WeaponData);

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

	// [DAY3] Death SSOT (서버만 변경, 복제)
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	// =========================================================================
	// Runtime guards
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

	// =========================================================================
	// Fire policy (DAY3 최소: 무기별 분기 전)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float HitscanDistance = 20000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	TEnumAsByte<ECollisionChannel> FireTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	FName HeadshotBoneName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float HeadshotDamageMultiplier = 2.0f; // [DAY3] 최소 헤드샷 배율

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultFireIntervalSec = 0.12f; // [DAY3] 무기 데이터 없을 때 기본 연사 간격

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Fire")
	float DefaultDamage = 25.0f; // [DAY3] 무기 데이터 없을 때 기본 데미지
};
