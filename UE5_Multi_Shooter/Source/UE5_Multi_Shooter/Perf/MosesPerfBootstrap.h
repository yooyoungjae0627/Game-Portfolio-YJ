#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfTestSubsystem.h"

#include "MosesPerfBootstrap.generated.h"

class AMosesPerfSpawner;
class AMosesPerfMarker;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfBootstrap : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfBootstrap();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Exec) void perf_dump();
	UFUNCTION(Exec) void perf_marker(int32 MarkerIndex);
	UFUNCTION(Exec) void perf_spawn(int32 Count);
	UFUNCTION(Exec) void perf_measure_begin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec);
	UFUNCTION(Exec) void perf_measure_end();

	UFUNCTION(Exec) void perf_ai_on();
	UFUNCTION(Exec) void perf_ai_off();

private:
	void BindToSubsystem();
	FName GetMarkerIdByIndex(int32 MarkerIndex) const;

private:
	UPROPERTY(EditInstanceOnly, Category = "Perf|Bootstrap")
	TObjectPtr<AMosesPerfSpawner> PerfSpawner;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Bootstrap")
	TObjectPtr<AMosesPerfMarker> Marker01;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Bootstrap")
	TObjectPtr<AMosesPerfMarker> Marker02;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Bootstrap")
	TObjectPtr<AMosesPerfMarker> Marker03;

	// ✅ 전/후 라벨 (SSOT에 전달할 값)
	UPROPERTY(EditInstanceOnly, Category = "Perf|Policy")
	EMosesPerfAIPolicyMode StartupAIPolicyMode = EMosesPerfAIPolicyMode::Off;
};
