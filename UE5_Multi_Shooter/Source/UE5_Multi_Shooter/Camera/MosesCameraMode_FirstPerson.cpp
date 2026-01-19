#include "UE5_Multi_Shooter/Camera/MosesCameraMode_FirstPerson.h"

UMosesCameraMode_FirstPerson::UMosesCameraMode_FirstPerson(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCameraMode_FirstPerson::UpdateView(float DeltaTime)
{
	const FVector PivotLoc = GetPivotLocation();
	FRotator PivotRot = GetPivotRotation();

	PivotRot.Pitch = FMath::ClampAngle(PivotRot.Pitch, ViewPitchMin, ViewPitchMax);

	// Pivot + 미세 오프셋
	View.Location = PivotLoc + PivotRot.RotateVector(DefaultEyeOffset);
	View.Rotation = PivotRot;
	View.ControlRotation = PivotRot;
	View.FieldOfView = FieldOfView;
}
