#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPerfTypes.h"
#include "MosesPerfMarker.generated.h"

/**
 * AMosesPerfMarker
 *
 * PerfTestLevel에서 "절대 고정된 카메라 기준점"을 제공하는 Actor.
 *
 * - 서버/클라 공통 사용 (판정 없음, 기준점 용도)
 * - BeginPlay 시 초기 Transform을 저장
 * - 런타임 중 이동/회전 변경 감지 시 Warning 로그 1회 출력
 * - Tick 사용 금지 (저빈도 타이머 기반 검증)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfMarker : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfMarker();

	/** Marker 고유 ID (실험 스크립트/로그 기준점) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moses|Perf")
	EMosesPerfMarkerId MarkerId = EMosesPerfMarkerId::Marker01;

	/** 저장된 카메라 기준 Transform 반환 */
	UFUNCTION(BlueprintPure, Category = "Moses|Perf")
	FTransform GetMarkerTransform() const;

protected:
	virtual void BeginPlay() override;

private:
	/** BeginPlay 시 확정된 기준 Transform */
	UPROPERTY(Transient)
	FTransform SavedCameraTransform;

	/** 이동 감지 경고를 한 번만 출력하기 위한 플래그 */
	bool bMoveWarningIssued = false;

	/** 주기적으로 Marker 이동 여부를 검증 */
	void ValidateTransform_NotMoved();

	/** 비교 허용 오차 (미세한 FP 오차 방지) */
	static constexpr float LocationTolerance = 0.1f;
	static constexpr float RotationToleranceDeg = 0.1f;
};
