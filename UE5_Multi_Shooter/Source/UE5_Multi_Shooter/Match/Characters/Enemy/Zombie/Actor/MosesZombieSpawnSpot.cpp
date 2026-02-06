// ============================================================================
// MosesZombieSpawnSpot.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Actor/MosesZombieSpawnSpot.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

AMosesZombieSpawnSpot::AMosesZombieSpawnSpot()
{
	bReplicates = true;
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][CTOR] Spot=%s Rep=%d"), *GetNameSafe(this), bReplicates ? 1 : 0);
}

void AMosesZombieSpawnSpot::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	CollectSpawnPointsFromChildren_Server();
	SpawnZombies_Server();
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
	check(HasAuthority());

	int32 Removed = 0;

	for (TObjectPtr<AMosesZombieCharacter>& Z : SpawnedZombies)
	{
		if (IsValid(Z))
		{
			Z->Destroy();
			Removed++;
		}
	}

	SpawnedZombies.Reset();

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Cleanup Spot=%s Removed=%d"), *GetNameSafe(this), Removed);
}

void AMosesZombieSpawnSpot::CollectSpawnPointsFromChildren_Server()
{
	check(HasAuthority());

	if (!Root)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] CollectSpawnPoints FAIL Root=NULL Spot=%s"), *GetNameSafe(this));
		return;
	}

	SpawnPoints.Reset();

	TArray<USceneComponent*> ChildSceneComps;
	Root->GetChildrenComponents(/*bIncludeAllDescendants=*/true, ChildSceneComps);

	const FString PrefixStr = SpawnPointPrefix.ToString();

	for (USceneComponent* C : ChildSceneComps)
	{
		if (!C)
		{
			continue;
		}

		// "SP_"로 시작하는 것만 SpawnPoint로 인정
		if (!PrefixStr.IsEmpty())
		{
			const FString Name = C->GetName();
			if (!Name.StartsWith(PrefixStr))
			{
				continue;
			}
		}

		SpawnPoints.Add(C);
	}

	// 정렬(이름 기준): TObjectPtr Sort 경고 회피를 위해 Raw로 정렬 후 재등록
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

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] CollectSpawnPoints Spot=%s Count=%d Prefix=%s"),
		*GetNameSafe(this), SpawnPoints.Num(), *PrefixStr);
}

void AMosesZombieSpawnSpot::SpawnZombies_Server()
{
	check(HasAuthority());

	if (SpawnPoints.Num() <= 0 || ZombieClasses.Num() <= 0)
	{
		UE_LOG(LogMosesZombie, Warning,
			TEXT("[ZOMBIE][SV] SpawnSpot Missing Setup Spot=%s Points=%d Classes=%d"),
			*GetNameSafe(this), SpawnPoints.Num(), ZombieClasses.Num());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 SpawnedCount = 0;

	for (int32 i = 0; i < SpawnPoints.Num(); ++i)
	{
		USceneComponent* P = SpawnPoints[i].Get();
		if (!P)
		{
			continue;
		}

		const int32 ClassIdx = (i % ZombieClasses.Num());
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
		SpawnedCount++;

		// ✅ STEP3 DoD 로그 (2종 중 1개)
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawn Spot=%s Zombie=%s Index=%d Point=%s"),
			*GetNameSafe(this), *GetNameSafe(Spawned), i, *GetNameSafe(P));
	}

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawn Spot=%s Spawned=%d"), *GetNameSafe(this), SpawnedCount);
}
