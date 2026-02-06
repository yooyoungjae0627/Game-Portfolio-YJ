// ============================================================================
// MosesPickupAmmo.h (FULL)
// ----------------------------------------------------------------------------
// 역할
// - 탄약 픽업 액터(월드 배치)
// - Overlap Begin/End에서 로컬 InteractionComponent 타겟 Set/Clear
// - E Press는 InteractionComponent가 서버로 전달
// - 서버에서만 CombatComponent에 Ammo 증가 적용 (SSOT)
// - BP에는 탄약 증가 로직 없음 (절대 금지)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupAmmo.generated.h"

class USphereComponent;
class UStaticMeshComponent;

class UMosesPickupAmmoData;
class AMosesPlayerState;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupAmmo : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupAmmo();

	/** 서버에서만 호출: 픽업 시도 → 성공하면 Destroy */
	bool ServerTryPickup(AMosesPlayerState* PickerPS, FText& OutAnnounceText);

	UMosesPickupAmmoData* GetPickupData() const { return PickupData; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	/** BP에서 지정: DA_Pickup_Ammo_* */
	UPROPERTY(EditDefaultsOnly, Category="Moses|Pickup")
	TObjectPtr<UMosesPickupAmmoData> PickupData = nullptr;
};
