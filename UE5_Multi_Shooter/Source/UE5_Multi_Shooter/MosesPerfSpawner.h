// MosesPerfSpawner.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPerfTypes.h"
#include "MosesPerfSpawner.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfSpawner();

	// ---------- Server-only API ----------
	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void ApplyScenario_Server(EMosesPerfScenarioId ScenarioId);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void PerfReset_Server();

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void SpawnZombies_Server(int32 Count);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void SpawnPickups_Server(int32 Count);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void SetForceAggro_Server(bool bOn);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void SetVFXSpam_Server(bool bOn);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void SetAudioSpam_Server(bool bOn);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf|Spawner")
	void SetShellSpam_Server(bool bOn);

protected:
	virtual void BeginPlay() override;

private:
	bool GuardServerAuthority(const TCHAR* Caller) const;

	FMosesPerfScenarioSpec GetSpecByScenarioId(EMosesPerfScenarioId ScenarioId) const;

	// ---- Spawn helpers ----
	FVector GetSpawnOrigin() const;
	FVector GetRandomSpawnLocation(float Radius) const;

	void DestroySpawnedActors(TArray<TWeakObjectPtr<AActor>>& Pool, const TCHAR* PoolName);

private:
	// ---------- Config (BP에서 지정) ----------
	UPROPERTY(EditAnywhere, Category="Moses|Perf|Config")
	TSubclassOf<AActor> ZombieClass;

	UPROPERTY(EditAnywhere, Category="Moses|Perf|Config")
	TSubclassOf<AActor> PickupClass;

	UPROPERTY(EditAnywhere, Category="Moses|Perf|Config")
	float SpawnRadius = 1200.0f;

	// ---------- Runtime Pools ----------
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedZombies;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedPickups;

	// ---------- Spam States ----------
	bool bForceAggro = false;
	bool bVFXSpam = false;
	bool bAudioSpam = false;
	bool bShellSpam = false;
};
