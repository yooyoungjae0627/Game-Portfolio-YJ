#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MosesZombieTryAttack.generated.h"

/**
 * BTTask - TryAttack (Server)
 * - TargetActor와 거리 체크 후 ZombieCharacter->ServerTryStartAttack_FromAI() 호출
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UBTTask_MosesZombieTryAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MosesZombieTryAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UPROPERTY(EditAnywhere, Category="Moses|AI")
	FName BBKey_TargetActor = TEXT("TargetActor");
};
