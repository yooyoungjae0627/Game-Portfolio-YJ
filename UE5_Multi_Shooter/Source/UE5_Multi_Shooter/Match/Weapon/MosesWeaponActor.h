#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesWeaponActor.generated.h"

class USkeletalMeshComponent;

/**
 * AMosesWeaponActor
 *
 * [역할]
 * - 무기 표현 전용 코스메틱 Actor.
 * - 판정/탄약/리로드 로직은 절대 포함하지 않는다.
 *
 * [규칙]
 * - Dedicated Server에서는 무기 액터를 직접 Replicate하지 않는다.
 * - SSOT(CombatComponent) 변경을 OnRep로 받아
 *   각 클라에서 로컬로 스폰/Attach해서 재현한다.
 */
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
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
};
