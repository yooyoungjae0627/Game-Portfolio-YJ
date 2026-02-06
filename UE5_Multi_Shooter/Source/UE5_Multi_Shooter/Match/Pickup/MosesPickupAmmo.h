#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponTypes.h" // EMosesAmmoType
#include "MosesPickupAmmo.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AMosesPlayerState;

/**
 * AMosesPickupAmmo
 *
 * - 상호작용(E)로 획득되는 탄약 픽업
 * - 서버에서만:
 *   ReserveMax += Delta
 *   ReserveCur += Fill (Clamp)
 * - SSOT: PlayerState->CombatComponent
 * - BP에는 로직 없음(메시/델타값만 세팅)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupAmmo : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupAmmo();

public:
	bool ServerTryPickup(AMosesPlayerState* PickerPS, FText& OutAnnounceText);

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

public:
	// -----------------------
	// Data (Editor/BP set)
	// -----------------------
	UPROPERTY(EditDefaultsOnly, Category="Moses|Ammo")
	EMosesAmmoType AmmoType = EMosesAmmoType::Rifle;

	UPROPERTY(EditDefaultsOnly, Category="Moses|Ammo", meta=(ClampMin="0"))
	int32 ReserveMaxDelta = 30;

	UPROPERTY(EditDefaultsOnly, Category="Moses|Ammo", meta=(ClampMin="0"))
	int32 ReserveFillDelta = 30;

	UPROPERTY(EditDefaultsOnly, Category="Moses|UI")
	FText PickupName = FText::FromString(TEXT("탄약"));
};
