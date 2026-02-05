// MosesPerfTestSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MosesPerfTypes.h"
#include "MosesPerfTestSubsystem.generated.h"

class AMosesPerfSpawner;
class AMosesPerfMarker;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesPerfTestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---------- PerfMode ----------
	UFUNCTION(BlueprintCallable, Category="Moses|Perf")
	void SetPerfMode(EMosesPerfMode NewMode);

	UFUNCTION(BlueprintPure, Category="Moses|Perf")
	EMosesPerfMode GetPerfMode() const { return CurrentPerfMode; }

	// ---------- Scenario ----------
	UFUNCTION(BlueprintCallable, Category="Moses|Perf")
	void ApplyScenario_Server(EMosesPerfScenarioId ScenarioId);

	UFUNCTION(BlueprintCallable, Category="Moses|Perf")
	void ResetScenario_Server();

	// ---------- Marker ----------
	UFUNCTION(BlueprintCallable, Category="Moses|Perf")
	void TeleportLocalCameraToMarker(EMosesPerfMarkerId MarkerId);

	// ---------- Spawner binding ----------
	UFUNCTION(BlueprintCallable, Category="Moses|Perf")
	void SetPerfSpawner(AMosesPerfSpawner* InSpawner);

	// ---------- Marker registry ----------
	UFUNCTION(BlueprintCallable, Category="Moses|Perf")
	void RegisterMarker(AMosesPerfMarker* MarkerActor);

protected:
	virtual void Deinitialize() override;

private:
	bool GuardWorld(const TCHAR* Caller) const;

	AMosesPerfMarker* FindMarker(EMosesPerfMarkerId MarkerId) const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AMosesPerfSpawner> PerfSpawner;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AMosesPerfMarker>> Markers;

	EMosesPerfMode CurrentPerfMode = EMosesPerfMode::Mode0;
};
