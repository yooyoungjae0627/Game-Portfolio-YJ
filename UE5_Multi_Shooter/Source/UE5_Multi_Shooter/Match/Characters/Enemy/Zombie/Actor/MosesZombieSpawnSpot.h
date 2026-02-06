// ============================================================================
// MosesZombieSpawnSpot.h (FULL)
// ----------------------------------------------------------------------------
// - Zone 단위 Zombie Spawn Spot
// - Server Authority only
// - SpawnPoints: Root의 자식 SceneComponent 중 이름이 "SP_" 접두사인 것들을
//   서버 BeginPlay에서 자동 수집 (에디터 배열 수동 입력 금지)
// - Respawn: 기존 스폰 좀비 제거 후 다시 스폰
// - Evidence logs:
//   - [ZOMBIE][SV] CollectSpawnPoints ...
//   - [ZOMBIE][SV] Spawn Spot=...
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesZombieSpawnSpot.generated.h"

class USceneComponent;
class AMosesZombieCharacter;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieSpawnSpot : public AActor
{
	GENERATED_BODY()

public:
	AMosesZombieSpawnSpot();

	/** 서버: 스팟 좀비 리스폰(기존 제거 -> 스폰) */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Spawn")
	void ServerRespawnSpotZombies();

protected:
	virtual void BeginPlay() override;

private:
	// ---------------------------------------------------------------------
	// Server internals
	// ---------------------------------------------------------------------
	void CollectSpawnPointsFromChildren_Server();
	void SpawnZombies_Server();
	void CleanupSpawnedZombies_Server();

private:
	// ---------------------------------------------------------------------
	// Components
	// ---------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root = nullptr;

private:
	// ---------------------------------------------------------------------
	// Config (Defaults)
	// ---------------------------------------------------------------------
	/** 자식 컴포넌트 이름 접두사. 예: "SP_" */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Spawn", meta = (AllowPrivateAccess = "true"))
	FName SpawnPointPrefix = TEXT("SP_");

	/** 좀비 BP 클래스 배열 (BP Defaults에서 세팅) */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Spawn", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<AMosesZombieCharacter>> ZombieClasses;

private:
	// ---------------------------------------------------------------------
	// Runtime (Server)
	// ---------------------------------------------------------------------
	/** [AUTO] Root 자식에서 자동 수집된 스폰 포인트들(읽기 전용) */
	UPROPERTY(VisibleInstanceOnly, Category = "Zombie|Spawn", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<USceneComponent>> SpawnPoints;

	/** 서버가 스폰한 좀비들(리스폰 시 Destroy) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AMosesZombieCharacter>> SpawnedZombies;
};
