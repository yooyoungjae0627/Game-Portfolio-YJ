// ============================================================================
// MosesPickupWeapon.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// - 월드에 배치되는 픽업 액터
// - 로컬 하이라이트(CustomDepth) + 로컬 말풍선(WidgetComponent)은 클라 코스메틱.
// - 서버 원자성: bConsumed로 "OK 1 / FAIL 1" 보장.
// - 성공 시 PlayerState SSOT(슬롯 소유/ItemId) 갱신 + 중앙 Announcement 텍스트 반환.
//
// [MOD 핵심]
// - Mesh: UStaticMeshComponent -> USkeletalMeshComponent
// - PickupData.WorldMesh: StaticMesh -> SkeletalMesh
// - [MOD] TraceTarget(Box): LineTrace(ECC_Visibility)로 선택 가능하도록 추가
//   (Mesh는 NoCollision 유지 가능. Trace는 TraceTarget이 담당)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupWeapon.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class UBoxComponent;

class UMosesPickupWeaponData;
class UMosesPickupPromptWidget;
class AMosesPlayerState;

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
	// ---- Overlap for local highlight + prompt ----
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

private:
	// ---- Components ----
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** [MOD] 월드 표시용 스켈레탈 메시 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** 근접 감지(프롬프트/하이라이트 트리거) */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere;

	/** [MOD] Trace 선택용 박스(Visibility Block) */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> TraceTarget;

	/** 월드 말풍선 UI */
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
