#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MosesZombieAnimInstance.generated.h"

/**
 * UMosesZombieAnimInstance
 *
 * - Zombie AnimBP가 "읽기만" 할 상태 제공
 * - bIsDead는 서버가 죽음 확정 시 NetMulticast로 1회 세팅
 * - AnimGraph에서 bIsDead로 Locomotion -> Dying(또는 DeathMontage)로 전환
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesZombieAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMosesZombieAnimInstance();

public:
	// ---------------------------------------------------------------------
	// [ADD] Event-driven Death flag setter
	// - Server: 죽음 확정 시 Multicast_SetDeadState(true)에서 호출
	// - Spawn/Respawn: BeginPlay에서 false로 리셋
	// ---------------------------------------------------------------------
	void SetIsDead(const bool bInDead); // [ADD]

protected:
	virtual void NativeInitializeAnimation() override;

private:
	void CacheOwner();

private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> OwnerPawn = nullptr;

private:
	// ---------------------------------------------------------------------
	// [ADD] Death (AnimBP reads)
	// ---------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Moses|Zombie|Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false; // [ADD]
};
