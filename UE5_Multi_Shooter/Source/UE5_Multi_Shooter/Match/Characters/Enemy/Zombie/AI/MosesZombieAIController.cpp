#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AI/MosesZombieAIController.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AIPerceptionTypes.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

AMosesZombieAIController::AMosesZombieAIController()
{
	bReplicates = false;

	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(*PerceptionComp);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
}

bool AMosesZombieAIController::IsServerAI() const
{
	return GetWorld() && GetWorld()->GetAuthGameMode() != nullptr;
}

void AMosesZombieAIController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsServerAI())
	{
		return;
	}

	SetupPerception();
}

void AMosesZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsServerAI())
	{
		return;
	}

	SetupBlackboardAndRunBT();
}

void AMosesZombieAIController::SetupPerception()
{
	if (!PerceptionComp || !SightConfig)
	{
		return;
	}

	// 기본값 (DA가 있으면 OnPossess 이후 갱신되게 아래에서 다시 Set 가능)
	SightConfig->SightRadius = 1500.f;
	SightConfig->LoseSightRadius = 1800.f;
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	SightConfig->SetMaxAge(2.0f);

	// 적/중립/아군 설정: 플레이어를 감지하는 용도라 일단 모두 감지 허용
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMosesZombieAIController::HandlePerceptionUpdated);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] Perception Ready Controller=%s"), *GetName());
}

void AMosesZombieAIController::SetupBlackboardAndRunBT()
{
	if (!BehaviorTreeAsset)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] Missing BehaviorTreeAsset Controller=%s"), *GetName());
		return;
	}

	AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(GetPawn());
	if (Zombie)
	{
		// DA 기반으로 Sight 파라미터 갱신
		if (UMosesZombieTypeData* DA = Zombie->GetZombieTypeData())
		{
			if (SightConfig && PerceptionComp)
			{
				SightConfig->SightRadius = DA->SightRadius;
				SightConfig->LoseSightRadius = DA->LoseSightRadius;
				SightConfig->PeripheralVisionAngleDegrees = DA->PeripheralVisionAngleDeg;
				PerceptionComp->RequestStimuliListenerUpdate();

				UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] ApplySight DA Sight=%.0f Lose=%.0f FOV=%.0f Zombie=%s"),
					DA->SightRadius, DA->LoseSightRadius, DA->PeripheralVisionAngleDeg, *GetNameSafe(Zombie));
			}
		}
	}

	RunBehaviorTree(BehaviorTreeAsset);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] RunBT OK BT=%s Pawn=%s"),
		*GetNameSafe(BehaviorTreeAsset), *GetNameSafe(GetPawn()));
}

void AMosesZombieAIController::ClearTargetActor()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	BB->ClearValue(BBKey_TargetActor);
}

void AMosesZombieAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!IsServerAI())
	{
		return;
	}

	if (!Actor)
	{
		return;
	}

	APawn* SensedPawn = Cast<APawn>(Actor);
	if (!SensedPawn)
	{
		return;
	}

	// PlayerState가 있는 Pawn만 플레이어로 취급
	if (!SensedPawn->GetPlayerState())
	{
		return;
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		BB->SetValueAsObject(BBKey_TargetActor, Actor);
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] SenseTarget OK Target=%s Controller=%s"),
			*GetNameSafe(Actor), *GetName());
	}
	else
	{
		// 타겟이 이 Actor였던 경우만 해제
		UObject* Cur = BB->GetValueAsObject(BBKey_TargetActor);
		if (Cur == Actor)
		{
			ClearTargetActor();
			UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] SenseLost Target=%s Controller=%s"),
				*GetNameSafe(Actor), *GetName());
		}
	}
}
