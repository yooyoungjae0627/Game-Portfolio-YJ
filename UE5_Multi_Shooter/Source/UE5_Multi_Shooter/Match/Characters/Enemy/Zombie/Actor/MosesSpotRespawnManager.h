#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesSpotRespawnManager.generated.h"

class AMosesZombieSpawnSpot;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesSpotRespawnManager : public AActor
{
	GENERATED_BODY()

public:
	AMosesSpotRespawnManager();

	/**
	 * 서버: FlagSpot 캡처 성공 시 호출
	 *
	 * 중요:
	 * - 이 함수에 전달되는 Spot은 "이번 캡처로 인해 Flag가 떠난 이전 Zone의 SpawnSpot"이어야 한다.
	 *   (새로 활성화된 Zone의 SpawnSpot을 넘기면 규칙이 틀어진다)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Respawn") // [MOD]
		void ServerOnSpotCaptured(AMosesZombieSpawnSpot* OldZoneSpotToRespawn);     // [MOD]

protected:
	virtual void BeginPlay() override;

private:
	// ---------------------------------------------------------------------
	// Countdown (Server)
	// ---------------------------------------------------------------------
	void StartCountdown_Server(AMosesZombieSpawnSpot* TargetSpot);
	void StopCountdown_Server();

	UFUNCTION()
	void ServerTickRespawnCountdown();

	void ExecuteCountdown_Server();

	void BroadcastCountdown_Server(int32 RemainingSec);

private:
	// ---------------------------------------------------------------------
	// Config
	// ---------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Respawn", meta = (AllowPrivateAccess = "true"))
	int32 RespawnCountdownSeconds = 5;

	/**
	 * (옵션) 관리 스팟 목록.
	 * 비워두면 BeginPlay에서 월드의 SpawnSpot들을 스캔하여 디버그용으로 채움.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Respawn", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AMosesZombieSpawnSpot>> ManagedSpots;

private:
	// ---------------------------------------------------------------------
	// Runtime (Server)
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	TObjectPtr<AMosesZombieSpawnSpot> PendingRespawnSpot = nullptr;

	UPROPERTY(Transient)
	float RespawnEndServerTime = 0.f;

	UPROPERTY()
	int32 LastBroadcastRemainingSec = -1;

	FTimerHandle CountdownTimerHandle;
};
