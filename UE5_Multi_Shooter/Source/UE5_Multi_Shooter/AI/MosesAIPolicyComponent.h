// AI/MosesAIPolicyComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesAITickPolicyTypes.h"
#include "MosesAIPolicyComponent.generated.h"

class AAIController;
class UBrainComponent;

/**
 * DAY3: 서버 AI 업데이트 빈도 정책 컴포넌트
 * - Dedicated Server에서만 동작
 * - 거리/중요도 기반 Tier로 Decision/Perception/Nav 빈도 제어
 */
UCLASS(ClassGroup=(Moses), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesAIPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesAIPolicyComponent();

	// ---- Policy API ----
	UFUNCTION(BlueprintCallable, Category="Moses|AI|Policy")
	void SetForceAggro(bool bInForceAggro);

	UFUNCTION(BlueprintPure, Category="Moses|AI|Policy")
	EMosesAITickTier GetCurrentTier() const { return CurrentTier; }

	/** BTTask에서 호출: MoveTo 재요청 가능 여부(쿨다운) */
	UFUNCTION(BlueprintCallable, Category="Moses|AI|Policy")
	bool CanRequestRepath() const;

	/** BTTask에서 호출: MoveTo 요청 직후 쿨다운 갱신 */
	UFUNCTION(BlueprintCallable, Category="Moses|AI|Policy")
	void MarkRepathRequested();

	UFUNCTION(BlueprintCallable, Category = "Moses|AI|Policy")
	void ForceSetTargetActor_Server(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Moses|AI|Policy")
	AActor* GetForcedTargetActor() const { return ForcedTargetActor.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ---- Core Loop ----
	void ServerTickPolicy();
	void ApplyTier(EMosesAITickTier NewTier, const TCHAR* Reason);

	// ---- Tier Decision ----
	EMosesAITickTier EvaluateTierFromDistance(float DistToNearestPlayer) const;
	float GetNearestPlayerDistanceSq() const;
	AActor* GetNearestPlayerActor(float& OutDistSq) const;

	const FMosesAITickTierParams& GetTierParams(EMosesAITickTier Tier) const;

	// ---- Engine Hooks ----
	bool GuardServerAuthority(const TCHAR* Caller) const;
	AAIController* GetAIController() const;
	UBrainComponent* GetBrainComponent() const;

private:
	// ---------- Config ----------
	UPROPERTY(EditAnywhere, Category="Moses|AI|Policy")
	FMosesAITickPolicyConfig PolicyConfig;

	UPROPERTY(EditAnywhere, Category="Moses|AI|Policy")
	float PolicyTickIntervalSec = 0.5f; // Tier 평가 자체는 너무 자주 할 필요 없음

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ForcedTargetActor;

	// ---------- Runtime ----------
	EMosesAITickTier CurrentTier = EMosesAITickTier::C;

	float LastTierChangeServerTime = 0.0f;

	bool bForceAggro = false;

	// Perception 수동 폴링 타이머
	float NextPerceptionPollServerTime = 0.0f;

	// Nav Repath 쿨다운
	float NextAllowedRepathServerTime = 0.0f;

	FTimerHandle TimerHandle_PolicyTick;

};
