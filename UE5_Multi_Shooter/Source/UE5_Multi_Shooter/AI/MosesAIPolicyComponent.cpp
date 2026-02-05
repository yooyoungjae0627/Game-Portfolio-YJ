#include "MosesAIPolicyComponent.h" 

#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesAIPolicyComponent::UMosesAIPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesAIPolicyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GuardServerAuthority(TEXT("BeginPlay")))
	{
		return;
	}

	ApplyTier(EMosesAITickTier::C, TEXT("Init"));

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_PolicyTick,
		this,
		&UMosesAIPolicyComponent::ServerTickPolicy,
		PolicyTickIntervalSec,
		true);

	UE_LOG(LogMosesPhase, Warning, TEXT("[AI][POLICY][SV] BeginPlay PolicyTick=%.2fs Owner=%s"),
		PolicyTickIntervalSec, *GetOwner()->GetName());
}

void UMosesAIPolicyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_PolicyTick);
	}
	Super::EndPlay(EndPlayReason);
}

bool UMosesAIPolicyComponent::GuardServerAuthority(const TCHAR* Caller) const
{
	if (!GetWorld())
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[AI][POLICY][GUARD] %s denied: World=null"), Caller);
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[AI][POLICY][GUARD] %s denied: NotAuthority"), Caller);
		return false;
	}

	// Dedicated Server ONLY가 원칙이지만, 여기서는 Authority 기준으로 제한.
	return true;
}

AAIController* UMosesAIPolicyComponent::GetAIController() const
{
	if (!GetOwner())
	{
		return nullptr;
	}

	// Owner가 Character면 Controller 사용
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<AAIController>(Pawn->GetController());
	}
	return nullptr;
}

UBrainComponent* UMosesAIPolicyComponent::GetBrainComponent() const
{
	if (AAIController* AIC = GetAIController())
	{
		return AIC->BrainComponent;
	}
	return nullptr;
}

void UMosesAIPolicyComponent::SetForceAggro(bool bInForceAggro)
{
	if (!GuardServerAuthority(TEXT("SetForceAggro")))
	{
		return;
	}

	bForceAggro = bInForceAggro;
	UE_LOG(LogMosesPhase, Warning, TEXT("[AI][POLICY][SV] ForceAggro=%d Owner=%s"),
		bForceAggro, *GetOwner()->GetName());
}

bool UMosesAIPolicyComponent::CanRequestRepath() const
{
	if (!GetWorld())
	{
		return false;
	}
	return GetWorld()->GetTimeSeconds() >= NextAllowedRepathServerTime;
}

void UMosesAIPolicyComponent::MarkRepathRequested()
{
	if (!GuardServerAuthority(TEXT("MarkRepathRequested")))
	{
		return;
	}

	const FMosesAITickTierParams& P = GetTierParams(CurrentTier);
	NextAllowedRepathServerTime = GetWorld()->GetTimeSeconds() + P.NavRepathCooldownSec;
}

const FMosesAITickTierParams& UMosesAIPolicyComponent::GetTierParams(EMosesAITickTier Tier) const
{
	switch (Tier)
	{
	case EMosesAITickTier::S: return PolicyConfig.TierS;
	case EMosesAITickTier::A: return PolicyConfig.TierA;
	case EMosesAITickTier::B: return PolicyConfig.TierB;
	case EMosesAITickTier::C:
	default: return PolicyConfig.TierC;
	}
}

AActor* UMosesAIPolicyComponent::GetNearestPlayerActor(float& OutDistSq) const
{
	OutDistSq = TNumericLimits<float>::Max();

	if (!GetWorld() || !GetOwner())
	{
		return nullptr;
	}

	const FVector MyLoc = GetOwner()->GetActorLocation();

	AActor* Best = nullptr;

	// Dedicated Server 기준: PlayerController가 없을 수 있으니 PlayerState/PlayerPawn를 직접 찾는다.
	TArray<AActor*> Pawns;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), Pawns);

	for (AActor* Actor : Pawns)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			continue;
		}

		// 플레이어만(컨트롤러가 PlayerController인 Pawn)
		if (!Pawn->IsPlayerControlled())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLoc, Pawn->GetActorLocation());
		if (DistSq < OutDistSq)
		{
			OutDistSq = DistSq;
			Best = Pawn;
		}
	}

	return Best;
}

float UMosesAIPolicyComponent::GetNearestPlayerDistanceSq() const
{
	float DistSq = TNumericLimits<float>::Max();
	GetNearestPlayerActor(DistSq);
	return DistSq;
}

EMosesAITickTier UMosesAIPolicyComponent::EvaluateTierFromDistance(float DistToNearestPlayer) const
{
	// ForceAggro가 켜지면 반응성을 최우선으로 올린다 (S/A까지만)
	if (bForceAggro)
	{
		if (DistToNearestPlayer <= PolicyConfig.DistS)
		{
			return EMosesAITickTier::S;
		}
		return EMosesAITickTier::A;
	}

	if (DistToNearestPlayer <= PolicyConfig.DistS)
	{
		return EMosesAITickTier::S;
	}
	if (DistToNearestPlayer <= PolicyConfig.DistA)
	{
		return EMosesAITickTier::A;
	}
	if (DistToNearestPlayer <= PolicyConfig.DistB)
	{
		return EMosesAITickTier::B;
	}
	return EMosesAITickTier::C;
}

void UMosesAIPolicyComponent::ServerTickPolicy()
{
	if (!GuardServerAuthority(TEXT("ServerTickPolicy")))
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	const float NearestDistSq = GetNearestPlayerDistanceSq();
	const float NearestDist = FMath::Sqrt(NearestDistSq);

	EMosesAITickTier NewTier = EvaluateTierFromDistance(NearestDist);

	// ---- 히스테리시스 + 최소 유지 시간 ----
	const float HoldElapsed = Now - LastTierChangeServerTime;
	if (HoldElapsed < PolicyConfig.MinTierHoldSec)
	{
		// 최소 유지 시간 동안 Tier 변경 금지
		NewTier = CurrentTier;
	}
	else
	{
		// 경계에서 왔다갔다 방지: Margin 적용
		// 단순 구현: 현재 Tier 기준 경계에 Margin을 더해 “내려갈 때”만 여유를 준다.
		// (필요하면 상승/하강 각각 Margin을 다르게 확장 가능)
		if (CurrentTier == EMosesAITickTier::S && NearestDist <= PolicyConfig.DistS + PolicyConfig.HysteresisMargin)
		{
			NewTier = EMosesAITickTier::S;
		}
		if (CurrentTier == EMosesAITickTier::A && NearestDist <= PolicyConfig.DistA + PolicyConfig.HysteresisMargin)
		{
			// A 유지 범위 확장
			NewTier = (NewTier == EMosesAITickTier::B || NewTier == EMosesAITickTier::C) ? EMosesAITickTier::A : NewTier;
		}
		if (CurrentTier == EMosesAITickTier::B && NearestDist <= PolicyConfig.DistB + PolicyConfig.HysteresisMargin)
		{
			NewTier = (NewTier == EMosesAITickTier::C) ? EMosesAITickTier::B : NewTier;
		}
	}

	if (NewTier != CurrentTier)
	{
		ApplyTier(NewTier, TEXT("DistanceTierChanged"));
		UE_LOG(LogMosesPhase, Warning, TEXT("[AI][POLICY][SV] TierChanged Owner=%s Dist=%.0f Tier=%d"),
			*GetOwner()->GetName(), NearestDist, (int32)CurrentTier);
	}

	// ---- Perception "수동 폴링" (안정/예측 가능) ----
	const FMosesAITickTierParams& P = GetTierParams(CurrentTier);
	if (Now >= NextPerceptionPollServerTime)
	{
		NextPerceptionPollServerTime = Now + P.PerceptionPollIntervalSec;

		// 여기서 실제로는: 가장 가까운 플레이어가 특정 거리 이내면 “타겟 갱신/추적 상태 갱신”
		// 네 기존 좀비 AI(Blackboard/State)가 있다면 여기에 훅을 꽂아라.
		UE_LOG(LogMosesPhase, VeryVerbose, TEXT("[AI][POLICY][SV] PerceptionPoll Owner=%s Tier=%d"),
			*GetOwner()->GetName(), (int32)CurrentTier);
	}
}

void UMosesAIPolicyComponent::ApplyTier(EMosesAITickTier NewTier, const TCHAR* Reason)
{
	if (!GuardServerAuthority(TEXT("ApplyTier")))
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	CurrentTier = NewTier;
	LastTierChangeServerTime = Now;

	const FMosesAITickTierParams& P = GetTierParams(CurrentTier);

	// 1) AIController TickInterval
	if (AAIController* AIC = GetAIController())
	{
		AIC->SetActorTickInterval(P.DecisionIntervalSec);
	}

	// 2) BrainComponent(BehaviorTree) TickInterval
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->SetComponentTickInterval(P.DecisionIntervalSec);
	}

	// 3) Nav Repath 쿨다운은 BTTask가 사용
	NextAllowedRepathServerTime = Now + P.NavRepathCooldownSec;

	UE_LOG(LogMosesPhase, Warning,
		TEXT("[AI][POLICY][SV] ApplyTier=%d Reason=%s Decision=%.2f Perception=%.2f RepathCD=%.2f Owner=%s"),
		(int32)CurrentTier, Reason, P.DecisionIntervalSec, P.PerceptionPollIntervalSec, P.NavRepathCooldownSec, *GetOwner()->GetName());
}
