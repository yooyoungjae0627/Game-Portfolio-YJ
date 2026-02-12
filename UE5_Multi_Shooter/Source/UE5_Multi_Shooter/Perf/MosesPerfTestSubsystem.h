#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MosesPerfTestSubsystem.generated.h"

class AMosesPerfSpawner;
class AMosesPerfMarker;
class APlayerController;

UENUM(BlueprintType)
enum class EMosesPerfAIPolicyMode : uint8
{
	Off UMETA(DisplayName = "AI OFF"),
	On  UMETA(DisplayName = "AI ON"),
};

USTRUCT()
struct FMosesPerfMeasureContext
{
	GENERATED_BODY()

	UPROPERTY()
	FName MeasureId = NAME_None;

	UPROPERTY()
	FName MarkerId = NAME_None;

	UPROPERTY()
	int32 SpawnCount = 0;

	UPROPERTY()
	int32 TrialIndex = 0;

	UPROPERTY()
	int32 TrialTotal = 3;

	UPROPERTY()
	float DurationSec = 30.0f;

	UPROPERTY()
	double BeginSeconds = 0.0;
};

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesPerfTestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMosesPerfTestSubsystem();

	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------------
	// Binding (Level -> Subsystem)
	// ---------------------------------------------------------------------
	void SetPerfSpawner(AMosesPerfSpawner* InSpawner);
	void RegisterMarker(FName MarkerId, AMosesPerfMarker* InMarker);

	bool IsReady() const;

	// ---------------------------------------------------------------------
	// Policy (Before/After label SSOT)
	// ---------------------------------------------------------------------
	void SetAIPolicyMode(EMosesPerfAIPolicyMode NewMode);
	EMosesPerfAIPolicyMode GetAIPolicyMode() const { return ActiveAIPolicyMode; }

	// ---------------------------------------------------------------------
	// Runtime (Measurement helpers)
	// ---------------------------------------------------------------------
	bool TryMoveLocalPlayerToMarker(APlayerController* PC, FName MarkerId);
	bool TrySpawnZombies_ServerAuthority(int32 Count);

	void DumpPerfBindingState(const UObject* WorldContext) const;

	// ---------------------------------------------------------------------
	// Server-side helper (avoid exposing AMosesPerfMarker in PC code)
	// ---------------------------------------------------------------------
	bool TryGetMarkerTransform(FName MarkerId, FTransform& OutTransform) const;

	// ---------------------------------------------------------------------
	// Measurement context (Evidence-First)
	// ---------------------------------------------------------------------
	void BeginMeasure(const UObject* WorldContext, FName MeasureId, FName MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec);
	void EndMeasure(const UObject* WorldContext);

private:
	AMosesPerfMarker* FindMarker(FName MarkerId) const;

private:
	UPROPERTY()
	TObjectPtr<AMosesPerfSpawner> PerfSpawner;

	UPROPERTY()
	TMap<FName, TObjectPtr<AMosesPerfMarker>> Markers;

	UPROPERTY()
	FMosesPerfMeasureContext ActiveMeasure;

	UPROPERTY()
	EMosesPerfAIPolicyMode ActiveAIPolicyMode = EMosesPerfAIPolicyMode::Off;
};
