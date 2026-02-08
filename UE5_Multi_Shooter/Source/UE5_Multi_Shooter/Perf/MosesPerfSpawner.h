// MosesPerfSpawner.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPerfSpawner.generated.h"

class ACharacter;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfSpawner();

	// Server authority spawn entry (Subsystem will call this)
	bool Server_SpawnZombies(int32 Count);

protected:
	virtual void BeginPlay() override;

private:
	bool HasServerAuthority() const;
	bool TryFindGroundLocation(const FVector& InLocation, FVector& OutGroundLocation) const;

private:
	UPROPERTY(EditInstanceOnly, Category = "Perf|Spawner")
	TSubclassOf<ACharacter> ZombieClass;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Spawner", meta=(ClampMin="0.0"))
	float SpawnRadius = 1800.0f;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Spawner", meta=(ClampMin="0.0"))
	float SpawnHeight = 500.0f;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Spawner")
	bool bForceAggro = false; // optional flag (you can hook later)

	UPROPERTY(VisibleInstanceOnly, Category = "Perf|Spawner")
	int32 LastSpawnedCount = 0;
};
