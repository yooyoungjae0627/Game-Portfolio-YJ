// AI/BTTask_MosesMoveToPolicy.h
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MosesMoveToPolicy.generated.h"

struct FBlackboardKeySelector;

/**
 * DAY3: MoveTo 호출 자체를 정책화해서 Nav Repath 폭주를 방지하는 BT Task
 * - 기존 BT의 Move To를 이걸로 교체한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UBTTask_MosesMoveToPolicy : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MosesMoveToPolicy();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	class UMosesAIPolicyComponent* FindPolicyComponent(APawn* ControlledPawn) const;

private:
	/** Target Actor (예: 플레이어) */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category="Move")
	float AcceptableRadius = 120.0f;

	UPROPERTY(EditAnywhere, Category="Move")
	bool bUsePathfinding = true;

	UPROPERTY(EditAnywhere, Category="Move")
	bool bAllowPartialPath = true;
};
