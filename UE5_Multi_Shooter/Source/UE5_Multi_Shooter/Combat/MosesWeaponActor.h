#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesWeaponActor.generated.h"

class USkeletalMeshComponent;

UCLASS(Blueprintable)
class UE5_MULTI_SHOOTER_API AMosesWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AMosesWeaponActor();

	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
};
