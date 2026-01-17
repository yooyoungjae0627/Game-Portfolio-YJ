#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponTypes.h"
#include "MosesCombatComponent.generated.h"

/**
 * UI/HUD는 Tick을 사용하지 않는다.
 * RepNotify → Delegate 브로드캐스트를 통해 즉시 갱신한다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAmmoChanged,
	EMosesWeaponType, WeaponType,
	const FAmmoState&, NewState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponSlotChanged,
	EMosesWeaponType, WeaponType,
	const FWeaponSlotState&, NewSlot
);

/**
 * UMosesCombatComponent
 *
 * - PlayerState 소유 (Single Source of Truth)
 * - 무기 슬롯 / 탄약 상태를 서버 권위로 관리
 * - RepNotify + Delegate 기반 UI 갱신
 *
 * ⚠️ Lyra 스타일 핵심:
 *   - 상태는 PS에 있고
 *   - Pawn/Character는 "표시와 요청"만 수행
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/* =========================
	 * Engine
	 * ========================= */

	 /** 네트워크 복제 필드 등록 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* =========================
	 * Ammo (Server Authoritative)
	 * ========================= */

	 /** 해당 무기 타입의 탄약 상태가 유효한지 확인 */
	bool HasAmmoState(EMosesWeaponType WeaponType) const;

	/** 현재 탄약 상태 조회 (복사본 반환) */
	FAmmoState GetAmmoState(EMosesWeaponType WeaponType) const;

	/**
	 * 서버 전용: 탄약 상태 직접 설정
	 * - 값 보정(Clamp) 포함
	 * - Replication으로 클라 동기화
	 */
	void Server_SetAmmoState(EMosesWeaponType WeaponType, const FAmmoState& NewState);

	/**
	 * 서버 전용: 예비 탄약 증가
	 * - Pickup 보상 등에 사용
	 */
	void Server_AddReserveAmmo(EMosesWeaponType WeaponType, int32 AddAmount);

	/**
	 * 서버 전용: 탄창 탄약 소모
	 * @return 소모 성공 여부
	 */
	bool Server_ConsumeMagAmmo(EMosesWeaponType WeaponType, int32 ConsumeAmount);

	/* =========================
	 * Weapon Slot (Server Authoritative)
	 * ========================= */

	 /** 슬롯 상태 조회 */
	FWeaponSlotState GetWeaponSlot(EMosesWeaponType WeaponType) const;

	/** 서버 전용: 무기 슬롯 상태 설정 */
	void Server_SetWeaponSlot(EMosesWeaponType WeaponType, const FWeaponSlotState& NewSlot);

	/* =========================
	 * Delegates (UI/HUD)
	 * ========================= */

	 /**
	  * 탄약 상태 변경 이벤트
	  * - RepNotify에서도 호출됨
	  * - UI는 이것만 구독하면 됨 (Tick 금지)
	  */
	UPROPERTY(BlueprintAssignable)
	FOnAmmoChanged OnAmmoChanged;

	/** 무기 슬롯 변경 이벤트 */
	UPROPERTY(BlueprintAssignable)
	FOnWeaponSlotChanged OnWeaponSlotChanged;

private:
	/* =========================
	 * RepNotify
	 * ========================= */

	 /** AmmoStates 복제 도착 시 호출 */
	UFUNCTION()
	void OnRep_AmmoStates();

	/** WeaponSlots 복제 도착 시 호출 */
	UFUNCTION()
	void OnRep_WeaponSlots();

private:
	/* =========================
	 * Internal Helpers
	 * ========================= */

	 /**
	  * 기본값 초기화
	  * - 서버/클라 공통
	  * - Replication 이전에도 안전
	  * - 한 번만 실행됨
	  */
	void InitializeDefaultsIfNeeded();

	/**
	 * Replicated 배열 크기 보장
	 * - enum 개수 변경에 안전
	 */
	void EnsureReplicatedArrays();

	/** enum 항목 개수 계산 (MAX 제외) */
	int32 GetWeaponTypeCount() const;

	/** enum 값이 유효 범위인지 검사 */
	bool IsValidWeaponType(EMosesWeaponType WeaponType) const;

	/** enum → array index 변환 */
	int32 WeaponTypeToIndex(EMosesWeaponType WeaponType) const;

	/** 모든 탄약 상태 브로드캐스트 */
	void BroadcastAmmoAll() const;

	/** 모든 슬롯 상태 브로드캐스트 */
	void BroadcastSlotsAll() const;

	/** 음수 방지 유틸 */
	static int32 ClampNonNegative(int32 Value);

private:
	/* =========================
	 * Replicated Data
	 * =========================
	 *
	 * ⚠️ UE는 TMap Replication을 지원하지 않음
	 * → enum index 기반 배열 사용
	 * → Lyra에서도 흔히 쓰는 방식
	 */

	UPROPERTY(ReplicatedUsing = OnRep_AmmoStates)
	TArray<FAmmoState> AmmoStates;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponSlots)
	TArray<FWeaponSlotState> WeaponSlots;

	/**
	 * 기본값 중복 초기화 방지 플래그
	 * - 서버/클라 모두 사용
	 */
	bool bDefaultsInitialized = false;
};
