// ============================================================================
// UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h  (FULL - UPDATED)  [STEP4]
// ============================================================================
//
// [STEP4 목표]
// - 파밍(픽업) 성공 시 "소유 슬롯(SlotOwnership)"만 갱신하는 것이 아니라,
//   CombatComponent(SSOT)의 슬롯 WeaponId + Ammo 초기화까지 서버에서 확정한다.
// - 결과:
//   - 파밍 직후 등(Back_1~3) 시각화는 STEP2 규칙으로 자동 반영
//   - 1~4 스왑 시 HUD는 CurrentSlot 탄약으로 즉시 전환(STEP3)
//   - 해당 무기만 탄약 소비/사격 가능(STEP1)
//
// 정책:
// - Server Authority 100%
// - Pawn/HUD는 RepNotify -> Delegate 결과만 표시
//
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupWeapon.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UWidgetComponent;

class UMosesPickupWeaponData;
class UMosesPickupPromptWidget;
class AMosesPlayerState;
class UMosesInteractionComponent;
class APawn;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupWeapon : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupWeapon();

	void SetLocalHighlight(bool bEnable);

	/**
	 * ServerTryPickup
	 * - 서버 원자성 판정(OK 1 / FAIL 1)
	 * - 성공 시:
	 *   1) SlotOwnershipComponent에 슬롯 소유 기록
	 *   2) CombatComponent(SSOT)에 Slot WeaponId + Ammo 초기화까지 확정   // ✅ [STEP4]
	 */
	bool ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereEndOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();

	bool CanPickup_Server(const AMosesPlayerState* RequesterPS) const;
	UMosesInteractionComponent* GetInteractionComponentFromPawn(APawn* Pawn) const;

	// Billboard (Local only)
	void StartPromptBillboard_Local();
	void StopPromptBillboard_Local();
	void TickPromptBillboard_Local();
	void ApplyBillboardRotation_Local();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	TObjectPtr<UMosesPickupWeaponData> PickupData = nullptr;

	UPROPERTY()
	bool bConsumed = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Prompt")
	float PromptBillboardInterval = 0.033f; // ~30fps

	FTimerHandle TimerHandle_PromptBillboard;
};
