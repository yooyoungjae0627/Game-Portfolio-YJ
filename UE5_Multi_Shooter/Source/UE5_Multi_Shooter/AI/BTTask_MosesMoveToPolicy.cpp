#include "BTTask_MosesMoveToPolicy.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

// ✅ [FIX] FPathFollowingRequestResult / EPathFollowingRequestResult 정의 포함
#include "Navigation/PathFollowingComponent.h"

#include "MosesAIPolicyComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

UBTTask_MosesMoveToPolicy::UBTTask_MosesMoveToPolicy()
{
	NodeName = TEXT("Moses MoveTo (Policy)");
	bNotifyTick = false;
}

UMosesAIPolicyComponent* UBTTask_MosesMoveToPolicy::FindPolicyComponent(APawn* ControlledPawn) const
{
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UMosesAIPolicyComponent>() : nullptr;
}

EBTNodeResult::Type UBTTask_MosesMoveToPolicy::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIC->GetPawn();
	if (!Pawn || !Pawn->HasAuthority())
	{
		// 서버 권위 100%
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	UMosesAIPolicyComponent* Policy = FindPolicyComponent(Pawn);
	if (Policy && !Policy->CanRequestRepath())
	{
		UE_LOG(LogMosesPhase, VeryVerbose, TEXT("[AI][NAV][SV] Repath SKIP (Cooldown) Pawn=%s"), *Pawn->GetName());
		return EBTNodeResult::Succeeded;
	}

	FAIMoveRequest Req;
	Req.SetGoalActor(TargetActor);
	Req.SetAcceptanceRadius(AcceptableRadius);
	Req.SetUsePathfinding(bUsePathfinding);
	Req.SetAllowPartialPath(bAllowPartialPath);

	FNavPathSharedPtr OutPath;
	const FPathFollowingRequestResult Res = AIC->MoveTo(Req, &OutPath);

	if (Policy)
	{
		Policy->MarkRepathRequested();
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[AI][NAV][SV] MoveTo Requested Code=%d Pawn=%s Target=%s"),
		(int32)Res.Code, *Pawn->GetName(), *TargetActor->GetName());

	// ✅ [FIX] namespace enum Type 비교
	return (Res.Code == EPathFollowingRequestResult::Failed) ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
}
