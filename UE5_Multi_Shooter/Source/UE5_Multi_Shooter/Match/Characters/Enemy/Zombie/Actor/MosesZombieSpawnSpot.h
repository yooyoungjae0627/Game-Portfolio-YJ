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
	void SpawnZombies_Server();
	void CleanupSpawnedZombies_Server();

public:
	UFUNCTION(BlueprintCallable, Category = "Zombie|Spawn")
	void ServerRespawnSpotZombies();

public:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root = nullptr;

	/** 레벨 인스턴스에서 배치한 SpawnPoint 컴포넌트들 */
	UPROPERTY(EditInstanceOnly, Category = "Zombie|Spawn")
	TArray<TObjectPtr<USceneComponent>> SpawnPoints;

	/** 4종 좀비 BP 클래스 배열 */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie|Spawn")
	TArray<TSubclassOf<AMosesZombieCharacter>> ZombieClasses;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMosesZombieCharacter>> SpawnedZombies;
};
