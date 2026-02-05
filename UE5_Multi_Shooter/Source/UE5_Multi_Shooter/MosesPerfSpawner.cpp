// MosesPerfSpawner.cpp
#include "MosesPerfSpawner.h"

#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "AI/MosesAIPolicyComponent.h"

AMosesPerfSpawner::AMosesPerfSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = false; // PerfTest 전용 도구. 상태를 복제할 이유 없음.
	SetActorEnableCollision(false);
}

void AMosesPerfSpawner::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWNER] BeginPlay World=%s NetMode=%d Name=%s"),
		*GetWorld()->GetName(), (int32)GetWorld()->GetNetMode(), *GetName());
}

bool AMosesPerfSpawner::GuardServerAuthority(const TCHAR* Caller) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][GUARD] %s denied: World=null"), Caller);
		return false;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][GUARD] %s denied: NotAuthority"), Caller);
		return false;
	}

	// Dedicated Server ONLY 원칙: (Listen/Standalone에서도 실행은 되지만, 증거 로그로 구분)
	return true;
}

FMosesPerfScenarioSpec AMosesPerfSpawner::GetSpecByScenarioId(EMosesPerfScenarioId ScenarioId) const
{
	FMosesPerfScenarioSpec Spec;

	switch (ScenarioId)
	{
	case EMosesPerfScenarioId::A1:
		Spec.ZombieCount = 50;
		break;
	case EMosesPerfScenarioId::A2:
		Spec.ZombieCount = 50;
		Spec.PickupCount = 50;
		break;
	case EMosesPerfScenarioId::A3:
		Spec.ZombieCount = 100;
		break;
	case EMosesPerfScenarioId::A4:
		Spec.bVFXSpam = true;
		break;
	case EMosesPerfScenarioId::A5:
		Spec.bShellSpam = true;
		break;
	default:
		break;
	}

	return Spec;
}

FVector AMosesPerfSpawner::GetSpawnOrigin() const
{
	return GetActorLocation();
}

FVector AMosesPerfSpawner::GetRandomSpawnLocation(float Radius) const
{
	const FVector Origin = GetSpawnOrigin();
	const FVector Rand2D = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(200.0f, Radius);
	return Origin + FVector(Rand2D.X, Rand2D.Y, 0.0f);
}

void AMosesPerfSpawner::DestroySpawnedActors(TArray<TWeakObjectPtr<AActor>>& Pool, const TCHAR* PoolName)
{
	int32 Destroyed = 0;

	for (TWeakObjectPtr<AActor>& Weak : Pool)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actor->Destroy();
			Destroyed++;
		}
	}

	Pool.Reset();

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][RESET] Pool=%s Destroyed=%d"), PoolName, Destroyed);
}

void AMosesPerfSpawner::ApplyScenario_Server(EMosesPerfScenarioId ScenarioId)
{
	if (!GuardServerAuthority(TEXT("ApplyScenario_Server")))
	{
		return;
	}

	const FMosesPerfScenarioSpec Spec = GetSpecByScenarioId(ScenarioId);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SCENARIO] Apply Scenario=%d Zombie=%d Pickup=%d ForceAggro=%d VFX=%d Audio=%d Shell=%d"),
		(int32)ScenarioId, Spec.ZombieCount, Spec.PickupCount, Spec.bForceAggro, Spec.bVFXSpam, Spec.bAudioSpam, Spec.bShellSpam);

	// “A를 만든다”는 목적이므로, 먼저 초기화 후 스펙 적용
	PerfReset_Server();

	if (Spec.ZombieCount > 0)
	{
		SpawnZombies_Server(Spec.ZombieCount);
	}
	if (Spec.PickupCount > 0)
	{
		SpawnPickups_Server(Spec.PickupCount);
	}

	SetForceAggro_Server(Spec.bForceAggro);
	SetVFXSpam_Server(Spec.bVFXSpam);
	SetAudioSpam_Server(Spec.bAudioSpam);
	SetShellSpam_Server(Spec.bShellSpam);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SCENARIO] Apply Done Scenario=%d"), (int32)ScenarioId);
}

void AMosesPerfSpawner::PerfReset_Server()
{
	if (!GuardServerAuthority(TEXT("PerfReset_Server")))
	{
		return;
	}

	DestroySpawnedActors(SpawnedZombies, TEXT("Zombies"));
	DestroySpawnedActors(SpawnedPickups, TEXT("Pickups"));

	bForceAggro = false;
	bVFXSpam = false;
	bAudioSpam = false;
	bShellSpam = false;

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][RESET] Done"));
}

void AMosesPerfSpawner::SpawnZombies_Server(int32 Count)
{
	if (!GuardServerAuthority(TEXT("SpawnZombies_Server")))
	{
		return;
	}

	if (!ZombieClass)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN] ZombieClass is null (set in BP_PerfSpawner)"));
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	int32 Spawned = 0;

	for (int32 i = 0; i < Count; i++)
	{
		const FVector Loc = GetRandomSpawnLocation(SpawnRadius);
		const FRotator Rot = FRotator(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* NewActor = World->SpawnActor<AActor>(ZombieClass, Loc, Rot, Params);
		if (NewActor)
		{
			SpawnedZombies.Add(NewActor);
			Spawned++;
		}
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN] Zombies Spawned=%d Requested=%d Total=%d"),
		Spawned, Count, SpawnedZombies.Num());
}

void AMosesPerfSpawner::SpawnPickups_Server(int32 Count)
{
	if (!GuardServerAuthority(TEXT("SpawnPickups_Server")))
	{
		return;
	}

	if (!PickupClass)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN] PickupClass is null (set in BP_PerfSpawner)"));
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	int32 Spawned = 0;

	for (int32 i = 0; i < Count; i++)
	{
		const FVector Loc = GetRandomSpawnLocation(SpawnRadius);
		const FRotator Rot = FRotator::ZeroRotator;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* NewActor = World->SpawnActor<AActor>(PickupClass, Loc, Rot, Params);
		if (NewActor)
		{
			SpawnedPickups.Add(NewActor);
			Spawned++;
		}
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN] Pickups Spawned=%d Requested=%d Total=%d"),
		Spawned, Count, SpawnedPickups.Num());
}

void AMosesPerfSpawner::SetForceAggro_Server(bool bOn)
{
	if (!GuardServerAuthority(TEXT("SetForceAggro_Server")))
	{
		return;
	}

	bForceAggro = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM][SV] ForceAggro=%d"), bForceAggro);

	// [MOD] 스폰된 좀비들에게 정책 컴포넌트가 있으면 강제 Aggro 플래그 전달
	int32 Applied = 0;
	for (const TWeakObjectPtr<AActor>& Weak : SpawnedZombies)
	{
		AActor* Zombie = Weak.Get();
		if (!Zombie)
		{
			continue;
		}

		if (UMosesAIPolicyComponent* Policy = Zombie->FindComponentByClass<UMosesAIPolicyComponent>())
		{
			Policy->SetForceAggro(bForceAggro);
			Applied++;
		}
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM][SV] ForceAggro Applied=%d/%d"),
		Applied, SpawnedZombies.Num());
}

void AMosesPerfSpawner::SetVFXSpam_Server(bool bOn)
{
	if (!GuardServerAuthority(TEXT("SetVFXSpam_Server")))
	{
		return;
	}

	bVFXSpam = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM] VFXSpam=%d"), bVFXSpam);

	// NOTE:
	// - Niagara 스팸은 “스팸용 Niagara Actor”를 따로 만들어 Timer로 Spawn/Activate하면 된다.
	// - DAY4~DAY6에서 정책 적용 전/후를 비교할 때 연결.
}

void AMosesPerfSpawner::SetAudioSpam_Server(bool bOn)
{
	if (!GuardServerAuthority(TEXT("SetAudioSpam_Server")))
	{
		return;
	}

	bAudioSpam = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM] AudioSpam=%d"), bAudioSpam);

	// NOTE:
	// - Audio 스팸은 서버/클라 구분해서 “클라에서만” 재생하는게 일반적.
	// - 이 플래그는 “스팸을 켰다”는 시나리오 고정에만 사용하고,
	//   실제 재생은 PerfPanel(클라)에서 수행하는 구조가 깔끔하다.
}

void AMosesPerfSpawner::SetShellSpam_Server(bool bOn)
{
	if (!GuardServerAuthority(TEXT("SetShellSpam_Server")))
	{
		return;
	}

	bShellSpam = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM] ShellSpam=%d"), bShellSpam);

	// NOTE:
	// - Shell(탄피) 스팸은 “탄피 Actor”를 물리 시뮬레이션 ON으로 대량 Spawn하면 된다.
	// - 여기서는 인프라만 제공.
}
