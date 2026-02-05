// MosesPerfTestSubsystem.cpp
#include "MosesPerfTestSubsystem.h"

#include "MosesPerfSpawner.h"
#include "MosesPerfMarker.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

bool UMosesPerfTestSubsystem::GuardWorld(const TCHAR* Caller) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][GUARD] %s denied: World=null"), Caller);
		return false;
	}
	return true;
}

void UMosesPerfTestSubsystem::Deinitialize()
{
	PerfSpawner.Reset();
	Markers.Reset();

	Super::Deinitialize();
}

void UMosesPerfTestSubsystem::SetPerfMode(EMosesPerfMode NewMode)
{
	if (!GuardWorld(TEXT("SetPerfMode")))
	{
		return;
	}

	const EMosesPerfMode Prev = CurrentPerfMode;
	CurrentPerfMode = NewMode;

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MODE] %d -> %d"), (int32)Prev, (int32)NewMode);

	// DAY1: “연결만”
	// DAY6+: 여기서 Render/Anim/Net 정책 Subsystem들에게 브로드캐스트하면 된다.
}

void UMosesPerfTestSubsystem::SetPerfSpawner(AMosesPerfSpawner* InSpawner)
{
	PerfSpawner = InSpawner;

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][BIND] PerfSpawner=%s"),
		InSpawner ? *InSpawner->GetName() : TEXT("null"));
}

void UMosesPerfTestSubsystem::RegisterMarker(AMosesPerfMarker* MarkerActor)
{
	if (!MarkerActor)
	{
		return;
	}

	Markers.AddUnique(MarkerActor);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MARKER] Registered Id=%d Name=%s"),
		(int32)MarkerActor->MarkerId, *MarkerActor->GetName());
}

void UMosesPerfTestSubsystem::ApplyScenario_Server(EMosesPerfScenarioId ScenarioId)
{
	if (!GuardWorld(TEXT("ApplyScenario_Server")))
	{
		return;
	}

	AMosesPerfSpawner* Spawner = PerfSpawner.Get();
	if (!Spawner)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][SCENARIO] No PerfSpawner bound"));
		return;
	}

	Spawner->ApplyScenario_Server(ScenarioId);
}

void UMosesPerfTestSubsystem::ResetScenario_Server()
{
	if (!GuardWorld(TEXT("ResetScenario_Server")))
	{
		return;
	}

	AMosesPerfSpawner* Spawner = PerfSpawner.Get();
	if (!Spawner)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][RESET] No PerfSpawner bound"));
		return;
	}

	Spawner->PerfReset_Server();
}

AMosesPerfMarker* UMosesPerfTestSubsystem::FindMarker(EMosesPerfMarkerId MarkerId) const
{
	for (const TWeakObjectPtr<AMosesPerfMarker>& Weak : Markers)
	{
		if (AMosesPerfMarker* Marker = Weak.Get())
		{
			if (Marker->MarkerId == MarkerId)
			{
				return Marker;
			}
		}
	}
	return nullptr;
}

void UMosesPerfTestSubsystem::TeleportLocalCameraToMarker(EMosesPerfMarkerId MarkerId)
{
	if (!GuardWorld(TEXT("TeleportLocalCameraToMarker")))
	{
		return;
	}

	AMosesPerfMarker* Marker = FindMarker(MarkerId);
	if (!Marker)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MARKER] NotFound Id=%d"), (int32)MarkerId);
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MARKER] PC=null"));
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MARKER] Pawn=null (spectator?)"));
		// Pawn이 없으면 ViewTarget만 옮겨도 됨. 여기선 단순화:
		PC->SetControlRotation(Marker->GetActorRotation());
		return;
	}

	Pawn->TeleportTo(Marker->GetActorLocation(), Marker->GetActorRotation());
	PC->SetControlRotation(Marker->GetActorRotation());

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MARKER] Teleport Id=%d Loc=%s Rot=%s"),
		(int32)MarkerId,
		*Marker->GetActorLocation().ToString(),
		*Marker->GetActorRotation().ToString());
}
