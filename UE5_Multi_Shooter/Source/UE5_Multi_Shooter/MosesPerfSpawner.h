// ============================================================================
// UE5_Multi_Shooter/Perf/MosesPerfSpawner.h  (FULL - UPDATED)
// ============================================================================
// [DAY1-3 요구 충족]
// - EditInstanceOnly 노출: SpawnCount / SpawnInterval / bForceAggro / SpawnRadius
// - 서버에서만 스폰 (Dedicated Server 원칙)
// - SpawnRadius=0 -> 완전 동일 위치 밀집 worst-case
// - >0 -> 원형 랜덤 균등 분포
// - 증거 로그: Start / Progress / Done
// - Tick 금지: Timer 기반
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPerfTypes.h"
#include "MosesPerfSpawner.generated.h"

class UMosesAIPolicyComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfSpawner();

	// ---------- Server-only API ----------
	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void ApplyScenario_Server(EMosesPerfScenarioId ScenarioId);

	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void PerfReset_Server();

	// “프리셋 인스턴스 값(SpawnCount/Interval/Radius/Aggro)”대로 스폰 시작
	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void StartSpawnZombiesPreset_Server();

	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void SpawnPickups_Server(int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void SetForceAggro_Server(bool bOn);

	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void SetVFXSpam_Server(bool bOn);

	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void SetAudioSpam_Server(bool bOn);

	UFUNCTION(BlueprintCallable, Category = "Moses|Perf|Spawner")
	void SetShellSpam_Server(bool bOn);

protected:
	virtual void BeginPlay() override;

private:
	bool GuardServerAuthority_DSOnly(const TCHAR* Caller) const;

	FMosesPerfScenarioSpec GetSpecByScenarioId(EMosesPerfScenarioId ScenarioId) const;

	// ---- Spawn helpers ----
	FVector GetSpawnOrigin() const;
	FVector GetSpawnLocation_ByRadiusPolicy(float Radius) const;

	void DestroySpawnedActors(TArray<TWeakObjectPtr<AActor>>& Pool, const TCHAR* PoolName);

	void SpawnZombies_Internal_Begin(int32 Count, float IntervalSeconds);
	void SpawnZombies_Internal_TickOne();

	void ApplyForceAggroToSpawnedZombies();

	AActor* FindAnyPlayerPawn_Server() const;

private:
	// ---------------------------------------------------------------------
	// [MOD] Instance Preset Options (EditInstanceOnly 권장)
	// ---------------------------------------------------------------------
	UPROPERTY(EditInstanceOnly, Category = "Moses|Perf|Preset", meta = (ClampMin = "0"))
	int32 SpawnCount = 100;

	UPROPERTY(EditInstanceOnly, Category = "Moses|Perf|Preset", meta = (ClampMin = "0.0"))
	float SpawnInterval = 0.05f;

	UPROPERTY(EditInstanceOnly, Category = "Moses|Perf|Preset")
	bool bForceAggroPreset = true;

	// SpawnRadius = 0 -> 동일 위치 밀집 (worst-case)
	// >0 -> 원형 랜덤
	UPROPERTY(EditInstanceOnly, Category = "Moses|Perf|Preset", meta = (ClampMin = "0.0"))
	float SpawnRadius = 0.0f;

	// ---------------------------------------------------------------------
	// Config (BP에서 지정)
	// ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Moses|Perf|Config")
	TSubclassOf<AActor> ZombieClass;

	UPROPERTY(EditAnywhere, Category = "Moses|Perf|Config")
	TSubclassOf<AActor> PickupClass;

	// ---------------------------------------------------------------------
	// Runtime Pools
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedZombies;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedPickups;

	// ---------------------------------------------------------------------
	// Spawn Runtime State (Timer)
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	int32 TargetSpawnTotal = 0;

	UPROPERTY(Transient)
	int32 SpawnedSoFar = 0;

	UPROPERTY(Transient)
	float CurrentSpawnInterval = 0.0f;

	FTimerHandle SpawnTimerHandle;

	// ---------------------------------------------------------------------
	// Spam States
	// ---------------------------------------------------------------------
	bool bForceAggro = false;
	bool bVFXSpam = false;
	bool bAudioSpam = false;
	bool bShellSpam = false;
};
