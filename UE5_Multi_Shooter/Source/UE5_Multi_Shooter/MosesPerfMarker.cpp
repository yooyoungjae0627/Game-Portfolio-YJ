#include "MosesPerfMarker.h"

#include "TimerManager.h"
#include "Engine/World.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

AMosesPerfMarker::AMosesPerfMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetActorEnableCollision(false);

	// PerfMarker는 "존재 + 위치"만 의미 있음
	SetReplicates(true);
}

void AMosesPerfMarker::BeginPlay()
{
	Super::BeginPlay();

	// ---------------------------------------------------------------------
	// 기준 Transform 확정
	// ---------------------------------------------------------------------
	SavedCameraTransform = GetActorTransform();

	UE_LOG(
		LogMosesPerf,
		Display,
		TEXT("[PERF][MARKER] BeginPlay Marker=%s Id=%d SavedTransform=(Loc=%s Rot=%s)"),
		*GetName(),
		static_cast<int32>(MarkerId),
		*SavedCameraTransform.GetLocation().ToString(),
		*SavedCameraTransform.GetRotation().Rotator().ToString()
	);

	// ---------------------------------------------------------------------
	// 이동 감지 검증 (저빈도 타이머)
	// - Tick 금지
	// - 실험 중 실수 방지용
	// ---------------------------------------------------------------------
	if (UWorld* World = GetWorld())
	{
		FTimerHandle ValidateTimer;
		World->GetTimerManager().SetTimer(
			ValidateTimer,
			this,
			&AMosesPerfMarker::ValidateTransform_NotMoved,
			1.0f,   // 1초 간격
			true
		);
	}
}

FTransform AMosesPerfMarker::GetMarkerTransform() const
{
	return SavedCameraTransform;
}

void AMosesPerfMarker::ValidateTransform_NotMoved()
{
	if (bMoveWarningIssued)
	{
		return;
	}

	const FTransform Current = GetActorTransform();

	const bool bLocationChanged =
		!Current.GetLocation().Equals(SavedCameraTransform.GetLocation(), LocationTolerance);

	const bool bRotationChanged =
		!Current.GetRotation().Rotator().Equals(
			SavedCameraTransform.GetRotation().Rotator(),
			RotationToleranceDeg
		);

	if (bLocationChanged || bRotationChanged)
	{
		bMoveWarningIssued = true;

		UE_LOG(
			LogMosesPerf,
			Warning,
			TEXT("[PERF][MARKER][INVALID] Marker=%s Id=%d MOVED! Initial=(Loc=%s Rot=%s) Current=(Loc=%s Rot=%s) -> Perf Result INVALID"),
			*GetName(),
			static_cast<int32>(MarkerId),
			*SavedCameraTransform.GetLocation().ToString(),
			*SavedCameraTransform.GetRotation().Rotator().ToString(),
			*Current.GetLocation().ToString(),
			*Current.GetRotation().Rotator().ToString()
		);
	}
}
