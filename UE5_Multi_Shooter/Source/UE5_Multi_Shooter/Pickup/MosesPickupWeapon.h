// ============================================================================
// MosesPickupWeapon.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// - 월드에 배치되는 픽업 액터
// - 로컬 하이라이트(CustomDepth) + 로컬 말풍선(WidgetComponent)은 클라 코스메틱.
// - 서버 원자성: bConsumed로 "OK 1 / FAIL 1" 보장.
// - 성공 시 PlayerState SSOT(슬롯 소유/ItemId) 갱신 + 중앙 Announcement 텍스트 반환.
//
// [MOD 핵심]
// - [MOD] Overlap 기반 타겟 시스템:
//   * 로컬 Pawn이 InteractSphere에 들어오면, Pawn의 UMosesInteractionComponent에
//     SetCurrentInteractTarget_Local(this)를 호출한다.
//   * 나가면 ClearCurrentInteractTarget_Local(this).
// - (라인트레이스/TraceTarget 의존 제거 가능)
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

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupWeapon : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupWeapon();

	// 클라: 로컬 하이라이트 On/Off (Overlap 등에서 호출)
	void SetLocalHighlight(bool bEnable);

	// 서버: 픽업 시도 (E)
	bool ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText);

protected:
	virtual void BeginPlay() override;

private:
	// ---- Overlap for local highlight + prompt + [MOD] target set/clear ----
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

	// ---- Prompt helpers ----
	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();

	// ---- Guards ----
	bool CanPickup_Server(const AMosesPlayerState* RequesterPS) const;

	// [MOD] interaction component resolve
	UMosesInteractionComponent* GetInteractionComponentFromPawn(APawn* Pawn) const;

private:
	// ---- Components ----
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	// ---- Data ----
	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	TObjectPtr<UMosesPickupWeaponData> PickupData;

	// ---- Server atomic ----
	UPROPERTY()
	bool bConsumed = false;

	// ---- Local prompt owner ----
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;
};
