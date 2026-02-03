#include "UE5_Multi_Shooter/Camera/MosesCameraMode_ThirdPerson.h"

#include "Curves/CurveVector.h"

UMosesCameraMode_ThirdPerson::UMosesCameraMode_ThirdPerson(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCameraMode_ThirdPerson::UpdateView(float DeltaTime)
{
	const FVector PivotLoc = GetPivotLocation();
	const FRotator PivotRotRaw = GetPivotRotation();

	// [MOD][FIX] ¾ÈÀü Pitch Clamp
	const FRotator PivotRot = ClampPivotRotationPitch_Safe(PivotRotRaw);

	FVector Offset = DefaultTargetOffset;
	if (TargetOffsetCurve)
	{
		Offset = TargetOffsetCurve->GetVectorValue(PivotRot.Pitch);
	}

	View.Location = PivotLoc + PivotRot.RotateVector(Offset);
	View.Rotation = PivotRot;
	View.ControlRotation = PivotRot;
	View.FieldOfView = FieldOfView;
}
