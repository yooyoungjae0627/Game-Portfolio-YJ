#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesSpotRespawnManager.generated.h"

class AMosesZombieSpawnSpot;

/**
 * Spot Respawn Manager (Server Authority)
 * - 캡처 성공 이벤트 수신:
 *   10초 카운트다운 방송(10→0) -> 0초에 해당 구역 좀비 리스폰(+Flag 리스폰 훅)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesSpotRespawnManager : public AActor
{
	GENERATED_BODY()

public:
	AMosesSpotRespawnManager();

protected:
	virtual void BeginPlay() override;

protected:
	void TickCountdown_Server();
	void BroadcastCountdown_Server(int32 Seconds);
	void ExecuteRespawn_Server();

public:
	UFUNCTION(BlueprintCallable, Category="Respawn")
	void ServerOnSpotCaptured(AMosesZombieSpawnSpot* CapturedSpot);

public:
	UPROPERTY(EditDefaultsOnly, Category="Respawn")
	int32 RespawnCountdownSeconds;

	UPROPERTY(EditInstanceOnly, Category="Respawn")
	TArray<TObjectPtr<AMosesZombieSpawnSpot>> ManagedSpots;

private:
	UPROPERTY()
	TObjectPtr<AMosesZombieSpawnSpot> PendingRespawnSpot;

	int32 CurrentCountdown;
	FTimerHandle CountdownTimerHandle;
};
