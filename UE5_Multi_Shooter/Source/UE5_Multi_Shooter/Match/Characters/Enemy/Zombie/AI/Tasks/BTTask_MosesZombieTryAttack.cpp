#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AI/Tasks/BTTask_MosesZombieTryAttack.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Actor.h"

UBTTask_MosesZombieTryAttack::UBTTask_MosesZombieTryAttack()
{
	NodeName = TEXT("Moses Try Attack");
	bNotifyTick = false;
}

EBTNodeResult::Type UBTTask_MosesZombieTryAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC)
	{
		return EBTNodeResult::Failed;
	}

	AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(AIC->GetPawn());
	if (!Zombie || !Zombie->HasAuthority())
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(BBKey_TargetActor));
	if (!IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	const float Dist = FVector::Dist(Zombie->GetActorLocation(), Target->GetActorLocation());
	const float AttackRange = Zombie->GetAttackRange_Server();

	if (Dist > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	const bool bStarted = Zombie->ServerTryStartAttack_FromAI(Target); // [MOD]
	return bStarted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
