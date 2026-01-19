#include "UE5_Multi_Shooter/Camera/MosesCameraMode_ThirdPerson.h"

#include "Curves/CurveVector.h"

UMosesCameraMode_ThirdPerson::UMosesCameraMode_ThirdPerson(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCameraMode_ThirdPerson::UpdateView(float DeltaTime)
{
	// 1) Pivot(캐릭터 기준점) 확보
	const FVector PivotLoc = GetPivotLocation();
	FRotator PivotRot = GetPivotRotation();

	// 2) Pitch 제한
	PivotRot.Pitch = FMath::ClampAngle(PivotRot.Pitch, ViewPitchMin, ViewPitchMax);

	// 3) 오프셋 결정 (커브가 있으면 Pitch 기반으로 변형)
	FVector Offset = DefaultTargetOffset;
	if (TargetOffsetCurve)
	{
		Offset = TargetOffsetCurve->GetVectorValue(PivotRot.Pitch);
	}

	// 4) 회전된 오프셋을 적용해 최종 카메라 위치 계산
	View.Location = PivotLoc + PivotRot.RotateVector(Offset);
	View.Rotation = PivotRot;
	View.ControlRotation = PivotRot;
	View.FieldOfView = FieldOfView;
}
