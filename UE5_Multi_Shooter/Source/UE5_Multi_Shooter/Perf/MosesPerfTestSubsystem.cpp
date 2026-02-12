#include "UE5_Multi_Shooter/Perf/MosesPerfTestSubsystem.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfSpawner.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfMarker.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

UMosesPerfTestSubsystem::UMosesPerfTestSubsystem()
{
}

void UMosesPerfTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMosesAuth, Log, TEXT("[PERF][BOOT] PerfTestSubsystem Initialize GI=%s"), *GetNameSafe(GetGameInstance()));
}

void UMosesPerfTestSubsystem::Deinitialize()
{
	UE_LOG(LogMosesAuth, Log, TEXT("[PERF][BOOT] PerfTestSubsystem Deinitialize"));

	PerfSpawner = nullptr;
	Markers.Empty();
	ActiveMeasure = FMosesPerfMeasureContext();
	ActiveAIPolicyMode = EMosesPerfAIPolicyMode::Off;

	Super::Deinitialize();
}

void UMosesPerfTestSubsystem::SetPerfSpawner(AMosesPerfSpawner* InSpawner)
{
	PerfSpawner = InSpawner;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][BIND] PerfSpawner=%s (World=%s)"),
		*GetNameSafe(PerfSpawner),
		*GetNameSafe(PerfSpawner ? PerfSpawner->GetWorld() : nullptr));
}

void UMosesPerfTestSubsystem::RegisterMarker(FName MarkerId, AMosesPerfMarker* InMarker)
{
	if (MarkerId.IsNone() || !InMarker)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MARKER] Register FAIL MarkerId=%s Marker=%s"),
			*MarkerId.ToString(), *GetNameSafe(InMarker));
		return;
	}

	Markers.FindOrAdd(MarkerId) = InMarker;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][MARKER] Registered %s -> %s Loc=%s"),
		*MarkerId.ToString(),
		*GetNameSafe(InMarker),
		*InMarker->GetActorLocation().ToString());
}

bool UMosesPerfTestSubsystem::IsReady() const
{
	return (PerfSpawner != nullptr) && (Markers.Num() > 0);
}

void UMosesPerfTestSubsystem::SetAIPolicyMode(EMosesPerfAIPolicyMode NewMode)
{
	ActiveAIPolicyMode = NewMode;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][POLICY] AI=%s"),
		(ActiveAIPolicyMode == EMosesPerfAIPolicyMode::On) ? TEXT("ON") : TEXT("OFF"));
}

AMosesPerfMarker* UMosesPerfTestSubsystem::FindMarker(FName MarkerId) const
{
	if (const TObjectPtr<AMosesPerfMarker>* Found = Markers.Find(MarkerId))
	{
		return Found ? Found->Get() : nullptr;
	}
	return nullptr;
}

bool UMosesPerfTestSubsystem::TryMoveLocalPlayerToMarker(APlayerController* PC, FName MarkerId)
{
	if (!PC || !PC->IsLocalController())
	{
		return false;
	}

	AMosesPerfMarker* Marker = FindMarker(MarkerId);
	if (!Marker)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] FAIL Marker not found Id=%s"), *MarkerId.ToString());
		return false;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] FAIL No Pawn PC=%s"), *GetNameSafe(PC));
		return false;
	}

	const FTransform Target = Marker->GetMarkerTransform();

	Pawn->SetActorLocationAndRotation(Target.GetLocation(), Target.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][MOVE] OK Marker=%s Pawn=%s Loc=%s Rot=%s"),
		*MarkerId.ToString(),
		*GetNameSafe(Pawn),
		*Target.GetLocation().ToString(),
		*Target.Rotator().ToString());

	return true;
}

bool UMosesPerfTestSubsystem::TrySpawnZombies_ServerAuthority(int32 Count)
{
	if (!PerfSpawner)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][SPAWN] FAIL No PerfSpawner bound"));
		return false;
	}

	return PerfSpawner->Server_SpawnZombies(Count);
}

void UMosesPerfTestSubsystem::DumpPerfBindingState(const UObject* WorldContext) const
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][DUMP] World=%s NetMode=%d Spawner=%s Markers=%d Policy_AI=%s"),
		*GetNameSafe(World),
		(int32)NetMode,
		*GetNameSafe(PerfSpawner),
		Markers.Num(),
		(ActiveAIPolicyMode == EMosesPerfAIPolicyMode::On) ? TEXT("ON") : TEXT("OFF"));

	for (const TPair<FName, TObjectPtr<AMosesPerfMarker>>& Pair : Markers)
	{
		const AMosesPerfMarker* Marker = Pair.Value.Get();
		UE_LOG(LogMosesAuth, Warning,
			TEXT("[PERF][DUMP] MarkerId=%s Actor=%s Loc=%s"),
			*Pair.Key.ToString(),
			*GetNameSafe(Marker),
			Marker ? *Marker->GetActorLocation().ToString() : TEXT("None"));
	}
}

bool UMosesPerfTestSubsystem::TryGetMarkerTransform(FName MarkerId, FTransform& OutTransform) const
{
	const AMosesPerfMarker* Marker = FindMarker(MarkerId);
	if (!Marker)
	{
		return false;
	}

	OutTransform = Marker->GetMarkerTransform();
	return true;
}

void UMosesPerfTestSubsystem::BeginMeasure(const UObject* WorldContext, FName MeasureId, FName MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;

	ActiveMeasure.MeasureId = MeasureId;
	ActiveMeasure.MarkerId = MarkerId;
	ActiveMeasure.SpawnCount = SpawnCount;
	ActiveMeasure.TrialIndex = TrialIndex;
	ActiveMeasure.TrialTotal = TrialTotal;
	ActiveMeasure.DurationSec = DurationSec;
	ActiveMeasure.BeginSeconds = World ? World->GetTimeSeconds() : 0.0;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][MEASURE] Begin Id=%s Marker=%s Count=%d Trial=%d/%d Duration=%.1fs Policy_AI=%s World=%s NetMode=%d"),
		*MeasureId.ToString(),
		*MarkerId.ToString(),
		SpawnCount,
		TrialIndex,
		TrialTotal,
		DurationSec,
		(ActiveAIPolicyMode == EMosesPerfAIPolicyMode::On) ? TEXT("ON") : TEXT("OFF"),
		*GetNameSafe(World),
		World ? (int32)World->GetNetMode() : -1);
}

void UMosesPerfTestSubsystem::EndMeasure(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;

	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const double Elapsed = Now - ActiveMeasure.BeginSeconds;

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][MEASURE] End Id=%s Elapsed=%.2fs (Expected=%.1fs) Marker=%s Count=%d Trial=%d/%d Policy_AI=%s"),
		*ActiveMeasure.MeasureId.ToString(),
		Elapsed,
		ActiveMeasure.DurationSec,
		*ActiveMeasure.MarkerId.ToString(),
		ActiveMeasure.SpawnCount,
		ActiveMeasure.TrialIndex,
		ActiveMeasure.TrialTotal,
		(ActiveAIPolicyMode == EMosesPerfAIPolicyMode::On) ? TEXT("ON") : TEXT("OFF"));

	ActiveMeasure = FMosesPerfMeasureContext();
}
