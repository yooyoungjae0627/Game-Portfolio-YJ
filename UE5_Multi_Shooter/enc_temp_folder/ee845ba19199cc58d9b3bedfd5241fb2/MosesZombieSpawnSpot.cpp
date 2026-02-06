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
		CollectSpawnPointsFromChildren_Server(); // [NEW]
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

	CollectSpawnPointsFromChildren_Server(); // [NEW] 리스폰 때도 재수집
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

void AMosesZombieSpawnSpot::CollectSpawnPointsFromChildren_Server()
{
	if (!HasAuthority() || !Root)
	{
		return;
	}

	SpawnPoints.Reset();

	// [FIX] 'Children' 이름 충돌 방지
	TArray<USceneComponent*> ChildSceneComps;
	Root->GetChildrenComponents(/*bIncludeAllDescendants=*/true, ChildSceneComps);

	for (USceneComponent* C : ChildSceneComps)
	{
		if (!C)
		{
			continue;
		}

		// 이름이 "SP_" 로 시작하는 것만 SpawnPoint로 인정
		const FString Name = C->GetName();
		if (SpawnPointPrefix != NAME_None)
		{
			const FString PrefixStr = SpawnPointPrefix.ToString();
			if (!Name.StartsWith(PrefixStr))
			{
				continue;
			}
		}

		SpawnPoints.Add(C); // 포인터로 Add -> OK
	}

	// [FIX] TObjectPtr Sort 경고 회피: Raw로 정렬 후 다시 담기
	TArray<USceneComponent*> SortedRaw;
	SortedRaw.Reserve(SpawnPoints.Num());
	for (const TObjectPtr<USceneComponent>& Ptr : SpawnPoints)
	{
		SortedRaw.Add(Ptr.Get());
	}

	SortedRaw.Sort([](const USceneComponent& A, const USceneComponent& B)
		{
			return A.GetName() < B.GetName();
		});

	SpawnPoints.Reset();
	SpawnPoints.Reserve(SortedRaw.Num());
	for (USceneComponent* P : SortedRaw)
	{
		if (P)
		{
			SpawnPoints.Add(P);
		}
	}

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] CollectSpawnPoints Spot=%s Count=%d"),
		*GetName(), SpawnPoints.Num());
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
		const TSubclassOf<AMosesZombieCharacter> SpawnClass = ZombieClasses[ClassIdx];
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

		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawn Spot=%s Zombie=%s Index=%d Point=%s"),
			*GetName(), *GetNameSafe(Spawned), i, *GetNameSafe(P));
	}
}
