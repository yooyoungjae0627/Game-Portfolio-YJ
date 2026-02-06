// ============================================================================
// UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmo.h  (FULL - UPDATED)
// ----------------------------------------------------------------------------
// 역할
// - 탄약 픽업 액터(월드 배치)
// - Overlap Begin/End:
//   * 로컬: 하이라이트 + 프롬프트 표시 + Interaction Target Set/Clear
// - E Press는 InteractionComponent가 서버 RPC로 전달
// - 서버에서만 CombatComponent에 탄약 증가 적용 (SSOT)
// - BP 그래프에서 탄약 증가/슬롯 변경/HUD 갱신 절대 금지
//
// UI
// - WeaponPickup과 동일하게 UWidgetComponent로 "월드 프롬프트" 표시
// - 빌보드(카메라 바라보기)는 Timer 기반 (Tick 금지)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupAmmo.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

class UMosesPickupAmmoData;
class UMosesPickupPromptWidget;

class AMosesPlayerState;
class UMosesInteractionComponent;
class APawn;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupAmmo : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupAmmo();

	// 로컬 하이라이트(코스메틱)
	void SetLocalHighlight(bool bEnable);

	/** 서버에서만 호출: 픽업 시도 → 성공하면 Destroy */
	bool ServerTryPickup(AMosesPlayerState* PickerPS, FText& OutAnnounceText);

	UMosesPickupAmmoData* GetPickupData() const { return PickupData; }

protected:
	virtual void BeginPlay() override;

private:
	// Overlap
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

	// Prompt
	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();

	UMosesInteractionComponent* GetInteractionComponentFromPawn(APawn* Pawn) const;

	// Billboard (Local only, Tick 금지)
	void StartPromptBillboard_Local();
	void StopPromptBillboard_Local();
	void TickPromptBillboard_Local();
	void ApplyBillboardRotation_Local();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent = nullptr;

	/** BP에서 지정: DA_Pickup_Ammo_* */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	TObjectPtr<UMosesPickupAmmoData> PickupData = nullptr;

	// 원자성 방지 (OK 1 / FAIL 1)
	UPROPERTY()
	bool bConsumed = false;

	// 로컬 프롬프트 대상 Pawn 캐시
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;

	// Billboard timer
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Prompt")
	float PromptBillboardInterval = 0.033f; // ~30fps

	FTimerHandle TimerHandle_PromptBillboard;
};
