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
	if (!AIC) return;

	AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(AIC->GetPawn());
	if (!Zombie || !Zombie->HasAuthority()) return;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return;

	AActor* CurTarget = Cast<AActor>(BB->GetValueAsObject(BBKey_TargetActor));

	// 1) 현재 타겟이 유효하면 거리 갱신 + 너무 멀면 해제
	auto IsValidPlayerPawn = [](AActor* Actor)->APawn*
		{
			APawn* P = Cast<APawn>(Actor);
			return (P && P->GetPlayerState()) ? P : nullptr;
		};

	APawn* CurPawn = IsValidPlayerPawn(CurTarget);

	// 2) 현재 타겟이 없거나 무효면 -> 반경에서 새 타겟 찾기
	if (!CurPawn)
	{
		APawn* BestPawn = nullptr;
		float BestDist = TNumericLimits<float>::Max();

		UWorld* World = Zombie->GetWorld();
		if (World)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (!PC) continue;

				APawn* P = PC->GetPawn();
				if (!P || !P->GetPlayerState()) continue;

				const float Dist = FVector::Dist(Zombie->GetActorLocation(), P->GetActorLocation());
				if (Dist < BestDist && Dist <= AcquireRadius)
				{
					BestDist = Dist;
					BestPawn = P;
				}
			}
		}

		if (BestPawn)
		{
			BB->SetValueAsObject(BBKey_TargetActor, BestPawn);
			BB->SetValueAsFloat(BBKey_DistanceToTarget, BestDist);

			UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] AcquireTarget ByRadius Dist=%.0f R=%.0f Target=%s Zombie=%s"),
				BestDist, AcquireRadius, *GetNameSafe(BestPawn), *GetNameSafe(Zombie));
		}
		else
		{
			BB->ClearValue(BBKey_TargetActor);
			BB->SetValueAsFloat(BBKey_DistanceToTarget, 999999.f);
		}

		return;
	}

	// 3) 현재 타겟 유지 로직 (거리 갱신)
	const float Dist = FVector::Dist(Zombie->GetActorLocation(), CurPawn->GetActorLocation());
	BB->SetValueAsFloat(BBKey_DistanceToTarget, Dist);

	// 4) 추적 반경 벗어나면 해제
	if (Dist > AcquireRadius)
	{
		BB->ClearValue(BBKey_TargetActor);
		BB->SetValueAsFloat(BBKey_DistanceToTarget, 999999.f);

		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] TargetCleared TooFar Dist=%.0f R=%.0f Zombie=%s"),
			Dist, AcquireRadius, *GetNameSafe(Zombie));
	}
}
