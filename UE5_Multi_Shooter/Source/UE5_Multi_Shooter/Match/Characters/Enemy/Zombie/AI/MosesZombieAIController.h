#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MosesZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UBlackboardComponent;

/**
 * AMosesZombieAIController (Server Authority)
 * - AIPerception(Sight)로 플레이어 감지 -> Blackboard TargetActor 갱신
 * - BehaviorTree 실행 (서버만)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMosesZombieAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	bool IsServerAI() const;

private:
	void SetupPerception();
	void SetupBlackboardAndRunBT();
	void ClearTargetActor();

private:
	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	UPROPERTY(VisibleAnywhere, Category="Moses|AI")
	TObjectPtr<UAIPerceptionComponent> PerceptionComp = nullptr;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig = nullptr;

	/** 좀비 BT (에디터에서 세팅) */
	UPROPERTY(EditDefaultsOnly, Category="Moses|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset = nullptr;

	/** BB 키 이름 고정 */
	UPROPERTY(EditDefaultsOnly, Category="Moses|AI")
	FName BBKey_TargetActor = TEXT("TargetActor");
};
