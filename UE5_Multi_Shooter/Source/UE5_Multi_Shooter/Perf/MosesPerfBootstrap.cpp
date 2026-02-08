// MosesPerfBootstrap.cpp
#include "UE5_Multi_Shooter/Perf/MosesPerfBootstrap.h"

#include "UE5_Multi_Shooter/Perf/MosesPerfTestSubsystem.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfSpawner.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfMarker.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMosesPerfBootstrap::AMosesPerfBootstrap()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
}

void AMosesPerfBootstrap::BeginPlay()
{
	Super::BeginPlay();

	BindToSubsystem();

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][BIND] Bootstrap BeginPlay Actor=%s World=%s NetMode=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetWorld()),
		GetWorld() ? (int32)GetWorld()->GetNetMode() : -1);
}

void AMosesPerfBootstrap::BindToSubsystem()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	Subsys->SetPerfSpawner(PerfSpawner);

	if (Marker01) { Subsys->RegisterMarker(Marker01->GetMarkerId(), Marker01); }
	if (Marker02) { Subsys->RegisterMarker(Marker02->GetMarkerId(), Marker02); }
	if (Marker03) { Subsys->RegisterMarker(Marker03->GetMarkerId(), Marker03); }

	Subsys->DumpPerfBindingState(this);
}

FName AMosesPerfBootstrap::GetMarkerIdByIndex(int32 MarkerIndex) const
{
	switch (MarkerIndex)
	{
	case 1: return Marker01 ? Marker01->GetMarkerId() : NAME_None;
	case 2: return Marker02 ? Marker02->GetMarkerId() : NAME_None;
	case 3: return Marker03 ? Marker03->GetMarkerId() : NAME_None;
	default: break;
	}
	return NAME_None;
}

void AMosesPerfBootstrap::perf_dump()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>())
		{
			Subsys->DumpPerfBindingState(this);
		}
	}
}

void AMosesPerfBootstrap::perf_marker(int32 MarkerIndex)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const FName MarkerId = GetMarkerIdByIndex(MarkerIndex);
	if (MarkerId.IsNone())
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] FAIL InvalidMarkerIndex=%d"), MarkerIndex);
		return;
	}

	Subsys->TryMoveLocalPlayerToMarker(PC, MarkerId);
}

void AMosesPerfBootstrap::perf_spawn(int32 Count)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	Subsys->TrySpawnZombies_ServerAuthority(Count);
}

void AMosesPerfBootstrap::perf_measure_begin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	Subsys->BeginMeasure(this, FName(*MeasureId), FName(*MarkerId), SpawnCount, TrialIndex, TrialTotal, DurationSec);
}

void AMosesPerfBootstrap::perf_measure_end()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	Subsys->EndMeasure(this);
}
