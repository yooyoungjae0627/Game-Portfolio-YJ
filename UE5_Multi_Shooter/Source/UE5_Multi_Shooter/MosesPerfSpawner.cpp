// ============================================================================
// UE5_Multi_Shooter/Perf/MosesPerfSpawner.cpp  (FULL - UPDATED)
// ============================================================================

#include "MosesPerfSpawner.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "AI/MosesAIPolicyComponent.h"

AMosesPerfSpawner::AMosesPerfSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = false; // PerfTest 전용 도구
	SetActorEnableCollision(false);
}

void AMosesPerfSpawner::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	UE_LOG(
		LogMosesPhase,
		Warning,
		TEXT("[PERF][SPAWNER] BeginPlay World=%s NetMode=%d Name=%s"),
		World ? *World->GetName() : TEXT("null"),
		World ? (int32)World->GetNetMode() : -1,
		*GetName()
	);
}

bool AMosesPerfSpawner::GuardServerAuthority_DSOnly(const TCHAR* Caller) const
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

	// [MOD] Dedicated Server ONLY 원칙을 “로그로 증거화”
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		UE_LOG(
			LogMosesPhase,
			Warning,
			TEXT("[PERF][GUARD] %s running on NetMode=%d (Expected DS). Evidence may be invalid."),
			Caller,
			(int32)World->GetNetMode()
		);
	}

	return true;
}

FMosesPerfScenarioSpec AMosesPerfSpawner::GetSpecByScenarioId(EMosesPerfScenarioId ScenarioId) const
{
	FMosesPerfScenarioSpec Spec;

	switch (ScenarioId)
	{
	case EMosesPerfScenarioId::A1:
		Spec.ZombieCount = 50;
		Spec.bForceAggro = true;
		break;

	case EMosesPerfScenarioId::A2:
		Spec.ZombieCount = 50;
		Spec.PickupCount = 50;
		Spec.bForceAggro = true;
		break;

	case EMosesPerfScenarioId::A3:
		Spec.ZombieCount = 100;
		Spec.bForceAggro = true;
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

FVector AMosesPerfSpawner::GetSpawnLocation_ByRadiusPolicy(float Radius) const
{
	const FVector Origin = GetSpawnOrigin();

	// [MOD] Radius=0 -> 완전 동일 위치 밀집(worst-case)
	if (Radius <= KINDA_SMALL_NUMBER)
	{
		return Origin;
	}

	// [MOD] 원형 균등 분포: angle + sqrt(r)로 면적 균등
	const float Angle = FMath::FRandRange(-PI, PI);
	const float R = Radius * FMath::Sqrt(FMath::FRand()); // 균등 분포
	const float X = FMath::Cos(Angle) * R;
	const float Y = FMath::Sin(Angle) * R;

	return Origin + FVector(X, Y, 0.0f);
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
	if (!GuardServerAuthority_DSOnly(TEXT("ApplyScenario_Server")))
	{
		return;
	}

	const FMosesPerfScenarioSpec Spec = GetSpecByScenarioId(ScenarioId);

	UE_LOG(
		LogMosesPhase,
		Warning,
		TEXT("[PERF][SCENARIO] Apply Scenario=%d Zombie=%d Pickup=%d ForceAggro=%d VFX=%d Audio=%d Shell=%d"),
		(int32)ScenarioId,
		Spec.ZombieCount,
		Spec.PickupCount,
		Spec.bForceAggro,
		Spec.bVFXSpam,
		Spec.bAudioSpam,
		Spec.bShellSpam
	);

	PerfReset_Server();

	// [MOD] Scenario 스폰은 “즉시 for-loop” 대신 “Timer 기반”으로 재현성 유지
	if (Spec.ZombieCount > 0)
	{
		SpawnCount = Spec.ZombieCount;
		bForceAggroPreset = Spec.bForceAggro;
		StartSpawnZombiesPreset_Server();
	}

	if (Spec.PickupCount > 0)
	{
		SpawnPickups_Server(Spec.PickupCount);
	}

	SetVFXSpam_Server(Spec.bVFXSpam);
	SetAudioSpam_Server(Spec.bAudioSpam);
	SetShellSpam_Server(Spec.bShellSpam);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SCENARIO] Apply Requested Scenario=%d"), (int32)ScenarioId);
}

void AMosesPerfSpawner::PerfReset_Server()
{
	if (!GuardServerAuthority_DSOnly(TEXT("PerfReset_Server")))
	{
		return;
	}

	// [MOD] 진행 중 스폰 타이머 중지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	TargetSpawnTotal = 0;
	SpawnedSoFar = 0;
	CurrentSpawnInterval = 0.0f;

	DestroySpawnedActors(SpawnedZombies, TEXT("Zombies"));
	DestroySpawnedActors(SpawnedPickups, TEXT("Pickups"));

	bForceAggro = false;
	bVFXSpam = false;
	bAudioSpam = false;
	bShellSpam = false;

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][RESET] Done"));
}

void AMosesPerfSpawner::StartSpawnZombiesPreset_Server()
{
	if (!GuardServerAuthority_DSOnly(TEXT("StartSpawnZombiesPreset_Server")))
	{
		return;
	}

	// [MOD] 프리셋 -> Runtime 상태로 복사
	const int32 Count = FMath::Max(0, SpawnCount);
	const float Interval = FMath::Max(0.0f, SpawnInterval);
	bForceAggro = bForceAggroPreset;

	SpawnZombies_Internal_Begin(Count, Interval);
}

void AMosesPerfSpawner::SpawnZombies_Internal_Begin(int32 Count, float IntervalSeconds)
{
	if (!ZombieClass)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN][SV] ZombieClass is null (set in BP_PerfSpawner)"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TargetSpawnTotal = Count;
	SpawnedSoFar = 0;
	CurrentSpawnInterval = IntervalSeconds;

	UE_LOG(
		LogMosesPhase,
		Warning,
		TEXT("[PERF][SPAWN][SV] Start Count=%d Interval=%.3f Radius=%.1f Aggro=%d"),
		TargetSpawnTotal,
		CurrentSpawnInterval,
		SpawnRadius,
		bForceAggro ? 1 : 0
	);

	// Count=0이면 바로 Done 로그
	if (TargetSpawnTotal <= 0)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN][SV] Done Total=0"));
		return;
	}

	// Interval=0이면 한 틱에 다 스폰 (그래도 Start/Progress/Done 로그는 남김)
	if (CurrentSpawnInterval <= KINDA_SMALL_NUMBER)
	{
		while (SpawnedSoFar < TargetSpawnTotal)
		{
			SpawnZombies_Internal_TickOne();
		}

		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN][SV] Done Total=%d"), SpawnedZombies.Num());

		if (bForceAggro)
		{
			ApplyForceAggroToSpawnedZombies();
		}
		return;
	}

	// [MOD] Timer 기반 스폰 (Tick 금지)
	World->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AMosesPerfSpawner::SpawnZombies_Internal_TickOne,
		CurrentSpawnInterval,
		true
	);
}

void AMosesPerfSpawner::SpawnZombies_Internal_TickOne()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (SpawnedSoFar >= TargetSpawnTotal)
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);

		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN][SV] Done Total=%d"), SpawnedZombies.Num());

		if (bForceAggro)
		{
			ApplyForceAggroToSpawnedZombies();
		}
		return;
	}

	const FVector Loc = GetSpawnLocation_ByRadiusPolicy(SpawnRadius);
	const FRotator Rot(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ZombieClass, Loc, Rot, Params);
	if (NewActor)
	{
		SpawnedZombies.Add(NewActor);
	}

	SpawnedSoFar++;

	// [MOD] Progress 로그 (50/100 같은 “눈에 보이는” 중간 체크포인트)
	const int32 Half = FMath::Max(1, TargetSpawnTotal / 2);
	if ((SpawnedSoFar % Half) == 0 || SpawnedSoFar == TargetSpawnTotal)
	{
		UE_LOG(
			LogMosesPhase,
			Warning,
			TEXT("[PERF][SPAWN][SV] Spawned %d/%d"),
			SpawnedSoFar,
			TargetSpawnTotal
		);
	}
}

void AMosesPerfSpawner::SpawnPickups_Server(int32 Count)
{
	if (!GuardServerAuthority_DSOnly(TEXT("SpawnPickups_Server")))
	{
		return;
	}

	if (!PickupClass)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAWN] PickupClass is null (set in BP_PerfSpawner)"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Spawned = 0;

	for (int32 i = 0; i < Count; i++)
	{
		const FVector Loc = GetSpawnLocation_ByRadiusPolicy(FMath::Max(SpawnRadius, 300.0f));
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
	if (!GuardServerAuthority_DSOnly(TEXT("SetForceAggro_Server")))
	{
		return;
	}

	bForceAggro = bOn;

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM][SV] ForceAggro=%d"), bForceAggro ? 1 : 0);

	if (bForceAggro)
	{
		ApplyForceAggroToSpawnedZombies();
	}
}

AActor* AMosesPerfSpawner::FindAnyPlayerPawn_Server() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return nullptr;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		APawn* Pawn = PS->GetPawn();
		if (Pawn)
		{
			return Pawn;
		}
	}

	return nullptr;
}

void AMosesPerfSpawner::ApplyForceAggroToSpawnedZombies()
{
	AActor* TargetPawn = FindAnyPlayerPawn_Server();

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
			// [MOD] “스폰 즉시 플레이어 타겟 고정” 재현성 목적
			Policy->SetForceAggro(true);

			if (TargetPawn)
			{
				Policy->ForceSetTargetActor_Server(TargetPawn); // <- 아래 주석 참고
			}

			Applied++;
		}
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM][SV] ForceAggro Applied=%d/%d Target=%s"),
		Applied,
		SpawnedZombies.Num(),
		TargetPawn ? *TargetPawn->GetName() : TEXT("null"));
}

void AMosesPerfSpawner::SetVFXSpam_Server(bool bOn)
{
	if (!GuardServerAuthority_DSOnly(TEXT("SetVFXSpam_Server")))
	{
		return;
	}

	bVFXSpam = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM] VFXSpam=%d"), bVFXSpam ? 1 : 0);
}

void AMosesPerfSpawner::SetAudioSpam_Server(bool bOn)
{
	if (!GuardServerAuthority_DSOnly(TEXT("SetAudioSpam_Server")))
	{
		return;
	}

	bAudioSpam = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM] AudioSpam=%d"), bAudioSpam ? 1 : 0);
}

void AMosesPerfSpawner::SetShellSpam_Server(bool bOn)
{
	if (!GuardServerAuthority_DSOnly(TEXT("SetShellSpam_Server")))
	{
		return;
	}

	bShellSpam = bOn;
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SPAM] ShellSpam=%d"), bShellSpam ? 1 : 0);
}
