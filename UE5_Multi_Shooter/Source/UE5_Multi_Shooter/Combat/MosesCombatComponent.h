#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MosesCombatComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FMosesOnEquippedChanged, int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/);

/**
 * UMosesCombatComponent
 *
 * [SSOT]
 * - Weapon Slot / CurrentSlot / WeaponId는 PlayerState가 단일 진실로 소유한다.
 *
 * [정책]
 * - Equip 결정권은 서버.
 * - 코스메틱(무기 스폰/부착)은 Pawn이 Delegate(OnEquippedChanged)로 처리한다.
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent();

	// ===== SSOT Query =====
	int32 GetCurrentSlot() const { return CurrentSlot; }
	FGameplayTag GetWeaponIdForSlot(int32 SlotIndex) const;
	FGameplayTag GetEquippedWeaponId() const;

	// ===== Equip API (Client → Server) =====
	void RequestEquipSlot(int32 SlotIndex);

	// 서버 권위 확정
	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(int32 SlotIndex);

	// 서버 전용: 기본 슬롯 무기 초기화(스모크 용)
	void ServerInitDefaultSlots(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3);

	// 서버 전용:“초기화 보장” (중복 호출 안전)
	void Server_EnsureInitialized_Day2();

	// ===== Delegates =====
	FMosesOnEquippedChanged OnEquippedChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// RepNotifies
	UFUNCTION() void OnRep_CurrentSlot();
	UFUNCTION() void OnRep_Slot1WeaponId();
	UFUNCTION() void OnRep_Slot2WeaponId();
	UFUNCTION() void OnRep_Slot3WeaponId();

private:
	void BroadcastEquippedChanged(const TCHAR* ContextTag);

	bool IsValidSlotIndex(int32 SlotIndex) const;
	FGameplayTag GetSlotWeaponIdInternal(int32 SlotIndex) const;

	// 서버에서 WeaponId -> DataAsset Resolve 로그 증거
	void LogResolveWeaponData_Server(const FGameplayTag& WeaponId) const;

private:
	// -----------------------
	// SSOT Replicated Variables
	// -----------------------
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSlot)
	int32 CurrentSlot = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Slot1WeaponId)
	FGameplayTag Slot1WeaponId;

	UPROPERTY(ReplicatedUsing = OnRep_Slot2WeaponId)
	FGameplayTag Slot2WeaponId;

	UPROPERTY(ReplicatedUsing = OnRep_Slot3WeaponId)
	FGameplayTag Slot3WeaponId;

private:
	// -----------------------
	// Runtime guards
	// -----------------------
	UPROPERTY(Transient)
	bool bInitialized_Day2 = false;
};
