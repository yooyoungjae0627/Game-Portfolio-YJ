// ============================================================================
// UE5_Multi_Shooter/Perf/MosesPerfBootstrap.cpp  (FULL - NEW)
// ============================================================================

#include "MosesPerfBootstrap.h"

#include "MosesPerfTestSubsystem.h"
#include "MosesPerfSpawner.h"
#include "MosesPerfMarker.h"

#include "Engine/World.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

AMosesPerfBootstrap::AMosesPerfBootstrap()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetActorEnableCollision(false);
	bReplicates = false; // 인프라 Actor (판정/상태 복제 목적 없음)
}

bool AMosesPerfBootstrap::GuardWorld(const TCHAR* Caller) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][BOOT][GUARD] %s denied: World=null"), Caller);
		return false;
	}
	return true;
}

void AMosesPerfBootstrap::BeginPlay()
{
	Super::BeginPlay();

	if (!GuardWorld(TEXT("BeginPlay")))
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][BOOT] GameInstance=null"));
		return;
	}

	UMosesPerfTestSubsystem* Subsystem = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][BOOT] PerfTestSubsystem=null"));
		return;
	}

	// ---------------------------------------------------------------------
	// 순서 고정 (Evidence-First)
	// ---------------------------------------------------------------------
	Subsystem->SetPerfSpawner(SpawnerRef);

	UE_LOG(
		LogMosesPhase,
		Warning,
		TEXT("[PERF][BOOT] SetSpawner=%s"),
		SpawnerRef ? *SpawnerRef->GetName() : TEXT("null")
	);

	// Marker 3개 등록 (없으면 Evidence invalid)
	for (AMosesPerfMarker* Marker : MarkerRefs)
	{
		if (!Marker)
		{
			UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][BOOT] MarkerRef=null (invalid)"));
			continue;
		}

		Subsystem->RegisterMarker(Marker);

		UE_LOG(
			LogMosesPhase,
			Warning,
			TEXT("[PERF][MARKER] Registered %s (Id=%d)"),
			*Marker->GetName(),
			(int32)Marker->MarkerId
		);
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][BOOT] Done"));
}
