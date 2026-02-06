#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MosesZombieUpdateTarget.generated.h"

/**
 * BTService - UpdateTarget (Server)
 * - TargetActor 유효성 체크
 * - 너무 멀면 타겟 해제 (MaxChaseDistance)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UBTService_MosesZombieUpdateTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_MosesZombieUpdateTarget();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	UPROPERTY(EditAnywhere, Category="Moses|AI")
	FName BBKey_TargetActor = TEXT("TargetActor");

	// [NEW] 거리 갱신용 (이미 만들었다면 그대로)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName BBKey_DistanceToTarget = TEXT("DistanceToTarget");

	// [NEW] 반경 기반 추적 거리 (요구: 뒤에 있어도 반경이면 쫓음)
	UPROPERTY(EditAnywhere, Category = "Chase")
	float AcquireRadius = 1500.f;   // 추적 시작/유지 반경(원하는 값으로)

	UPROPERTY(EditAnywhere, Category = "Attack")
	float AttackRadius = 150.f;     // 공격 반경(요구: 150)
};
