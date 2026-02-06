#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AI/Services/BTService_MosesZombieUpdateTarget.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UBTService_MosesZombieUpdateTarget::UBTService_MosesZombieUpdateTarget()
{
	NodeName = TEXT("Moses Update Target");
	Interval = 0.3f;
	RandomDeviation = 0.05f;
	bNotifyBecomeRelevant = false;
	bNotifyCeaseRelevant = false;
}

void UBTService_MosesZombieUpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC)
	{
		return;
	}

	AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(AIC->GetPawn());
	if (!Zombie || !Zombie->HasAuthority())
	{
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(BBKey_TargetActor));
	if (!IsValid(Target))
	{
		return;
	}

	APawn* TargetPawn = Cast<APawn>(Target);
	if (!TargetPawn || !TargetPawn->GetPlayerState())
	{
		BB->ClearValue(BBKey_TargetActor);
		return;
	}

	const float MaxChase = Zombie->GetMaxChaseDistance_Server();
	const float Dist = FVector::Dist(Zombie->GetActorLocation(), Target->GetActorLocation());
	if (Dist > MaxChase)
	{
		BB->ClearValue(BBKey_TargetActor);
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] TargetCleared TooFar Dist=%.0f Max=%.0f Zombie=%s"),
			Dist, MaxChase, *GetNameSafe(Zombie));
	}
}
