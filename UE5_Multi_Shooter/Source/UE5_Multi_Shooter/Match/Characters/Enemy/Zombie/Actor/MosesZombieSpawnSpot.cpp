#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Actor/MosesZombieSpawnSpot.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"


AMosesZombieSpawnSpot::AMosesZombieSpawnSpot()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AMosesZombieSpawnSpot::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SpawnZombies_Server();
	}
}

void AMosesZombieSpawnSpot::ServerRespawnSpotZombies()
{
	if (!HasAuthority())
	{
		return;
	}

	CleanupSpawnedZombies_Server();
	SpawnZombies_Server();
}

void AMosesZombieSpawnSpot::CleanupSpawnedZombies_Server()
{
	for (AMosesZombieCharacter* Z : SpawnedZombies)
	{
		if (IsValid(Z))
		{
			Z->Destroy();
		}
	}
	SpawnedZombies.Reset();
}

void AMosesZombieSpawnSpot::SpawnZombies_Server()
{
	if (!HasAuthority())
	{
		return;
	}

	if (SpawnPoints.Num() <= 0 || ZombieClasses.Num() <= 0)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] SpawnSpot Missing Setup Spot=%s Points=%d Classes=%d"),
			*GetName(), SpawnPoints.Num(), ZombieClasses.Num());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = 0; i < SpawnPoints.Num(); ++i)
	{
		USceneComponent* P = SpawnPoints[i];
		if (!P)
		{
			continue;
		}

		const int32 ClassIdx = i % ZombieClasses.Num();
		TSubclassOf<AMosesZombieCharacter> SpawnClass = ZombieClasses[ClassIdx];
		if (!SpawnClass)
		{
			continue;
		}

		const FTransform TM = P->GetComponentTransform();

		AMosesZombieCharacter* Spawned = World->SpawnActorDeferred<AMosesZombieCharacter>(
			SpawnClass, TM, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

		if (!Spawned)
		{
			continue;
		}

		Spawned->FinishSpawning(TM);
		SpawnedZombies.Add(Spawned);

		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawn Spot=%s Zombie=%s Index=%d"),
			*GetName(), *GetNameSafe(Spawned), i);
	}
}
