// MosesPerfSpawner.cpp
#include "UE5_Multi_Shooter/Perf/MosesPerfSpawner.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"

AMosesPerfSpawner::AMosesPerfSpawner()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AMosesPerfSpawner::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* World = GetWorld();
	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][SPAWN] BeginPlay Actor=%s World=%s NetMode=%d ZombieClass=%s Radius=%.0f"),
		*GetNameSafe(this),
		*GetNameSafe(World),
		World ? (int32)World->GetNetMode() : -1,
		*GetNameSafe(ZombieClass),
		SpawnRadius);
}

bool AMosesPerfSpawner::HasServerAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && HasAuthority();
}

bool AMosesPerfSpawner::TryFindGroundLocation(const FVector& InLocation, FVector& OutGroundLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start = InLocation + FVector(0, 0, SpawnHeight);
	const FVector End = InLocation - FVector(0, 0, SpawnHeight * 2.0f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PerfSpawnGroundTrace), false, this);

	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return false;
	}

	OutGroundLocation = Hit.ImpactPoint;
	return true;
}

static FVector2D MosesPerf_MakeRandomUnitVector2D()
{
	const float AngleRad = FMath::FRandRange(0.0f, 2.0f * PI);
	return FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
}

bool AMosesPerfSpawner::Server_SpawnZombies(int32 Count)
{
	if (!HasServerAuthority())
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][SPAWN][SV] FAIL NoAuthority Actor=%s"), *GetNameSafe(this));
		return false;
	}

	if (Count <= 0)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][SPAWN][SV] FAIL InvalidCount=%d"), Count);
		return false;
	}

	if (!ZombieClass)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][SPAWN][SV] FAIL ZombieClass null"));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	LastSpawnedCount = 0;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][SPAWN][SV] Request Count=%d Class=%s Radius=%.0f ForceAggro=%d"),
		Count,
		*GetNameSafe(ZombieClass),
		SpawnRadius,
		bForceAggro ? 1 : 0);

	const FVector Center = GetActorLocation();

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector2D Rand2D = MosesPerf_MakeRandomUnitVector2D() * FMath::FRandRange(0.0f, SpawnRadius);
		const FVector Candidate = Center + FVector(Rand2D.X, Rand2D.Y, 0.0f);

		FVector GroundLoc;
		if (!TryFindGroundLocation(Candidate, GroundLoc))
		{
			// If no ground, just skip this unit (evidence matters more than "force spawn")
			continue;
		}

		const FRotator SpawnRot(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ACharacter* Spawned = World->SpawnActor<ACharacter>(ZombieClass, GroundLoc, SpawnRot, Params);
		if (!Spawned)
		{
			continue;
		}

		++LastSpawnedCount;
	}

	UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][SPAWN][SV] Done Spawned=%d/%d"), LastSpawnedCount, Count);
	return LastSpawnedCount > 0;
}
