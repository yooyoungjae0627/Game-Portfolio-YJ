// MosesPerfBootstrap.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	// ---------------------------------------------------------------------
	// Exec (Type in console)
	// ---------------------------------------------------------------------
	// Usage:
	//  - perf_dump
	//  - perf_marker 1
	//  - perf_spawn 100
	//  - perf_measure_begin DAY3_PHYSICS Marker01 200 1 3 30
	//  - perf_measure_end
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

private:
	void BindToSubsystem();

	FName GetMarkerIdByIndex(int32 MarkerIndex) const;

private:
	// Level references
	UPROPERTY(EditInstanceOnly, Category="Perf|Bootstrap")
	TObjectPtr<AMosesPerfSpawner> PerfSpawner;

	UPROPERTY(EditInstanceOnly, Category="Perf|Bootstrap")
	TObjectPtr<AMosesPerfMarker> Marker01;

	UPROPERTY(EditInstanceOnly, Category="Perf|Bootstrap")
	TObjectPtr<AMosesPerfMarker> Marker02;

	UPROPERTY(EditInstanceOnly, Category="Perf|Bootstrap")
	TObjectPtr<AMosesPerfMarker> Marker03;
};
