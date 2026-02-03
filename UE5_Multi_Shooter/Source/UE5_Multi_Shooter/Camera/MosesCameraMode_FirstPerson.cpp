#include "UE5_Multi_Shooter/Camera/MosesCameraMode_FirstPerson.h"

UMosesCameraMode_FirstPerson::UMosesCameraMode_FirstPerson(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCameraMode_FirstPerson::UpdateView(float DeltaTime)
{
	const FVector PivotLoc = GetPivotLocation();
	const FRotator PivotRotRaw = GetPivotRotation();

	// [MOD][FIX] 안전 Pitch Clamp(프로젝트 최소 보장)
	const FRotator PivotRot = ClampPivotRotationPitch_Safe(PivotRotRaw);

	View.Location = PivotLoc + PivotRot.RotateVector(DefaultEyeOffset);
	View.Rotation = PivotRot;
	View.ControlRotation = PivotRot;
	View.FieldOfView = FieldOfView;
}
