// ============================================================================
// UE5_Multi_Shooter/Perf/MosesPerfBootstrap.h  (FULL - NEW)
// ============================================================================
// - PerfTestLevel에 1개 배치용
// - BeginPlay에서 PerfSubsystem에 Spawner/Marker를 "순서 고정"으로 등록
// - Evidence-First 로그 필수
// ============================================================================

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

private:
	bool GuardWorld(const TCHAR* Caller) const;

private:
	// ---------------------------------------------------------------------
	// [MOD] Level Instance Binding (Details에서 고정 연결)
	// ---------------------------------------------------------------------
	UPROPERTY(EditInstanceOnly, Category="Moses|Perf|Bootstrap")
	TObjectPtr<AMosesPerfSpawner> SpawnerRef = nullptr;

	UPROPERTY(EditInstanceOnly, Category="Moses|Perf|Bootstrap")
	TArray<TObjectPtr<AMosesPerfMarker>> MarkerRefs;
};
