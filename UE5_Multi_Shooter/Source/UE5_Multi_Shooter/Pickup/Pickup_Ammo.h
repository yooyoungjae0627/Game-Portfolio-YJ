#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponTypes.h"
#include "UE5_Multi_Shooter/Pickup/PickupBase.h"
#include "Pickup_Ammo.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API APickup_Ammo : public APickupBase
{
	GENERATED_BODY()

public:
	APickup_Ammo();

protected:
	virtual bool Server_ApplyPickup(APawn* InstigatorPawn) override;

private:
	UPROPERTY(EditDefaultsOnly, Category="Pickup|Ammo")
	EMosesWeaponType WeaponType = EMosesWeaponType::Rifle;

	UPROPERTY(EditDefaultsOnly, Category="Pickup|Ammo")
	int32 AddReserveAmmo = 30;
};
