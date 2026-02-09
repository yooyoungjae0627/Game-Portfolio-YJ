#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "MosesPerfCheatManager.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesPerfCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void perf_dump();

	UFUNCTION(Exec)
	void perf_marker(int32 MarkerIndex);

	UFUNCTION(Exec)
	void perf_spawn(int32 Count);

	UFUNCTION(Exec)
	void perf_measure_begin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec);

	UFUNCTION(Exec)
	void perf_measure_end();
};
