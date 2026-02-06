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
};
