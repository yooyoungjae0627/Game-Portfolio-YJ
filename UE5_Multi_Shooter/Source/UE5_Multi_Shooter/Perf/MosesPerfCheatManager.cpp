#include "UE5_Multi_Shooter/Perf/MosesPerfCheatManager.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerController.h"

void UMosesPerfCheatManager::perf_dump()
{
	if (AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOuterAPlayerController()))
	{
		PC->Perf_Dump();
	}
}

void UMosesPerfCheatManager::perf_marker(int32 MarkerIndex)
{
	if (AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOuterAPlayerController()))
	{
		PC->Perf_Marker(MarkerIndex);
	}
}

void UMosesPerfCheatManager::perf_spawn(int32 Count)
{
	if (AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOuterAPlayerController()))
	{
		PC->Perf_Spawn(Count);
	}
}

void UMosesPerfCheatManager::perf_measure_begin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec)
{
	if (AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOuterAPlayerController()))
	{
		PC->Perf_MeasureBegin(MeasureId, MarkerId, SpawnCount, TrialIndex, TrialTotal, DurationSec);
	}
}

void UMosesPerfCheatManager::perf_measure_end()
{
	if (AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOuterAPlayerController()))
	{
		PC->Perf_MeasureEnd();
	}
}
