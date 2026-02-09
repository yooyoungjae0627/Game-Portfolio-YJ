#include "UE5_Multi_Shooter/Perf/MosesPerfCheatManager.h"
#include "UE5_Multi_Shooter/MosesPlayerController.h" 
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfTestSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

static UMosesPerfTestSubsystem* MosesPerf_GetSubsys(UCheatManager* CM, APlayerController*& OutPC)
{
	OutPC = CM ? CM->GetOuterAPlayerController() : nullptr;
	if (!OutPC)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][CMD] FAIL NoOuterPC"));
		return nullptr;
	}

	UGameInstance* GI = OutPC->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][CMD] FAIL NoGameInstance PC=%s"), *GetNameSafe(OutPC));
		return nullptr;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][CMD] FAIL NoPerfTestSubsystem"));
		return nullptr;
	}

	return Subsys;
}

void UMosesPerfCheatManager::perf_dump()
{
	APlayerController* PC = nullptr;
	if (UMosesPerfTestSubsystem* Subsys = MosesPerf_GetSubsys(this, PC))
	{
		Subsys->DumpPerfBindingState(PC);
	}
}

void UMosesPerfCheatManager::perf_marker(int32 MarkerIndex)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] FAIL NoOuterPC"));
		return;
	}

	AMosesPlayerController* MPC = Cast<AMosesPlayerController>(PC);
	if (!MPC)
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] FAIL PC not AMosesPlayerController PC=%s"), *GetNameSafe(PC));
		return;
	}

	FName MarkerId = NAME_None;
	switch (MarkerIndex)
	{
	case 1: MarkerId = FName(TEXT("Marker01")); break;
	case 2: MarkerId = FName(TEXT("Marker02")); break;
	case 3: MarkerId = FName(TEXT("Marker03")); break;
	default:
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] FAIL InvalidMarkerIndex=%d"), MarkerIndex);
		return;
	}

	UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE] REQ Marker=%s PC=%s"), *MarkerId.ToString(), *GetNameSafe(PC));

	// ✅ 서버로 요청 (서버 권위 이동)
	MPC->Server_PerfMarker(MarkerId);
}

void UMosesPerfCheatManager::perf_spawn(int32 Count)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		return;
	}

	AMosesPlayerController* MPC = Cast<AMosesPlayerController>(PC);
	if (!MPC)
	{
		return;
	}

	UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][SPAWN] REQ Count=%d PC=%s"), Count, *GetNameSafe(PC));

	// ✅ 서버로 요청
	MPC->Server_PerfSpawn(Count);
}

void UMosesPerfCheatManager::perf_measure_begin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec)
{
	APlayerController* PC = nullptr;
	if (UMosesPerfTestSubsystem* Subsys = MosesPerf_GetSubsys(this, PC))
	{
		Subsys->BeginMeasure(PC, FName(*MeasureId), FName(*MarkerId), SpawnCount, TrialIndex, TrialTotal, DurationSec);
	}
}

void UMosesPerfCheatManager::perf_measure_end()
{
	APlayerController* PC = nullptr;
	if (UMosesPerfTestSubsystem* Subsys = MosesPerf_GetSubsys(this, PC))
	{
		Subsys->EndMeasure(PC);
	}
}
