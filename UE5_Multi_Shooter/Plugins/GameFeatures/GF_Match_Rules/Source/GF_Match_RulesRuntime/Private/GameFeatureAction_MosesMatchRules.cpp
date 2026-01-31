// ============================================================================
// GameFeatureAction_MosesMatchRules.cpp (FULL)
// ============================================================================

#include "GameFeatureAction_MosesMatchRules.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"

void UGameFeatureAction_MosesMatchRules::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	UE_LOG(LogTemp, Log, TEXT("[GF][SV] GF_Match_Rules Activating Enable=%d"), bEnableOnActivate ? 1 : 0);

	ApplyToAllRelevantWorlds(bEnableOnActivate);
}

void UGameFeatureAction_MosesMatchRules::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	UE_LOG(LogTemp, Log, TEXT("[GF][SV] GF_Match_Rules Deactivating -> Disable"));

	ApplyToAllRelevantWorlds(false);
}

bool UGameFeatureAction_MosesMatchRules::ShouldApplyToWorld(const UWorld* World) const
{
	if (!World)
	{
		return false;
	}

	if (!bOnlyApplyInGameWorld)
	{
		return true;
	}

	return (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UGameFeatureAction_MosesMatchRules::ApplyToAllRelevantWorlds(bool bEnable) const
{
	if (!GEngine)
	{
		return;
	}

	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UWorld* World = WC.World();
		if (!World || !ShouldApplyToWorld(World))
		{
			continue;
		}

		// 서버에서만
		if (!World->GetAuthGameMode())
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("[GF][SV] Apply World=%s NetMode=%d Enable=%d"),
			*World->GetName(),
			(int32)World->GetNetMode(),
			bEnable ? 1 : 0);

		ApplyFlagEnableToWorld(World, bEnable);
	}
}

void UGameFeatureAction_MosesMatchRules::ApplyFlagEnableToWorld(UWorld* World, bool bEnable) const
{
	check(World);

	int32 CandidateCount = 0;
	int32 AppliedCount = 0;

	// 월드 전체 액터를 순회하면서 "SetFlagSystemEnabled(bool)" 함수를 가진 액터를 대상으로 처리.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (!IsFlagSpotCandidate(Actor))
		{
			continue;
		}

		++CandidateCount;
		InvokeSetFlagSystemEnabled(Actor, bEnable);
		++AppliedCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[GF][SV] FlagEnable Applied Enable=%d Candidates=%d Applied=%d World=%s"),
		bEnable ? 1 : 0,
		CandidateCount,
		AppliedCount,
		*World->GetName());
}

bool UGameFeatureAction_MosesMatchRules::IsFlagSpotCandidate(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// BlueprintImplementable/Native 함수든 상관 없이, UFunction이 존재하면 후보로 간주한다.
	const UFunction* Func = Actor->FindFunction(FlagEnableFunctionName);
	return (Func != nullptr);
}

void UGameFeatureAction_MosesMatchRules::InvokeSetFlagSystemEnabled(AActor* Actor, bool bEnable) const
{
	check(Actor);

	UFunction* Func = Actor->FindFunction(FlagEnableFunctionName);
	if (!Func)
	{
		return;
	}

	// 함수 시그니처: SetFlagSystemEnabled(bool bEnable)
	struct FParams
	{
		bool bEnabled;
	};

	FParams Params;
	Params.bEnabled = bEnable;

	Actor->ProcessEvent(Func, &Params);
}
