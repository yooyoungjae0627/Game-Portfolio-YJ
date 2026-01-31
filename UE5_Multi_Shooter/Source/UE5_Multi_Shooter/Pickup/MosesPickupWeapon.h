// ============================================================================
// MosesPickupWeapon.h (FULL)
// - 월드에 배치되는 픽업 액터
// - 로컬 하이라이트(CustomDepth)는 클라 코스메틱.
// - 서버 원자성: bConsumed로 "OK 1 / FAIL 1" 보장.
// - 성공 시 PlayerState SSOT(슬롯 소유/ItemId) 갱신 -> HUD는 RepNotify/Delegate로 갱신.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupWeapon.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMosesPickupWeaponData;
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
	bool ServerTryPickup(AMosesPlayerState* RequesterPS);

protected:
	virtual void BeginPlay() override;

private:
	// ---- Overlap for local highlight hint ----
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

	// ---- Guards ----
	bool CanPickup_Server(const AMosesPlayerState* RequesterPS) const;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere;

	// ---- Data ----
	UPROPERTY(EditDefaultsOnly, Category="Pickup")
	TObjectPtr<UMosesPickupWeaponData> PickupData;

	// ---- Server atomic ----
	UPROPERTY()
	bool bConsumed = false;
};
