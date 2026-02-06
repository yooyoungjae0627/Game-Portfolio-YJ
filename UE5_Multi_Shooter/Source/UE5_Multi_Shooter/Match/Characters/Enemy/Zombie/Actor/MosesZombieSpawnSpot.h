#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesZombieSpawnSpot.generated.h"

class USceneComponent;
class AMosesZombieCharacter;

/**
 * Zombie Spawn Spot
 * - 구역(스팟) 단위로 좀비 스폰 포인트 배열을 가진다.
 * - 서버만 스폰/리스폰 권한을 가진다.
 *
 * [MOD] SpawnPoints는 에디터에서 직접 배열에 넣지 않고,
 *       Root의 자식 SceneComponent 중 이름이 "SP_"로 시작하는 것들을
 *       서버 BeginPlay에서 자동 수집한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesZombieSpawnSpot : public AActor
{
	GENERATED_BODY()

public:
	AMosesZombieSpawnSpot();

protected:
	virtual void BeginPlay() override;

private:
	void CollectSpawnPointsFromChildren_Server(); // [NEW]
	void SpawnZombies_Server();
	void CleanupSpawnedZombies_Server();

public:
	UFUNCTION(BlueprintCallable, Category = "Zombie|Spawn")
	void ServerRespawnSpotZombies();

public:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root = nullptr;

	/** [MOD] 자동 수집된 SpawnPoint 컴포넌트들 */
	UPROPERTY(VisibleInstanceOnly, Category = "Zombie|Spawn") // [MOD] 인스턴스에서 보이기만(수정 불가)
		TArray<TObjectPtr<USceneComponent>> SpawnPoints;

	/** 4종 좀비 BP 클래스 배열 (BP Defaults에서 세팅) */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Spawn")
	TArray<TSubclassOf<AMosesZombieCharacter>> ZombieClasses;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMosesZombieCharacter>> SpawnedZombies;

	/** [NEW] 자식 컴포넌트 이름 접두사 */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Spawn")
	FName SpawnPointPrefix = TEXT("SP_");
};
